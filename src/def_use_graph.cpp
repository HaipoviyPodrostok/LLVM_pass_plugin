#include <cstdint>

#include <llvm/Passes/PassBuilder.h>
#include <llvm/Plugins/PassPlugin.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/Support/FileSystem.h>

#include <map>
#include <sys/types.h>
#include <system_error>

#include "def_use_graph.hpp"

using namespace llvm;

namespace {

inline const std::string dotFile = "assets/dot_file.dot";

class DefUseGraph {
 public:
  explicit DefUseGraph(Module& M) : M(M) {}
  
  void buildAndSafe() {
    indexation();
    makeDotFile();
  }
  
 private:
  Module& M;
  std::map<Value*, uint64_t> IDmap;

  void indexation();
  void makeDotFile();
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

void DefUseGraph::makeDotFile() {
  std::error_code EC;

  raw_fd_ostream File(dotFile, EC, sys::fs::OF_Text);
  if (EC) {
    errs() << "File open error: " << EC.message() << "\n";
    return;
  }
  
  File << "digraph DefUseGraph {\n";

  for (auto const& IDPair : IDmap) {
    const Value*   val = IDPair.first;
    const uint64_t id  = IDPair.second;

    File << "  " << id << " [label=\"" << val->getNameOrAsOperand() << "\"];\n";
  }

  for (Function& F : M) {
    for (BasicBlock& BB : F) {
      for (Instruction& I : BB) {
        const uint64_t CurrentID = IDmap[&I];

        for (Use& U : I.operands()) {
          Value* Op = U.get();

          if (IDmap.count(Op)) {
            const uint64_t OpID = IDmap[Op];
            File << "  " << OpID << " -> " << CurrentID << ";\n";
          }
        }
      }
    }
  }
  
  File << "}\n";
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
                [](StringRef Name, ModulePassManager &MPM,
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