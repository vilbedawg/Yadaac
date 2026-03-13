#include <stdexcept>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "yadaac/detail/builder_impl.hpp"
#include "yadaac/yadaac.hpp"

namespace {

using yadaac::detail::construction::builder_impl;

TEST(DAACConstructionTest, ThrowsOnEmptyPatternSet) {
    std::vector<std::string> patterns;
    EXPECT_THROW((void)builder_impl(patterns), std::runtime_error);
}

TEST(DAACConstructionTest, ThrowsOnDuplicatePattern) {
    std::vector<std::string> patterns = {"abc", "abc"};
    EXPECT_THROW((void)builder_impl(patterns), std::runtime_error);
}

TEST(DAACConstructionTest, BuildsOutputsForAllUniquePatterns) {
    std::vector<std::string> patterns = {"he", "she", "his", "hers"};
    builder_impl builder(patterns);

    ASSERT_EQ(builder.outputs.size(), patterns.size());
    EXPECT_GE(builder.get_state_count(), static_cast<uint32_t>(1));

    for (const auto& out : builder.outputs) {
        ASSERT_LT(out.value, patterns.size());
        EXPECT_EQ(out.length, patterns[out.value].size());
    }
}

TEST(DAACConstructionTest, SinglePattern) {
    std::vector<std::string> patterns = {"hello"};
    builder_impl builder(patterns);
    ASSERT_EQ(builder.outputs.size(), 1U);
    EXPECT_EQ(builder.outputs[0].length, 5U);
    EXPECT_GE(builder.get_state_count(), 1U);
}

TEST(DAACConstructionTest, LargeNumberOfPatterns) {
    std::vector<std::string> patterns;
    patterns.reserve(1000);
    for (int i = 0; i < 1000; ++i) {
        patterns.push_back("pattern" + std::to_string(i));
    }
    builder_impl builder(patterns);
    ASSERT_EQ(builder.outputs.size(), 1000U);

    for (const auto& out : builder.outputs) {
        ASSERT_LT(out.value, 1000U);
        EXPECT_EQ(out.length, patterns[out.value].size());
    }
}

TEST(DAACConstructionTest, VeryLongPattern) {
    std::string long_pattern(1000, 'x');
    std::vector<std::string> patterns = {long_pattern};
    builder_impl builder(patterns);
    ASSERT_EQ(builder.outputs.size(), 1U);
    EXPECT_EQ(builder.outputs[0].length, 1000U);
}

TEST(DAACConstructionTest, NonASCIIPatterns) {
    std::string p1 = {(char)0x80, (char)0x81, (char)0x82};
    std::string p2 = {(char)0xFE, (char)0xFF};
    std::string p3 = {(char)0x00, (char)0x01, (char)0x02};
    std::vector<std::string> patterns = {p1, p2, p3};
    builder_impl builder(patterns);
    ASSERT_EQ(builder.outputs.size(), 3U);
}

TEST(DAACConstructionTest, StateAndTransitionCounts) {
    std::vector<std::string> patterns = {"a", "ab", "abc", "b", "bc"};
    yadaac::daac automaton(patterns);
    // Must have at least 1 state (root) and some transitions
    EXPECT_GE(automaton.num_of_states(), 1U);
    EXPECT_GT(automaton.num_of_transitions(), 0U);
    // States should be <= transitions (transitions include the state array)
    EXPECT_LE(automaton.num_of_states(), automaton.num_of_transitions());
}

}  // namespace
