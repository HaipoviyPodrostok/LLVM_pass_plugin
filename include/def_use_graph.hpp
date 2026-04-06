#pragma once

#include <cstdint>
#include <llvm/IR/Value.h>
#include <map>
#include <string>

#include <llvm/IR/Argument.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>

#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include <llvm/Config/llvm-config.h>
#include <llvm/IR/Analysis.h>

namespace DefUseGraphPass {

inline const std::string dotFile = "assets/dot_file.dot";

class DefUseGraphPass : llvm::PassInfoMixin<DefUseGraphPass> {
 public:
  llvm::PreservedAnalyses run(llvm::Module& M, llvm::FunctionAnalysisManager& );

 private:
  llvm::Module M;
 
 
  [[nodiscard]] std::map<llvm::Value*, uint64_t> indexation(llvm::Function& F);
  
  void makeDotFile(std::map<llvm::Value*, uint64_t> IDmap);
  
};

}  // namespace DefUseGraphPass