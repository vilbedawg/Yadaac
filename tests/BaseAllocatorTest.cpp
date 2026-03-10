#include <cstdint>
#include <span>
#include <unordered_set>
#include <vector>

#include <gtest/gtest.h>

#include "yadaac/detail/base_allocator.hpp"

namespace {

using yadaac::detail::allocation::base_allocator;

TEST(BaseAllocatorTest, SingleLabelAllocationsYieldDistinctChildSlots) {
  base_allocator allocator;
  constexpr uint8_t label = 42;

  std::unordered_set<uint32_t> child_indices;
  for (int i = 0; i < 512; ++i) {
    uint32_t base = allocator.allocate_base(label);
    uint32_t child = base ^ label;
    EXPECT_TRUE(child_indices.insert(child).second);
  }
}

TEST(BaseAllocatorTest, MultiLabelAllocationReservesAllChildrenUniquely) {
  base_allocator allocator;
  const std::vector<uint8_t> labels = {1, 10, 64, 200};

  std::unordered_set<uint32_t> child_indices;
  for (int i = 0; i < 128; ++i) {
    uint32_t base = allocator.allocate_base(std::span<const uint8_t>(labels));
    for (uint8_t label : labels) {
      uint32_t child = base ^ label;
      EXPECT_TRUE(child_indices.insert(child).second);
    }
  }
}

TEST(BaseAllocatorTest, SingleLabelBaseIsMonotonic) {
  base_allocator allocator;
  constexpr uint8_t label = 7;

  uint32_t prev = allocator.allocate_base(label);
  for (int i = 0; i < 1024; ++i) {
    uint32_t curr = allocator.allocate_base(label);
    EXPECT_GE(curr, prev);
    prev = curr;
  }
}


TEST(BaseAllocatorTest, AllLabelsAllocation) {
  base_allocator allocator;
  std::vector<uint8_t> labels(256);
  for (int i = 0; i < 256; ++i) labels[i] = static_cast<uint8_t>(i);

  uint32_t base = allocator.allocate_base(std::span<const uint8_t>(labels));

  std::unordered_set<uint32_t> child_indices;
  for (int i = 0; i < 256; ++i) {
    uint32_t child = base ^ static_cast<uint8_t>(i);
    EXPECT_TRUE(child_indices.insert(child).second)
        << "Duplicate child index for label " << i;
  }
  EXPECT_EQ(child_indices.size(), 256U);
}

TEST(BaseAllocatorTest, MixedAllocations) {
  base_allocator allocator;
  std::unordered_set<uint32_t> all_children;

  // Interleave single-label and multi-label allocations
  for (int round = 0; round < 50; ++round) {
    // Single-label allocation
    uint8_t label = static_cast<uint8_t>(round % 256);
    uint32_t base1 = allocator.allocate_base(label);
    uint32_t child1 = base1 ^ label;
    EXPECT_TRUE(all_children.insert(child1).second)
        << "Collision on single-label round " << round;

    // Multi-label allocation
    std::vector<uint8_t> labels = {static_cast<uint8_t>((round * 3) % 256),
                                   static_cast<uint8_t>((round * 3 + 100) % 256)};
    uint32_t base2 = allocator.allocate_base(std::span<const uint8_t>(labels));
    for (uint8_t l : labels) {
      uint32_t child = base2 ^ l;
      EXPECT_TRUE(all_children.insert(child).second)
          << "Collision on multi-label round " << round << " label " << (int)l;
    }
  }
}

TEST(BaseAllocatorTest, StressMultipleBlocks) {
  base_allocator allocator;
  constexpr uint8_t label = 0;
  constexpr int count = 10000;

  std::unordered_set<uint32_t> child_indices;
  for (int i = 0; i < count; ++i) {
    uint32_t base = allocator.allocate_base(label);
    uint32_t child = base ^ label;
    EXPECT_TRUE(child_indices.insert(child).second)
        << "Duplicate at allocation " << i;
  }
  EXPECT_EQ(child_indices.size(), static_cast<size_t>(count));
}

}  // namespace
