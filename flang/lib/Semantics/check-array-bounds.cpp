//===-- check-array-bounds.cpp -- Array Bounds Checker ------------------===//
#include "flang/Semantics/check-array-bounds.h"
#include "flang/Evaluate/call.h"
#include "flang/Evaluate/expression.h"
#include "flang/Evaluate/fold.h"
#include "flang/Evaluate/tools.h"
#include "flang/Parser/parse-tree.h"
#include "flang/Semantics/expression.h"
#include "flang/Semantics/semantics.h"
#include "flang/Semantics/symbol.h"
#include "flang/Semantics/tools.h"
#include "flang/Semantics/type.h"
#include "llvm/Support/raw_ostream.h"
#include <set>

namespace Fortran::semantics {

static const evaluate::Expr<evaluate::SomeType> *GetTypedExpr(
    const parser::Expr &expr) {
  if (const auto &typed{expr.typedExpr}) {
    if (const auto &opt{typed->v}) {
      return &*opt;
    }
  }
  return nullptr;
}

// ── Collect array bounds from declarations ───────────────────────────────────

void ArrayBoundsChecker::Enter(const parser::EntityDecl &entityDecl) {
  const auto &name{std::get<parser::Name>(entityDecl.t)};
  if (name.symbol) {
    CollectArrayBounds(*name.symbol);
  }
}

void ArrayBoundsChecker::CollectArrayBounds(const Symbol &sym) {
  const auto *details{sym.detailsIf<ObjectEntityDetails>()};
  if (!details || details->shape().empty()) {
    return;
  }

  std::vector<DimBounds> dims;
  int dimNum{1};
  for (const auto &shapeSpec : details->shape()) {
    DimBounds db;
    db.lower = 1;
    db.dimNumber = dimNum++;

    const auto &lbOpt{shapeSpec.lbound().GetExplicit()};
    if (lbOpt) {
      if (auto cv{evaluate::ToInt64(*lbOpt)}) {
        db.lower = *cv;
      }
    }
    const auto &ubOpt{shapeSpec.ubound().GetExplicit()};
    if (ubOpt) {
      if (auto cv{evaluate::ToInt64(*ubOpt)}) {
        db.upper = *cv;
      }
    }
    if (db.lower && db.upper) {
      db.size = *db.upper - *db.lower + 1;
    }
    dims.push_back(db);
  }

  if (!dims.empty()) {
    const std::string symName{sym.name().ToString()};
    arrayBoundsMap_[symName] = dims;
    arrayRankMap_[symName] = static_cast<int>(dims.size());

    // Assumed-size detection: dummy arg declared as A(*)
    if (details->shape().CanBeAssumedSize()) {
      assumedSizeArrays_.insert(symName);
      arrayBoundsMap_[symName].back().isAssumedSize = true;
    }

    // Common block tracking: register array with its common block
    if (const Symbol *commonBlock{details->commonBlock()}) {
      const std::string blockName{commonBlock->name().ToString()};
      commonBlockMap_[blockName].push_back(symName);
      arrayToCommonBlock_[symName] = blockName;
      ++stats_.totalCommonBlock;

      // Also register all other arrays in the same common block
      const auto *blockDetails{
          commonBlock->detailsIf<CommonBlockDetails>()};
      if (blockDetails) {
        for (const auto &objRef : blockDetails->objects()) {
          const Symbol &obj{*objRef};
          if (obj.name().ToString() != symName) {
            // Register sibling array if not already registered
            if (arrayBoundsMap_.find(obj.name().ToString()) ==
                arrayBoundsMap_.end()) {
              CollectArrayBounds(obj);
            }
          }
        }
      }
    }
  }
}

// ── Feature 2: Check array slice (SubscriptTriplet) bounds ─────────────────
void ArrayBoundsChecker::CheckSliceDimension(const std::string &arrName,
    int dimIdx, const DimBounds &db, int rank,
    const parser::SubscriptTriplet &triplet) {

  // SubscriptTriplet = tuple<optional<Subscript>, optional<Subscript>,
  //                          optional<Subscript>>
  // [lower : upper : stride]
  const auto &lowerOpt{std::get<0>(triplet.t)};
  const auto &upperOpt{std::get<1>(triplet.t)};

  // Check slice lower bound
  if (lowerOpt) {
    // ScalarIntExpr = Scalar<IntExpr> = Scalar<Integer<Indirection<Expr>>>
    // .thing = Integer<Indirection<Expr>>
    // .thing = Indirection<Expr>
    // .value() = Expr
    const parser::Expr &lExpr{lowerOpt->thing.thing.value()};
    const parser::CharBlock src{lExpr.source};
    if (const auto *typed{GetTypedExpr(lExpr)}) {
      if (auto cv{evaluate::ToInt64(*typed)}) {
        const std::int64_t sliceLo{*cv};
        if (db.lower && sliceLo < *db.lower) {
          ++stats_.totalErrors;
          const std::int64_t lo{db.lower.value_or(1)};
          const std::int64_t hi{db.upper.value_or(0)};
          if (rank > 1) {
            context_.Say(src,
                "Array '%s' dimension %d of %d: slice lower bound %jd "
                "is outside declared bounds [%jd:%jd]"_err_en_US,
                arrName, dimIdx + 1, rank,
                static_cast<std::intmax_t>(sliceLo),
                static_cast<std::intmax_t>(lo),
                static_cast<std::intmax_t>(hi));
          } else {
            context_.Say(src,
                "Array '%s' dimension %d: slice lower bound %jd "
                "is outside declared bounds [%jd:%jd]"_err_en_US,
                arrName, dimIdx + 1,
                static_cast<std::intmax_t>(sliceLo),
                static_cast<std::intmax_t>(lo),
                static_cast<std::intmax_t>(hi));
          }
          llvm::errs() << "  hint: valid slice lower bound for '"
                       << arrName << "' dimension " << (dimIdx + 1)
                       << " is >= " << lo << "\n";
        } else {
          ++stats_.totalVerified;
        }
      }
    }
  }

  // Check slice upper bound
  if (upperOpt) {
    const parser::Expr &uExpr{upperOpt->thing.thing.value()};
    const parser::CharBlock src{uExpr.source};
    if (const auto *typed{GetTypedExpr(uExpr)}) {
      if (auto cv{evaluate::ToInt64(*typed)}) {
        const std::int64_t sliceHi{*cv};
        if (db.upper && sliceHi > *db.upper) {
          ++stats_.totalErrors;
          const std::int64_t lo{db.lower.value_or(1)};
          const std::int64_t hi{db.upper.value_or(0)};
          if (rank > 1) {
            context_.Say(src,
                "Array '%s' dimension %d of %d: slice upper bound %jd "
                "is outside declared bounds [%jd:%jd]"_err_en_US,
                arrName, dimIdx + 1, rank,
                static_cast<std::intmax_t>(sliceHi),
                static_cast<std::intmax_t>(lo),
                static_cast<std::intmax_t>(hi));
          } else {
            context_.Say(src,
                "Array '%s' dimension %d: slice upper bound %jd "
                "is outside declared bounds [%jd:%jd]"_err_en_US,
                arrName, dimIdx + 1,
                static_cast<std::intmax_t>(sliceHi),
                static_cast<std::intmax_t>(lo),
                static_cast<std::intmax_t>(hi));
          }
          llvm::errs() << "  hint: valid slice upper bound for '"
                       << arrName << "' dimension " << (dimIdx + 1)
                       << " is <= " << hi << "\n";
        } else {
          ++stats_.totalVerified;
        }
      }
    }
  }
}

// ── Check array subscripts ───────────────────────────────────────────────────

void ArrayBoundsChecker::Enter(const parser::ArrayElement &arrayElem) {
  const parser::DataRef &baseRef{arrayElem.Base()};
  const parser::Name *namePart{std::get_if<parser::Name>(&baseRef.u)};
  if (!namePart || !namePart->symbol) {
    return;
  }

  const std::string arrName{namePart->symbol->name().ToString()};

  // Lazy detection for dummy arguments — EntityDecl does not fire for them
  if (arrayBoundsMap_.find(arrName) == arrayBoundsMap_.end() &&
      aliasMap_.find(arrName) == aliasMap_.end()) {
    const Symbol &ultimate{namePart->symbol->GetUltimate()};
    const auto *details{ultimate.detailsIf<ObjectEntityDetails>()};
    if (details && !details->shape().empty()) {
      CollectArrayBounds(ultimate);
    }
  }

  // Look up bounds — try direct name then alias map
  auto it{arrayBoundsMap_.find(arrName)};
  if (it == arrayBoundsMap_.end()) {
    auto aliasIt{aliasMap_.find(arrName)};
    if (aliasIt != aliasMap_.end()) {
      it = arrayBoundsMap_.find(aliasIt->second);
    }
  }
  if (it == arrayBoundsMap_.end()) {
    return;
  }

  // Assumed-size warning — any access is unverifiable
  if (assumedSizeArrays_.count(arrName)) {
    ++stats_.totalAssumedSize;
    const auto &subs{arrayElem.Subscripts()};
    if (!subs.empty()) {
      const auto *firstIntSub{
          std::get_if<parser::IntExpr>(&subs.front().u)};
      if (firstIntSub) {
        const parser::CharBlock src{firstIntSub->thing.value().source};
        context_.Say(src,
            "Array '%s' is assumed-size (declared with *); "
            "bounds cannot be verified for any access"_warn_en_US,
            arrName);
      }
    }
    return;
  }

  const std::vector<DimBounds> &declaredDims{it->second};
  const auto &subscripts{arrayElem.Subscripts()};

  if (subscripts.size() != declaredDims.size()) {
    return;
  }

  int rank{static_cast<int>(declaredDims.size())};
  int dimIdx{0};

  for (const auto &sub : subscripts) {
    const DimBounds &db{declaredDims[dimIdx]};

    // Handle SubscriptTriplet (slice) — Feature 2
    if (const auto *triplet{
            std::get_if<parser::SubscriptTriplet>(&sub.u)}) {
      CheckSliceDimension(arrName, dimIdx, db, rank, *triplet);
      ++dimIdx;
      continue;
    }

    const auto *intSub{std::get_if<parser::IntExpr>(&sub.u)};
    if (!intSub) {
      ++dimIdx;
      continue;
    }

    const parser::Expr &theExpr{intSub->thing.value()};
    const parser::CharBlock src{theExpr.source};
    const auto *typedExpr{GetTypedExpr(theExpr)};

    if (!typedExpr) {
      ++stats_.totalWarnings;
      if (rank > 1) {
        context_.Say(src,
            "Array '%s' dimension %d of %d: index cannot be verified "
            "at compile time"_warn_en_US,
            arrName, dimIdx + 1, rank);
      } else {
        context_.Say(src,
            "Array '%s' dimension %d: index cannot be verified at "
            "compile time"_warn_en_US,
            arrName, dimIdx + 1);
      }
      ++dimIdx;
      continue;
    }

    std::optional<std::int64_t> idxVal{evaluate::ToInt64(*typedExpr)};

    if (idxVal) {
      ++stats_.totalVerified;
      CheckOneDimension(arrName, dimIdx, db, idxVal, src, rank);
    } else {
      ++stats_.totalWarnings;
      if (rank > 1) {
        context_.Say(src,
            "Array '%s' dimension %d of %d: index cannot be verified "
            "at compile time"_warn_en_US,
            arrName, dimIdx + 1, rank);
      } else {
        context_.Say(src,
            "Array '%s' dimension %d: index cannot be verified at "
            "compile time"_warn_en_US,
            arrName, dimIdx + 1);
      }
    }
    ++dimIdx;
  }
}

// ── Diagnostic emitter ───────────────────────────────────────────────────────

void ArrayBoundsChecker::CheckOneDimension(const std::string &arrayName,
    int dimIndex, const DimBounds &bounds,
    std::optional<std::int64_t> indexValue,
    parser::CharBlock source, int rank) {

  if (!indexValue) return;

  const std::int64_t idx{*indexValue};
  const bool tooLow{bounds.lower && idx < *bounds.lower};
  const bool tooHigh{bounds.upper && idx > *bounds.upper};

  if (!tooLow && !tooHigh) return;

  ++stats_.totalErrors;

  const std::int64_t lo{bounds.lower.value_or(1)};
  const std::int64_t hi{bounds.upper.value_or(0)};

  if (rank > 1) {
    context_.Say(source,
        "Array '%s' dimension %d of %d: index %jd is outside declared "
        "bounds [%jd:%jd]"_err_en_US,
        arrayName, dimIndex + 1, rank,
        static_cast<std::intmax_t>(idx),
        static_cast<std::intmax_t>(lo),
        static_cast<std::intmax_t>(hi));
  } else {
    context_.Say(source,
        "Array '%s' dimension %d: index %jd is outside declared bounds "
        "[%jd:%jd]"_err_en_US,
        arrayName, dimIndex + 1,
        static_cast<std::intmax_t>(idx),
        static_cast<std::intmax_t>(lo),
        static_cast<std::intmax_t>(hi));
  }

  // Feature 4: hint with nearest valid index
  const std::int64_t suggested{tooLow ? lo : hi};
  llvm::errs() << "  hint: nearest valid index for '" << arrayName
               << "' dimension " << (dimIndex + 1)
               << " is " << suggested
               << " (valid range is [" << lo << ":" << hi << "])\n";
}

// ── Feature 4: Check array sizes at call sites ──────────────────────────────
void ArrayBoundsChecker::Enter(const parser::CallStmt &callStmt) {
  // Get the typed call — filled in by Flang's semantic pass
  if (!callStmt.typedCall) {
    return;
  }

  const evaluate::ProcedureRef &procRef{*callStmt.typedCall};
  const auto &args{procRef.arguments()};

  // Get the procedure symbol to find dummy argument declarations
  const Symbol *procSym{nullptr};
  if (const auto *named{
          std::get_if<evaluate::SymbolRef>(&procRef.proc().u)}) {
    procSym = &named->get();
  }
  if (!procSym) {
    return;
  }

  // Get the subroutine's scope to look up dummy argument bounds
  const Scope *subScope{procSym->scope()};
  if (!subScope) {
    return;
  }

  // Walk actual arguments and match to dummy arguments
  const auto *subDetails{procSym->detailsIf<SubprogramDetails>()};
  if (!subDetails) {
    return;
  }

  const auto &dummyArgs{subDetails->dummyArgs()};
  std::size_t argIdx{0};

  for (const auto &arg : args) {
    if (!arg || argIdx >= dummyArgs.size()) {
      ++argIdx;
      continue;
    }

    // Get actual argument expression
    const evaluate::Expr<evaluate::SomeType> *actualExpr{arg->UnwrapExpr()};
    if (!actualExpr) {
      ++argIdx;
      continue;
    }

    // Get actual array symbol
    const Symbol *actualSym{evaluate::UnwrapWholeSymbolDataRef(*actualExpr)};
    if (!actualSym) {
      ++argIdx;
      continue;
    }

    // Get dummy argument symbol
    const Symbol *dummySym{dummyArgs[argIdx]};
    if (!dummySym) {
      ++argIdx;
      continue;
    }

    // Get actual array bounds
    const auto *actualDetails{
        actualSym->detailsIf<ObjectEntityDetails>()};
    const auto *dummyDetails{
        dummySym->detailsIf<ObjectEntityDetails>()};

    if (!actualDetails || !dummyDetails) {
      ++argIdx;
      continue;
    }

    // Only check 1D arrays for now
    if (actualDetails->shape().Rank() != 1 ||
        dummyDetails->shape().Rank() != 1) {
      ++argIdx;
      continue;
    }

    // Skip assumed-size dummy args — they accept any size
    if (dummyDetails->shape().CanBeAssumedSize()) {
      ++argIdx;
      continue;
    }

    // Get actual array size
    std::optional<std::int64_t> actualSize;
    for (const auto &shapeSpec : actualDetails->shape()) {
      const auto &ubOpt{shapeSpec.ubound().GetExplicit()};
      const auto &lbOpt{shapeSpec.lbound().GetExplicit()};
      if (ubOpt && lbOpt) {
        auto ub{evaluate::ToInt64(*ubOpt)};
        auto lb{evaluate::ToInt64(*lbOpt)};
        if (ub && lb) {
          actualSize = *ub - *lb + 1;
        }
      }
    }

    // Get dummy array size
    std::optional<std::int64_t> dummySize;
    for (const auto &shapeSpec : dummyDetails->shape()) {
      const auto &ubOpt{shapeSpec.ubound().GetExplicit()};
      const auto &lbOpt{shapeSpec.lbound().GetExplicit()};
      if (ubOpt && lbOpt) {
        auto ub{evaluate::ToInt64(*ubOpt)};
        auto lb{evaluate::ToInt64(*lbOpt)};
        if (ub && lb) {
          dummySize = *ub - *lb + 1;
        }
      }
    }

    // Compare sizes
    if (actualSize && dummySize && *actualSize < *dummySize) {
      ++stats_.totalErrors;
      const parser::CharBlock src{callStmt.source};
      context_.Say(src,
          "Actual argument '%s' has size %jd but dummy argument '%s' "
          "expects size %jd"_warn_en_US,
          actualSym->name().ToString(),
          static_cast<std::intmax_t>(*actualSize),
          dummySym->name().ToString(),
          static_cast<std::intmax_t>(*dummySize));
      llvm::errs() << "  hint: pass an array of at least size "
                   << *dummySize << " for argument '"
                   << dummySym->name().ToString() << "'\n";
    } else if (actualSize && dummySize) {
      ++stats_.totalVerified;
    }

    ++argIdx;
  }
}

// ── Feature 5: Track ALLOCATE statements for allocatable arrays ─────────────
void ArrayBoundsChecker::Enter(const parser::AllocateStmt &allocStmt) {
  // AllocateStmt = tuple<optional<TypeSpec>, list<Allocation>, list<AllocOpt>>
  const auto &allocations{std::get<std::list<parser::Allocation>>(allocStmt.t)};

  for (const auto &alloc : allocations) {
    // Allocation = tuple<AllocateObject, list<AllocateShapeSpec>, optional<...>>
    const auto &allocObj{std::get<parser::AllocateObject>(alloc.t)};
    const auto &shapeSpecs{
        std::get<std::list<parser::AllocateShapeSpec>>(alloc.t)};

    // Get array name from AllocateObject
    const parser::Name *namePart{
        std::get_if<parser::Name>(&allocObj.u)};
    if (!namePart || !namePart->symbol) {
      continue;
    }

    const std::string arrName{namePart->symbol->name().ToString()};

    if (shapeSpecs.empty()) {
      continue;
    }

    // Build bounds from AllocateShapeSpec list
    // AllocateShapeSpec = tuple<optional<BoundExpr>, BoundExpr>
    // [lower :] upper
    std::vector<DimBounds> dims;
    int dimNum{1};
    bool allConstant{true};

    for (const auto &spec : shapeSpecs) {
      DimBounds db;
      db.dimNumber = dimNum++;
      db.lower = 1; // default lower bound

      // Lower bound (optional)
      const auto &lbOpt{std::get<0>(spec.t)};
      if (lbOpt) {
        // BoundExpr = ScalarIntExpr = Scalar<IntExpr>
        // .thing = IntExpr = Integer<Indirection<Expr>>
        // .thing = Indirection<Expr>
        // .value() = Expr
        const parser::Expr &lExpr{lbOpt->thing.thing.value()};
        if (const auto *typed{GetTypedExpr(lExpr)}) {
          if (auto cv{evaluate::ToInt64(*typed)}) {
            db.lower = *cv;
          } else {
            allConstant = false;
          }
        }
      }

      // Upper bound (required)
      const auto &ubExpr{std::get<1>(spec.t)};
      const parser::Expr &uExpr{ubExpr.thing.thing.value()};
      if (const auto *typed{GetTypedExpr(uExpr)}) {
        if (auto cv{evaluate::ToInt64(*typed)}) {
          db.upper = *cv;
          if (db.lower && db.upper) {
            db.size = *db.upper - *db.lower + 1;
          }
        } else {
          allConstant = false;
        }
      }

      dims.push_back(db);
    }

    if (!dims.empty()) {
      // Store in both maps — allocatable bounds override static bounds
      allocatableBoundsMap_[arrName] = dims;
      arrayBoundsMap_[arrName] = dims;
      arrayRankMap_[arrName] = static_cast<int>(dims.size());

      if (allConstant) {
        ++stats_.totalAllocatable;
        llvm::errs() << "  [alloc] Tracked ALLOCATE(" << arrName << "(";
        for (std::size_t i{0}; i < dims.size(); ++i) {
          if (i > 0) llvm::errs() << ",";
          if (dims[i].lower && *dims[i].lower != 1)
            llvm::errs() << *dims[i].lower << ":";
          if (dims[i].upper) llvm::errs() << *dims[i].upper;
        }
        llvm::errs() << "))\n";
      }
    }
  }
}

// ── Summary report ───────────────────────────────────────────────────────────

ArrayBoundsChecker::~ArrayBoundsChecker() {
  llvm::errs()
      << "\n===== Array Bounds Check Summary =====\n"
      << "  Verified safe accesses : " << stats_.totalVerified << "\n"
      << "  Unverifiable (warnings): " << stats_.totalWarnings << "\n"
      << "  Assumed-size accesses  : " << stats_.totalAssumedSize << "\n"
      << "  Common block arrays    : " << stats_.totalCommonBlock << "\n"
      << "  Allocatable tracked    : " << stats_.totalAllocatable << "\n"
      << "  Out-of-bounds (errors) : " << stats_.totalErrors << "\n"
      << "======================================\n";
}

} // namespace Fortran::semantics
