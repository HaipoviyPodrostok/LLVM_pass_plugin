#include <cstdint>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Plugins/PassPlugin.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Demangle/Demangle.h>

#include <map>
#include <sys/types.h>
#include <system_error>

#include "def_use_graph.hpp"

using namespace llvm;

namespace {

inline const std::string dotFile     = "assets/dot_files/without_instrumentation.dot";
inline const std::string nodeStyle   = "\", shape=record, style=filled, fillcolor=lightgrey];\n";
inline const std::string defUseStyle = " [color=red, fontcolor=red, label=\"user\"];\n";
inline const std::string cfgStyle    = " [color=black, style=bold];\n";

class DefUseGraph {
 public:
  explicit DefUseGraph(Module& M) : M(M) {}
  
  void buildAndSafe() {
    indexation();
    makeDotFile();
    addInstrumentation();
  }
  
 private:
  Module& M;
  std::map<Value*, uint64_t> IDmap;

  void indexation();
  void makeDotFile();
  void addInstrumentation();

  std::string escapeLabel(const std::string& raw);
};

void DefUseGraph::indexation() {
  uint64_t NextID = 1;
  
  for (GlobalVariable& GV : M.globals()) {
    IDmap[&GV] = NextID;
    NextID++;
  }

  for (Function& F : M) {
    if (F.isDeclaration()) { continue; }
   
    for (Argument& A : F.args()) {
      IDmap[&A] = NextID;
      NextID++;
    }

    for (BasicBlock& BB : F) {
      for (Instruction& I : BB) {
        IDmap[&I] = NextID;
        NextID++;
      }
    }
  }
}

std::string DefUseGraph::escapeLabel(const std::string& raw) {
  std::string result;
  
  for (char c : raw) {
    if (c == '"') { result += "\\\""; }
    else if (c == '<' || c == '>' || c == '{' || c == '}' || c == '|') {
      result += '\\';
      result += c;
    }
    else result += c;
  }
  
  return result;
}

void DefUseGraph::makeDotFile() {
  std::error_code EC;

  raw_fd_ostream File(dotFile, EC, sys::fs::OF_Text);
  if (EC) {
    errs() << "File open error: " << EC.message() << "\n";
    return;
  }
  
  File << "digraph DefUseGraph {\n";
  File << "  compound=true;\n";

  uint64_t clusterID = 0;

  for (Function& F : M) {
    if (F.isDeclaration()) { continue; }

    if (F.arg_size() > 0) {
      File << "  subgraph cluster_" << clusterID++ << " {\n";
      File << "    label=\"" << llvm::demangle(F.getName().str()) << " args\";\n";
      File << "    style=dashed;\n";

      for (Argument& A : F.args()) {
        std::string label;
        raw_string_ostream OS(label);
        A.print(OS, true);

        File << "    " << IDmap[&A] << " [label=\"" << escapeLabel(label) << nodeStyle;
      }
      File << "  }\n";
    }

    for (BasicBlock& BB : F) {
      File << "  subgraph cluster_" << clusterID++ << " {\n";
      
      std::string bbName = BB.hasName() ? BB.getName().str() : "unnamed";

      File << "    label=\"" << llvm::demangle(F.getName().str()) << ": ";
      if (BB.hasName()) { File << BB.getName(); }
      File << "\";\n";
      File << "    style=solid;\n";

      for (Instruction& I : BB) {
        std::string label;

        if (I.getType()->isVoidTy()) {
          raw_string_ostream OS(label);
          I.print(OS, true);
        } 
        else {
          raw_string_ostream OS(label);
          I.print(OS, true);
        }

        File << "    " << IDmap[&I] << " [label=\"" << escapeLabel(label) 
             << nodeStyle;
      }
      File << "  }\n";
    }
  }

  for (GlobalVariable& GV : M.globals()) {
    std::string label;
    raw_string_ostream OS(label);
    GV.printAsOperand(OS, false, &M);

    File << "  " << IDmap[&GV] << " [label=\"" << escapeLabel(label) << nodeStyle;
  }


  for (Function& F : M) {
    if (F.isDeclaration()) { continue; }

    for (BasicBlock& BB : F) {

      Instruction* Prev = nullptr;
      for (Instruction& I : BB) {
        if (Prev != nullptr) {
          File << "  " << IDmap[Prev] << " -> " << IDmap[&I] << cfgStyle;
        }
        Prev = &I;
      }

      Instruction* Term = BB.getTerminator();
      if (Term == nullptr) { continue; }

      for (unsigned i = 0; i < Term->getNumSuccessors(); ++i) {
        BasicBlock* Succ = Term->getSuccessor(i);
        Instruction& FirstInst = Succ->front();

        File << "  " << IDmap[Term] << " -> " << IDmap[&FirstInst] << cfgStyle;
      }
    }
  }


  for (Function& F : M) {
    for (BasicBlock& BB : F) {
      for (Instruction& I : BB) {
        const uint64_t CurrentID = IDmap[&I];

        for (Use& U : I.operands()) {
          Value* Op = U.get();

          if (IDmap.count(Op)) {
            const uint64_t OpID = IDmap[Op];
            File << "  " << OpID << " -> " << CurrentID << defUseStyle;
          }
        }
      }
    }
  }
  
  File << "}\n";
}

void DefUseGraph::addInstrumentation() {
  LLVMContext&   Context   = M.getContext();
  Type*          Int64Type = Type::getInt64Ty(Context);
  FunctionCallee LogFunc   = M.getOrInsertFunction("__log_value", Type::getVoidTy(Context),
                                  Int64Type, Int64Type);

  for (Function& F : M) {
    if (F.isDeclaration()) continue;

    IRBuilder<> Builder(&*F.getEntryBlock().getFirstInsertionPt());
    
    for (Argument& A : F.args()) {
      const uint64_t ArgID = IDmap[&A];
      
      Value* IDConst = Builder.getInt64(ArgID);
      Value* CastedVal;
      
      if (A.getType()->isPointerTy()) {
        CastedVal = Builder.CreatePtrToInt(&A, Int64Type);
      } else {
        CastedVal = Builder.CreateZExtOrBitCast(&A, Int64Type);
      }
      
      Builder.CreateCall(LogFunc, {IDConst, CastedVal});
    }

    for (BasicBlock& BB : F) {
      for (Instruction& I : BB) {

        if (I.getType()->isVoidTy() || I.isTerminator() || isa<PHINode>(&I)) {
          continue;
        }

        if (IDmap.find(&I) == IDmap.end()) {
          continue;
        }

        IRBuilder<> Builder(I.getNextNode());
        
        const uint64_t InstID  = IDmap[&I];
        Value*         IDConst = Builder.getInt64(InstID);
        
        Value* CastedVal;

        if (I.getType()->isPointerTy()) {
          CastedVal = Builder.CreatePtrToInt(&I, Int64Type);
        } else {
          CastedVal = Builder.CreateZExtOrBitCast(&I, Int64Type);
        }

        Builder.CreateCall(LogFunc, {IDConst, CastedVal});
      }
    }
  }                          
}
} // namespace

PreservedAnalyses DefUseGraphPass::run(Module& M, ModuleAnalysisManager& ) {
  DefUseGraph graph{M};
  graph.buildAndSafe();

  return PreservedAnalyses::all();
}

extern "C" ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {LLVM_PLUGIN_API_VERSION, "DefUseGraphPass", LLVM_VERSION_STRING,
          [](PassBuilder &PB) {
            PB.registerPipelineParsingCallback(
                [](StringRef Name, ModulePassManager& MPM,
                   ArrayRef<PassBuilder::PipelineElement>) {
                    if (Name == "def-use-graph") {
                      MPM.addPass(DefUseGraphPass());
                      return true;
                    }
                    return false;
                }
            );
          }
        };
}