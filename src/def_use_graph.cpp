#include "def_use_graph.hpp"

#include <llvm/Demangle/Demangle.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Module.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Plugins/PassPlugin.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/GraphWriter.h>
#include <llvm/Support/raw_ostream.h>

#include <cstdint>
#include <system_error>
#include <unordered_map>

using namespace llvm;

namespace {

// FIXME[flops]: Naming differs, pick one convention: dotFile -> DotFile,

inline constexpr std::string_view dotFile =
  "assets/dot_files/without_instrumentation.dot";
inline constexpr std::string_view nodeStyle =
  "\", shape=record, style=filled, fillcolor=lightgrey];\n";
inline constexpr std::string_view defUseStyle =
  " [color=red, fontcolor=red, label=\"user\"];\n";
inline constexpr std::string_view cfgStyle = " [color=black, style=bold];\n";

class DefUseGraph {
 public:
  explicit DefUseGraph(Module& M): M(M) {}

  void buildAndSave() {
    indexUnits();
    makeDotFile();
    addInstrumentation();
  }

 private:
  // clang-format off
  Module& M;
  std::unordered_map<Value*, uint64_t> IDmap;
  // clang-format on

  void indexUnits();
  void makeDotFile();
  void addInstrumentation();
};

class DotDumper {
 public:
  DotDumper(raw_fd_ostream& OS, std::unordered_map<Value*, uint64_t>& IDmap)
    : OS(OS), IDmap(IDmap), clusterID(0) {}

  void dump(Module& M);

 private:
  // clang-format off
  raw_fd_ostream& OS;
  uint64_t        clusterID;
  const std::unordered_map<Value*, uint64_t>& IDmap;
  // clang-format on

