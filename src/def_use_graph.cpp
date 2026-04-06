#include <cstdint>
#include <llvm/IR/Function.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/Support/FileSystem.h>
#include <map>
#include <sys/types.h>
#include <system_error>

#include "def_use_graph.hpp"

using namespace llvm;

namespace DefUseGraphPass {
  
PreservedAnalyses DefUseGraphPass::run(Module& M, FunctionAnalysisManager& ) {
  return PreservedAnalyses::all();
}

std::map<llvm::Value*, uint64_t> indexation(Module& M) {
  std::map<llvm::Value*, uint64_t> IDmap;
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
  
  return IDmap;
}

void DefUseGraphPass::makeDotFile(std::map<llvm::Value*, uint64_t> IDmap) {
  std::error_code EC;

  raw_fd_ostream File(dotFile, EC, sys::fs::OF_Text);
  if (EC) {
    errs() << "File open error: " << EC.message() << "\n";
    return;
  }
  File << "DefUseGraph {\n";

  for (auto const& IDPair : IDmap) {
    Value*   val = IDPair.first;
    uint64_t id  = IDPair.second;

    File << "  " << id << " [label=\"" << val->getNameOrAsOperand() << "\"];\n";
  }

  for (BasicBlock& BB : F) {
    for (Instruction& I : BB) {
      uint64_t CurrentID = IDmap[&I];
      // Перебираем все входные данные (операнды) этой инструкции
      for (Use &U : I.operands()) {
        Value *Op = U.get();

        // Проверяем, есть ли этот операнд в нашей мапе ID
        if (IDmap.count(Op)) {
          uint64_t OpID = IDmap[Op];
          // Рисуем стрелку: Операнд -> Инструкция
          File << "  " << OpID << " -> " << CurrentID << ";\n";
        }
      }
    }
  }
  File << "}\n";
}
} // namespace DefUseGraphPass



// extern "C" ::llvm::PassPluginLibraryInfo llvmGetPassPluginInfo() {
//   return {LLVM_PLUGIN_API_VERSION, "HelloWorldPass", LLVM_VERSION_STRING,
//           [](PassBuilder &PB) {
//             PB.registerPipelineParsingCallback(
//                 [](StringRef Name, FunctionPassManager &FPM,
//                    ArrayRef<PassBuilder::PipelineElement>) {
//                   if (Name == "hello-world") {
//                     FPM.addPass(DefUseGraphPass());
//                     return true;
//                   }
//                   return false;
//                 });
//           }};
// }