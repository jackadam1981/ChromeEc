#include "llvm/Analysis/CFG.h"
#include "llvm/Analysis/ConstantFolding.h"
#include "llvm/IR/Constants.h"
#include "llvm/IR/DebugInfoMetadata.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/Pass.h"
#include "llvm/Support/raw_ostream.h"

#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace llvm;

namespace {
struct TypePairHash {
  std::size_t operator () (const std::pair<Type *, Type *> &TyPair) const {
    return std::hash<Type *>{}(
        TyPair.first) ^ std::hash<Type *>{}(TyPair.second);
  }
};
typedef std::unordered_set<std::pair<Type *, Type *>, TypePairHash> TypePair;

class Annotator : public ModulePass {
  std::unordered_map<Value *, std::vector<Constant *>> ConstantCache;

  bool isLayoutIdentical(Type *ATy, Type *BTy, TypePair *StackedTy) {
    bool Ret = false;

    std::pair<Type *, Type *> TyPair = std::make_pair(ATy, BTy);
    if (StackedTy->count(TyPair)) {
      return true;
    }
    StackedTy->insert(TyPair);

    if (auto *APtrTy = dyn_cast<PointerType>(ATy)) {
      auto *BPtrTy = dyn_cast<PointerType>(BTy);
      if (!BPtrTy) {
        return false;
      }
      // We are checking the layout identical, so all pointers are same.
      Ret = true;
    } else if (auto *AIntTy = dyn_cast<IntegerType>(ATy)) {
      auto *BIntTy = dyn_cast<IntegerType>(BTy);
      if (!BIntTy) {
        return false;
      }
      if (AIntTy->getBitWidth() != BIntTy->getBitWidth() ||
          AIntTy->getBitMask() != BIntTy->getBitMask() ||
          AIntTy->getSignBit() != BIntTy->getSignBit()) {
        return false;
      }
      Ret = true;
    } else if (auto *AFuncTy = dyn_cast<FunctionType>(ATy)) {
      auto *BFuncTy = dyn_cast<FunctionType>(BTy);
      if (!BFuncTy) {
        return false;
      }
      if (AFuncTy->isVarArg() || BFuncTy->isVarArg()) {
        return false;
      }
      if (AFuncTy->getNumParams() != BFuncTy->getNumParams()) {
        return false;
      }
      if (!isLayoutIdentical(AFuncTy->getReturnType(), BFuncTy->getReturnType(),
                             StackedTy)) {
        return false;
      }
      for (unsigned i = 0; i < AFuncTy->getNumParams(); ++i) {
        if (!isLayoutIdentical(AFuncTy->getParamType(i),
                               BFuncTy->getParamType(i), StackedTy)) {
          return false;
        }
      }
      Ret = true;
    } else if (auto *AStrTy = dyn_cast<StructType>(ATy)) {
      auto *BStrTy = dyn_cast<StructType>(BTy);
      if (!BStrTy) {
        return false;
      }
      if (AStrTy->isPacked() != BStrTy->isPacked() ||
          AStrTy->isOpaque() != BStrTy->isOpaque() ||
          AStrTy->isSized() != BStrTy->isSized()) {
        return false;
      }
      if (AStrTy->getNumElements() != BStrTy->getNumElements()) {
        return false;
      }
      for (unsigned i = 0; i < AStrTy->getNumElements(); ++i) {
        if (!isLayoutIdentical(AStrTy->getElementType(i),
                               BStrTy->getElementType(i), StackedTy)) {
          return false;
        }
      }
      Ret = true;
    } else if (auto *ASeqTy = dyn_cast<SequentialType>(ATy)) {
      auto *BSeqTy = dyn_cast<SequentialType>(BTy);
      if (!BSeqTy) {
        return false;
      }
      if (ASeqTy->getNumElements() != BSeqTy->getNumElements()) {
        return false;
      }
      Ret = isLayoutIdentical(ASeqTy->getElementType(),
			      BSeqTy->getElementType(), StackedTy);
    } else {
      Ret = ATy == BTy;
    }

    if (Ret) {
      StackedTy->erase(TyPair);
    }
    return Ret;
  }