  void writeDotHeader() const;
  void dumpArgs(Function& F);
  void dumpBBs(Function& F);
  void dumpGlobals(Module& M);
  void dumpCFGEdges(Module& M);
  void dumpDefUseEdges(Module& M);
  void writeDotEnd() const { OS << "}\n"; }
};

void DefUseGraph::indexUnits() {
  uint64_t NextID = 1;

  for (GlobalVariable& GV : M.globals()) {
    IDmap[&GV] = NextID++;
  }

  for (Function& F : M) {
    if (F.isDeclaration()) {
      continue;
    }

    for (Argument& A : F.args()) {
      IDmap[&A] = NextID++;
    }

    for (BasicBlock& BB : F) {
      for (Instruction& I : BB) {
        IDmap[&I] = NextID++;
      }
    }
  }
}

void DefUseGraph::makeDotFile() {
  std::error_code EC;

  raw_fd_ostream File(dotFile, EC, sys::fs::OF_Text);
  if (EC) {
    errs() << "File open error: " << EC.message() << "\n";
    return;
  }

  DotDumper dumper {File, IDmap};
  dumper.dump(M);
}

void DotDumper::dump(Module& M) {
  writeDotHeader();

  for (llvm::Function& F : M) {
    if (!F.isDeclaration()) {
      if (F.arg_size() > 0) {
        dumpArgs(F);
      }
      dumpBBs(F);
    }
  }

  dumpGlobals(M);
  dumpCFGEdges(M);
  dumpDefUseEdges(M);

  writeDotEnd();
}

void DotDumper::writeDotHeader() const {
  OS << "digraph DefUseGraph {\n";
  OS << "  compound=true;\n";
}

void DotDumper::dumpArgs(Function& F) {
  OS << "  subgraph cluster_" << clusterID++ << " {\n";
  OS << "    label=\"" << llvm::demangle(F.getName().str()) << " args\";\n";
  OS << "    style=dashed;\n";

  for (Argument& A : F.args()) {
    std::string        label;
    raw_string_ostream LabelOS(label);
    A.print(LabelOS, true);

    OS << "    " << IDmap.at(&A) << " [label=\"" << llvm::DOT::EscapeString(label)
       << nodeStyle;
  }
  OS << "  }\n";
}

void DotDumper::dumpBBs(Function& F) {
  for (BasicBlock& BB : F) {
    OS << "  subgraph cluster_" << clusterID++ << " {\n";
    OS << "    label=\"" << llvm::demangle(F.getName().str()) << ": ";
    OS << BB.getName();  // TODO был if
    OS << "\";\n";
    OS << "    style=solid;\n";

    for (Instruction& I : BB) {
      std::string label;

      raw_string_ostream OS(label);
      I.print(OS, true);

      OS << "    " << IDmap.at(&I) << " [label=\"" << llvm::DOT::EscapeString(label)
         << nodeStyle;
    }
    OS << "  }\n";
  }
}

void DotDumper::dumpGlobals(Module& M) {
  for (GlobalVariable& GV : M.globals()) {
    std::string        label;
    raw_string_ostream OS(label);
    GV.printAsOperand(OS, false, &M);

    OS << "  " << IDmap.at(&GV) << " [label=\"" << llvm::DOT::EscapeString(label)
       << nodeStyle;
  }
}

void DotDumper::dumpCFGEdges(Module& M) {
  for (Function& F : M) {
    if (F.isDeclaration()) {
      continue;
    }

    for (BasicBlock& BB : F) {
      Instruction* Prev = nullptr;

      for (Instruction& I : BB) {
        if (Prev != nullptr) {
          OS << "  " << IDmap.at(Prev) << " -> " << IDmap.at(&I) << cfgStyle;
        }
        Prev = &I;
      }

      Instruction* Term = BB.getTerminator();
      if (Term == nullptr) {
        continue;
      }

      for (auto* Succ : successors(Term)) {
        Instruction* FirstInst = &*Succ->getFirstNonPHIOrDbg();
        OS << "  " << IDmap.at(Term) << " -> " << IDmap.at(FirstInst) << cfgStyle;
      }
    }
  }
}

void DotDumper::dumpDefUseEdges(Module& M) {
  for (Function& F : M) {
    for (BasicBlock& BB : F) {
      for (Instruction& I : BB) {
        const uint64_t CurrentID = IDmap.at(&I);

        for (Use& U : I.operands()) {
          Value* Op = U.get();

          if (IDmap.count(Op)) {
            const uint64_t OpID = IDmap.at(Op);
            OS << "  " << OpID << " -> " << CurrentID << defUseStyle;
          }
        }
      }
    }
  }
}

void DefUseGraph::addInstrumentation() {
  LLVMContext&   Context   = M.getContext();
  Type*          Int64Type = Type::getInt64Ty(Context);
  FunctionCallee LogFunc =
    M.getOrInsertFunction("__log_value", Type::getVoidTy(Context), Int64Type, Int64Type);

  for (Function& F : M) {
    if (F.isDeclaration())
      continue;

    IRBuilder<> Builder(&*F.getEntryBlock().getFirstInsertionPt());

    for (Argument& A : F.args()) {
      const uint64_t ArgID = IDmap[&A];

      Value* IDConst = Builder.getInt64(ArgID);
      Value* CastedVal;

      if (A.getType()->isPointerTy()) {
        CastedVal = Builder.CreatePtrToInt(&A, Int64Type);
      }
      else {
        CastedVal = Builder.CreateZExtOrBitCast(&A, Int64Type);
      }

      Builder.CreateCall(LogFunc, {IDConst, CastedVal});
    }

    for (BasicBlock& BB : F) {
      for (Instruction& I : BB) {
        if (I.getType()->isVoidTy() || I.isTerminator() || isa<PHINode>(&I)) {
          continue;
        }

        auto mapIt = IDmap.find(&I);

        if (mapIt == IDmap.end()) {
          continue;
        }

        IRBuilder<> Builder(I.getNextNode());

        const uint64_t InstID  = mapIt->second;
        Value*         IDConst = Builder.getInt64(InstID);

        Value* CastedVal;

        if (I.getType()->isPointerTy()) {
          CastedVal = Builder.CreatePtrToInt(&I, Int64Type);
        }
        else {
          CastedVal = Builder.CreateZExtOrBitCast(&I, Int64Type);
        }

        Builder.CreateCall(LogFunc, {IDConst, CastedVal});
      }
    }
  }
}
}  // namespace

PreservedAnalyses DefUseGraphPass::run(Module& M, ModuleAnalysisManager&) {
  DefUseGraph graph {M};
  graph.buildAndSave();

  return PreservedAnalyses::none();
}

extern "C" ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
  return {
    LLVM_PLUGIN_API_VERSION, "DefUseGraphPass", LLVM_VERSION_STRING, [](PassBuilder& PB) {
      PB.registerPipelineParsingCallback([](StringRef Name, ModulePassManager& MPM,
                                            ArrayRef<PassBuilder::PipelineElement>) {
        if (Name == "def-use-graph") {
          MPM.addPass(DefUseGraphPass());
          return true;
        }
        return false;
      });
    }};
}