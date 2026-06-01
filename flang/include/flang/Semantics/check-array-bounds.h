//===-- check-array-bounds.h -- Array Bounds Checker -----------*- C++ -*-===//
#ifndef FORTRAN_SEMANTICS_CHECK_ARRAY_BOUNDS_H_
#define FORTRAN_SEMANTICS_CHECK_ARRAY_BOUNDS_H_

#include "flang/Semantics/scope.h"
#include "flang/Semantics/semantics.h"
#include "flang/Semantics/symbol.h"
#include "flang/Parser/parse-tree.h"
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace Fortran::semantics {

struct DimBounds {
  std::optional<std::int64_t> lower;
  std::optional<std::int64_t> upper;
  std::optional<std::int64_t> size;
  int dimNumber{1};
  bool isAssumedSize{false};
};

struct BoundsCheckStats {
  int totalErrors{0};
  int totalWarnings{0};
  int totalVerified{0};
  int totalAssumedSize{0};
  int totalCommonBlock{0};
  int totalAllocatable{0};
};

class ArrayBoundsChecker : public virtual BaseChecker {
public:
  explicit ArrayBoundsChecker(SemanticsContext &context)
      : context_{context} {}
  ~ArrayBoundsChecker();

  void Enter(const parser::ArrayElement &);
  void Enter(const parser::EntityDecl &);
  void Enter(const parser::CallStmt &);
  void Enter(const parser::AllocateStmt &);

private:
  SemanticsContext &context_;
  std::map<std::string, std::vector<DimBounds>> arrayBoundsMap_;
  std::map<std::string, int> arrayRankMap_;
  std::map<std::string, std::string> aliasMap_;
  std::set<std::string> assumedSizeArrays_;
  std::map<std::string, std::vector<std::string>> commonBlockMap_;
  std::map<std::string, std::string> arrayToCommonBlock_;
  std::map<std::string, std::vector<DimBounds>> allocatableBoundsMap_;
  BoundsCheckStats stats_;

  void CollectArrayBounds(const Symbol &symbol);
  void CheckOneDimension(const std::string &arrayName, int dimIndex,
      const DimBounds &bounds, std::optional<std::int64_t> indexValue,
      parser::CharBlock source, int rank = 1);
  void CheckSliceDimension(const std::string &arrName, int dimIdx,
      const DimBounds &db, int rank,
      const parser::SubscriptTriplet &triplet);
};

} // namespace Fortran::semantics
#endif
