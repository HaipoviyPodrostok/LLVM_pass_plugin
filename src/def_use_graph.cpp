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
#include <sys/types.h> // FIXME[flops]: Potentially unused
#include <system_error>

#include "def_use_graph.hpp"

using namespace llvm;

namespace {

// FIXME[flops]: Naming differs, pick one convention: dotFile -> DotFile, 

// FIXME[flops]: Use constexpr std::string_view there instead, it's cheaper
inline const std::string dotFile     = "assets/dot_files/without_instrumentation.dot";
inline const std::string nodeStyle   = "\", shape=record, style=filled, fillcolor=lightgrey];\n";
inline const std::string defUseStyle = " [color=red, fontcolor=red, label=\"user\"];\n";
inline const std::string cfgStyle    = " [color=black, style=bold];\n";

class DefUseGraph {
 public:
  explicit DefUseGraph(Module& M) : M(M) {}
  
  void buildAndSafe() { // FIXME[flops]: buildAndSave*
    indexation(); // FIXME[flops]: better rename it makeIndexing, indexUnits e.t.c.
    makeDotFile();
    addInstrumentation();
  }
  
 private:
  Module& M;
  std::map<Value*, uint64_t> IDmap; // FIXME[flops]: You don't need ordering there, so you can pick another container for those purposes: std::unordered_map / llvm::DenseMap e.t.c. std:: API is better, because LLVM API is unstable and it can change from version to version

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
        IDmap[&A] = NextID; // [flops]:  IDmap[&I] = NextID++; <---|
        NextID++; // [flops]: You can make one liner there --------|
    }

    for (BasicBlock& BB : F) {
      for (Instruction& I : BB) {
        IDmap[&I] = NextID; // [flops]:  IDmap[&I] = NextID++; <---|
        NextID++; // [flops]: You can make one liner there --------|
      }
    }
  }
}

std::string DefUseGraph::escapeLabel(const std::string& raw) { // TODO[flops]: Mark as static and const (does not depend on object state and doesn't change it too)
                                                               // FIXME[flops]: std::string_view
                                                               // [flops]: Also you can check complete solutions for this: llvm::raw_ostream::write_escaped, `llvm/Support/GraphWriter.h`
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
  
  File << "digraph DefUseGraph {\n"; // FIXME[flops]: Move all dot dump functionality in separate methods
  File << "  compound=true;\n";

  uint64_t clusterID = 0;

  for (Function& F : M) {
    if (F.isDeclaration()) { continue; }

    if (F.arg_size() > 0) { // FIXME[flops]: Separate method for args dumping
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
      
      std::string bbName = BB.hasName() ? BB.getName().str() : "unnamed"; // FIXME[flops]: Unused value!

      File << "    label=\"" << llvm::demangle(F.getName().str()) << ": ";
      if (BB.hasName()) { File << BB.getName(); }
      File << "\";\n";
      File << "    style=solid;\n";

      for (Instruction& I : BB) {
        std::string label;

        if (I.getType()->isVoidTy()) { // FIXME[flops]: You are doing exactly the same thing in both conditions
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

      for (unsigned i = 0; i < Term->getNumSuccessors(); ++i) { // TODO[flops]: You can iterate through successors directly: for (auto *Succ: successors(Term))
        BasicBlock* Succ = Term->getSuccessor(i);
        Instruction& FirstInst = Succ->front(); // [flops]: Better use Succ->getFirst<*>Inst

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

        if (IDmap.find(&I) == IDmap.end()) { // <--------------------------|
          continue; //                                                       |
        } //                                                                 |
        //                                                                   |
        IRBuilder<> Builder(I.getNextNode()); //                          |
        //                                                                   |
        const uint64_t InstID  = IDmap[&I]; // Just save it from find there -|
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

  return PreservedAnalyses::all(); // There are cases when you instrument nothing, so it should return `PreservedAnalyses::none();`, otherwise return `PreservedAnalyses::all();`
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