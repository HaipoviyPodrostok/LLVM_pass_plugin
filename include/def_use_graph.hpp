#pragma once

#include <llvm/IR/Value.h>

#include <llvm/IR/Argument.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Instruction.h>

#include "llvm/IR/Function.h"
#include "llvm/IR/PassManager.h"
#include <llvm/Config/llvm-config.h>
#include <llvm/IR/Analysis.h>

// FIXME[flops]: You only need Pass include there, remove all unused includes
struct DefUseGraphPass : llvm::PassInfoMixin<DefUseGraphPass> {
  llvm::PreservedAnalyses run(llvm::Module& M, llvm::ModuleAnalysisManager& );  
};