  Constant * simplifyConstantExpr(ConstantExpr *CE) {
    if (CE->getOpcode() == Instruction::GetElementPtr) {
      if (auto *PCE = dyn_cast<ConstantExpr>(CE->getOperand(0))) {
        if (PCE->getOpcode() == Instruction::BitCast) {
          // Try to remove the redundant bitcast for identical but renamed
          // structures.
          auto *SrcPtrTy = dyn_cast<PointerType>(PCE->getOperand(0)->getType());
          auto *DstPtrTy = dyn_cast<PointerType>(PCE->getType());
          if (SrcPtrTy && DstPtrTy) {
            // The first operand of GEP is a bitcase pointer.
            // TODO: Other cases (e.g. vector of pointers)
            TypePair StackedTy;
            if (isLayoutIdentical(SrcPtrTy->getElementType(),
                                  DstPtrTy->getElementType(),
                                  &StackedTy)) {
              return ConstantExpr::getGetElementPtr(
                  nullptr,
                  PCE->getOperand(0),
                  std::vector<Value *>(CE->op_begin() + 1, CE->op_end()));
            }
          }
        }
      }
    }
    // Can't simplify, return original constant expression.
    return CE;
  }

  // This function will return empty list if it can't resolve the value to
  // constants, or there is no constant.
  std::vector<Constant *> concretizeValue(Value *V, const DataLayout &DL) {
    std::vector<Constant *> Ret;

    auto CacheIt = ConstantCache.find(V);
    if (CacheIt != ConstantCache.end()) {
      return CacheIt->second;
    }
    // Insert empty result first. So if there is a loop, it will return the
    // empty result and make the concretization fail.
    ConstantCache.insert(std::make_pair(V, std::vector<Constant *>()));

    if (auto *C = dyn_cast<Constant>(V)) {
      Ret = {C};
    } else if (auto *LI = dyn_cast<LoadInst>(V)) {
      std::vector<Constant *> Ptrs = concretizeValue(LI->getPointerOperand(),
                                                     DL);
      std::vector<Constant *> AllCVs;
      bool Error = false;
      for (Constant *Ptr : Ptrs) {
        if (Ptr->isNullValue()) {
          Error = true;
          break;
        }
        if (auto *CE = dyn_cast<ConstantExpr>(Ptr)) {
          Ptr = simplifyConstantExpr(CE);
        }
        auto *C = ConstantFoldLoadFromConstPtr(Ptr, LI->getType(), DL);
        if (!C) {
          Error = true;
          break;
        }
        AllCVs.push_back(C);
      }
      if (!Error) {
        Ret = AllCVs;
      }
    } else if (auto *GI = dyn_cast<GetElementPtrInst>(V)) {
      std::vector<Constant *> Ptrs = concretizeValue(GI->getPointerOperand(),
                                                     DL);
      for (Constant *Ptr : Ptrs) {
        if (Ptr->isNullValue()) {
          continue;
        }

        std::vector<std::vector<Constant *>> CandIdxLists;
        std::vector<Value *> IdxList;
        bool Error = false;

        CandIdxLists.push_back({});

        for (Value *Index : GI->indices()) {
          std::vector<Constant *> Cands;

          if (auto *C = dyn_cast<Constant>(Index)) {
            Cands.push_back(C);
          } else {
            uint64_t Range = 0;
            IntegerType *IdxTy = dyn_cast<IntegerType>(Index->getType());
            if (!IdxTy) {
              Error = true;
              break;
            }

            Type *ElmTy = GetElementPtrInst::getIndexedType(
              GI->getSourceElementType(), IdxList);
            if (auto *STy = dyn_cast<SequentialType>(ElmTy)) {
              Range = STy->getNumElements();
            } else {
              Error = true;
              break;
            }

            for (uint64_t i = 0; i < Range; ++i) {
              Cands.push_back(ConstantInt::get(IdxTy, i, false));
            }
          }

          std::vector<std::vector<Constant *>> NewIdxLists;
          for (auto &CandIdxList : CandIdxLists) {
            for (auto *C : Cands) {
              std::vector<Constant *> NewIdxList = CandIdxList;
              NewIdxList.push_back(C);
              NewIdxLists.push_back(NewIdxList);
            }
          }
          CandIdxLists = NewIdxLists;

          IdxList.push_back(Index);
        }
        if (!Error) {
          for (auto &CandIdxList : CandIdxLists) {
            if (auto *C = ConstantExpr::getGetElementPtr(nullptr, Ptr,
                                                         CandIdxList)) {
              Ret.push_back(C);
            }
          }
        }
      }
    } else if (auto *AI = dyn_cast<Argument>(V)) {
      Function *F = AI->getParent();
      std::vector<Constant *> AllCVs;
      bool Error = false;
      for (const User *U : F->users()) {
        if (auto *CI = dyn_cast<CallInst>(U)) {
          Value *OP = CI->getArgOperand(AI->getArgNo());
          std::vector<Constant *> CArgs = concretizeValue(OP, DL);
          if (CArgs.size() == 0) {
            Error = true;
            break;
          }
          AllCVs.insert(AllCVs.end(), CArgs.begin(), CArgs.end());
        } else {
          Error = true;
          break;
        }
      }
      if (!Error) {
        Ret = AllCVs;
      }
    } else if (auto *CI = dyn_cast<CallInst>(V)) {
      std::vector<Constant *> Ptrs = concretizeValue(CI->getCalledValue(), DL);
      std::vector<Constant *> AllCVs;
      bool Error = false;
      for (Constant *Ptr : Ptrs) {
        if (Ptr->isNullValue()) {
          Error = true;
          break;
        }
        if (auto *F = dyn_cast<Function>(Ptr)) {
          for (BasicBlock &BB : *F) {
            if (auto *RI = dyn_cast<ReturnInst>(BB.getTerminator())) {
              if (Value *RV = RI->getReturnValue()) {
                std::vector<Constant *> CRVs = concretizeValue(RV, DL);
                if (CRVs.size() == 0) {
                  Error = true;
                  break;
                }
                AllCVs.insert(AllCVs.end(), CRVs.begin(), CRVs.end());
              }
            }
          }
          if (Error) {
            break;
          }
        }
      }
      if (!Error) {
        Ret = AllCVs;
      }
    } else if (auto *PHI = dyn_cast<PHINode>(V)) {
      std::vector<Constant *> AllCVs;
      bool Error = false;
      for (unsigned i = 0; i < PHI->getNumIncomingValues(); ++i) {
        BasicBlock *PBB = PHI->getIncomingBlock(i);
        Value *PV = PHI->getIncomingValue(i);
        if (isPotentiallyReachable(PHI->getParent(), PBB)) {
          // There is a loop.
          Error = true;
          break;
        } else {
          std::vector<Constant *> CPVs = concretizeValue(PV, DL);
          if (CPVs.size() == 0) {
            Error = true;
            break;
          }
          AllCVs.insert(AllCVs.end(), CPVs.begin(), CPVs.end());
        }
      }
      if (!Error) {
        Ret = AllCVs;
      }
    }

    ConstantCache[V] = Ret;
    return Ret;
  }

public:
  static char ID;
  Annotator() : ModulePass(ID) {}

