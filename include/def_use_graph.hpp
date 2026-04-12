#pragma once

#include "llvm/IR/PassManager.h"

struct DefUseGraphPass : llvm::PassInfoMixin<DefUseGraphPass> {
  llvm::PreservedAnalyses run(llvm::Module& M, llvm::ModuleAnalysisManager& );  
};