  bool runOnModule(Module &M) override {
    for (auto &GV : M.globals()) {
      // HACK: Set all global variables to constant so the constant propgation
      // can derive constant values from them.
      GV.setConstant(true);
    }

    for (Function &F : M) {
      for (BasicBlock &BB : F) {
        for (Instruction &I : BB) {
          if (auto CI = dyn_cast<CallInst>(&I)) {
            if (auto VI = dyn_cast<Instruction>(CI->getCalledValue())) {
              DILocation *DL = CI->getDebugLoc().get();
              if (!DL) {
                // Fail to get the line number.
                continue;
              }

              // Found an indirect call.
              // Try to conretize its targets.
              std::vector<Constant *> Cands = concretizeValue(
                  VI, M.getDataLayout());
              if (Cands.size() == 0) {
                // Fail to resolve the targets.
                continue;
              }

              bool Error = false;
              std::unordered_set<DISubprogram *> Targets;
              for (Constant *C : Cands) {
                if (C->isNullValue()) {
                  // Found a null target.
                  Error = true;
                  break;
                }
                auto *CandFunction = dyn_cast<Function>(C);
                if (!CandFunction) {
                  Error = true;
                  break;
                }
                DISubprogram *DS = CandFunction->getSubprogram();
                if (!DS) {
                  Error = true;
                  break;
                }
                Targets.insert(DS);
              }
              if (!Error) {
                // Output annotation.
                errs() << F.getName() << "[" <<
                       DL->getFilename() << ":" << DL->getLine() <<
                       "]:\n";
                for (DISubprogram *DS : Targets) {
                  errs() << "- " << DS->getName() << "[" <<
                         DS->getFilename() << ":" << DS->getLine() <<
                         "]\n";
                }
              }
            }
          }
        }
      }
    }
    return true;
  }
}; // end of class Annotater
} // end of anonymous namespace

char Annotator::ID = 0;
static RegisterPass<Annotator> X("annotator", "Annotater Pass", false , false);
