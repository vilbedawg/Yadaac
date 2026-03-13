#ifndef YADAAC_BUILDER_IMPL_HPP
#define YADAAC_BUILDER_IMPL_HPP

#include <array>
#include <cstdint>
#include <numeric>
#include <queue>
#include <span>
#include <stack>
#include <stdexcept>
#include <string>
#include <vector>

#include "../base.hpp"
#include "base_allocator.hpp"
#include "state.hpp"

namespace yadaac::detail::construction {

constexpr size_t INSERT_SORT_THRESHOLD = 32;
constexpr size_t ROOT_STATE_IDX = 0U;

struct build_frame {
  uint32_t state_id{};
  uint32_t left{};
  uint32_t right{};
  uint16_t depth{};
};

namespace kpi {

constexpr uint32_t IDX_MASK_PACK = 0x00FFFFFF;

YADAAC_INLINE uint32_t pack(uint32_t index, uint8_t key) {
  // Packed layout: [ key:8 | pattern_index:24 ].
  // This allows sorting by key via normal uint32 ordering.
  return (static_cast<uint32_t>(key) << 24) | (index & IDX_MASK_PACK);
}

YADAAC_INLINE uint32_t unpack_index(uint32_t packed) { return packed & IDX_MASK_PACK; }
YADAAC_INLINE uint8_t unpack_key(uint32_t packed) { return static_cast<uint8_t>(packed >> 24); }

}  // namespace kpi

class builder_impl {
 public:
  builder_impl(const std::span<const std::string> input_patterns)
      : states(allocation::BLOCK_LEN, state{}), patterns(input_patterns), num_of_states{1} {
    if (patterns.empty()) {
      throw std::runtime_error{"No patterns given"};
    }

    if (patterns.size() > MAX_PATTERN_AMOUNT) {
      throw std::runtime_error("Pattern count exceeds 16.7 million limit");
    }

    outputs.reserve(patterns.size());
    states.reserve(patterns.size() * 2);

    build_double_array();
    build_failures();

    states.shrink_to_fit();
    outputs.shrink_to_fit();
  }

  [[nodiscard]] uint32_t get_state_count() const { return num_of_states; }

  std::vector<state> states;
  std::vector<output> outputs;

 private:
  void build_double_array() {
    // KPI stores either raw pattern indices or packed (key,index) values in-place.
    std::vector<uint32_t> kpi(patterns.size());
    std::iota(kpi.begin(), kpi.end(), 0);

    std::stack<build_frame> stack;
    stack.push({ROOT_STATE_IDX, 0, static_cast<uint32_t>(kpi.size()), 0});

    std::vector<uint8_t> labels;
    labels.reserve(ALPHABET_SIZE);

    while (!stack.empty()) {
      auto frame = stack.top();
      stack.pop();

      auto kpi_view = std::span{kpi}.subspan(frame.left, frame.right - frame.left);
      labels.clear();

      uint32_t term_count = partition_and_pack(kpi_view, frame.depth);
      if (term_count > 0) {
        if (term_count > 1) {
          throw std::runtime_error("Duplicate patterns found");
        }

        // Terminators are at the start of the view
        uint32_t patt_idx = kpi::unpack_index(kpi_view[0]);

        outputs.push_back({patt_idx, 0, frame.depth});
        states[frame.state_id].set_output_index(outputs.size());
      }

      // The rest of the view contains patterns that continue deeper.
      auto continuations = kpi_view.subspan(term_count);

      switch (continuations.size()) {
        case 0:  // Nothing to do
          continue;
        case 1: {  // Handle tail optimization (single patterns)
          uint32_t patt_idx = kpi::unpack_index(continuations[0]);
          emit_linear_tail(frame.state_id, frame.depth, patt_idx);
          continue;
        }
        case 2:
          // Natural sort of uint32_t works because Key is in MSB
          if (continuations[0] > continuations[1]) {
            std::swap(continuations[0], continuations[1]);
          }
          break;
        case 3 ... INSERT_SORT_THRESHOLD:
          insertion_sort(continuations);
          break;
        default:
          radix_sort(continuations);
          break;
      }

      collect_unique_labels(continuations, labels);

      // Allocate based on whether we have a single label or multiple
      uint32_t base = (labels.size() == 1) ? allocate_da_node(labels[0]) : allocate_da_node(labels);
      states[frame.state_id].base = base;

      size_t kpi_offset = 0;
      // Absolute range start in the global KPI array for the first child label.
      uint32_t abs_start_idx = frame.left + term_count;
      for (uint8_t c : labels) {
        uint32_t child_idx = base ^ c;
        states[child_idx].set_check(c);
        states[frame.state_id].add_transition_to_filter(c);

        // Determine the range for this label
        // Since continuations are key-sorted, each label occupies one contiguous range.
        size_t range_len = 0;
        while (kpi_offset < continuations.size() &&
               kpi::unpack_key(continuations[kpi_offset]) == c) {
          kpi_offset++;
          range_len++;
        }

        stack.push({child_idx, abs_start_idx, abs_start_idx + static_cast<uint32_t>(range_len),
                    static_cast<uint16_t>(frame.depth + 1)});

        // Advance index for the next sibling
        abs_start_idx += range_len;
      }
    }
  }

  void build_failures() {
    std::queue<uint32_t> q;
    q.push(ROOT_STATE_IDX);

    while (!q.empty()) {
      uint32_t u = q.front();
      q.pop();

      uint32_t filter = states[u].trans_filter;

      // Iterate Children using the Filter
      // The filter creates "buckets" of 32 chars.
      // If bit K is set, we check chars K, K+32, K+64, etc.
      while (filter != 0) {
        int bit = std::countr_zero(filter);
        filter &= ~(1U << bit);

        // If c exists, it must live at bit + (N * 32).
        for (int c = bit; c < static_cast<int>(ALPHABET_SIZE); c += 32) {
          uint32_t child_idx = states[u].base ^ c;

          if (states[child_idx].get_check() != c) {
            continue;
          }

          uint32_t fail = states[u].fail;
          while (u != ROOT_STATE_IDX) {
            if (states[fail].might_have_transition(c)) {
              uint32_t next = states[fail].base ^ c;
              if (states[next].get_check() == c) {
                fail = next;
                break;
              }
            }

            if (fail == ROOT_STATE_IDX) {
              break;
            }
            fail = states[fail].fail;
          }

          states[child_idx].fail = fail;

          uint32_t intrinsic = states[child_idx].get_output_index();
          uint32_t propagated = states[fail].get_output_index();
          // Output index points to the first output of this node; if intrinsic exists it
          // stays head, and we attach the fail-output chain as its parent.
          uint32_t final_out = (intrinsic > 0) ? intrinsic : propagated;

          if (intrinsic > 0 && propagated > 0) {
            outputs[intrinsic - 1].parent = propagated;
          }

          states[child_idx].set_output_index(final_out);

          if (!states[child_idx].is_leaf()) {
            q.push(child_idx);
          }
        }
      }
    }
  }

  YADAAC_INLINE uint32_t allocate_da_node(uint8_t label) {
    num_of_states++;

    uint32_t base = base_allocator.allocate_base(label);
    if (base >= states.size()) {
      extend_array();
    }

    return base;
  }

  YADAAC_INLINE uint32_t allocate_da_node(std::span<const uint8_t> labels) {
    num_of_states += labels.size();

    uint32_t base = base_allocator.allocate_base(labels);
    if (base >= states.size()) {
      extend_array();
    }

    return base;
  }

  YADAAC_INLINE void extend_array() {
    const size_t new_size = states.size() + allocation::BLOCK_LEN;

    if (new_size >= std::numeric_limits<uint32_t>::max()) {
      throw std::runtime_error{"Base index exceeds 4-byte limit"};
    }

    states.resize(new_size, {});
  }

  [[nodiscard]] YADAAC_INLINE uint32_t partition_and_pack(std::span<uint32_t> kpi_view,
                                                          uint32_t depth) const {
    uint32_t term_count = 0;

    for (size_t i = 0; i < kpi_view.size(); ++i) {
      // Even after swaps, each slot still contains an unpackable pattern index in low 24
      // bits.
      uint32_t idx = kpi::unpack_index(kpi_view[i]);

      if (depth >= patterns[idx].size()) {
        // Found a Terminator: Swap it to the 'term_count' slot.
        // Note: The value moving FROM term_count to i is a Continuation
        // (processed in a previous step of this loop), so it is already packed.
        std::swap(kpi_view[i], kpi_view[term_count]);
        term_count++;
      } else {
        // Found a Continuation: Pack the key in-place.
        auto key = static_cast<uint8_t>(patterns[idx][depth]);
        kpi_view[i] = kpi::pack(idx, key);
      }
    }

    return term_count;
  }

  static YADAAC_INLINE void collect_unique_labels(std::span<const uint32_t> continuations,
                                                  std::vector<uint8_t>& labels) {
    uint8_t prev = kpi::unpack_key(continuations[0]);
    labels.push_back(prev);
    for (size_t i = 1; i < continuations.size(); ++i) {
      uint8_t curr = kpi::unpack_key(continuations[i]);
      if (curr != prev) {
        labels.push_back(curr);
        prev = curr;
      }
    }
  }

  YADAAC_INLINE void emit_linear_tail(uint32_t state_id, uint16_t depth, uint32_t pattern_idx) {
    const std::string& pat = patterns[pattern_idx];

    for (size_t d = depth; d < pat.size(); ++d) {
      auto label = static_cast<uint8_t>(pat[d]);

      uint32_t base = allocate_da_node(label);
      states[state_id].base = base;
      states[state_id].add_transition_to_filter(label);

      state_id = base ^ label;
      states[state_id].set_check(label);
    }

    outputs.push_back({pattern_idx, 0, static_cast<uint16_t>(pat.size())});
    states[state_id].set_output_index(outputs.size());
  }

  static YADAAC_INLINE void insertion_sort(std::span<uint32_t> continuations) {
    // Sorting the packed value naturally groups identical characters together
    // because the character is in the most significant bits.
    for (size_t i = 1; i < continuations.size(); ++i) {
      uint32_t temp = continuations[i];
      int j = static_cast<int>(i) - 1;
      while (j >= 0 && continuations[j] > temp) {
        continuations[j + 1] = continuations[j];
        j--;
      }
      continuations[j + 1] = temp;
    }
  }

  static YADAAC_INLINE void radix_sort(std::span<uint32_t> continuations) {
    constexpr int radix_bin = 256;
    std::array<uint32_t, radix_bin> counts{};

    std::array<std::span<uint32_t>::iterator, radix_bin + 1> heads_storage{};
    std::span<uint32_t>::iterator* heads = heads_storage.data() + 1;

    for (uint32_t c : continuations) {
      counts[kpi::unpack_key(c)]++;
    }

    heads[-1] = heads[0] = continuations.begin();
    for (int i = 1; i < radix_bin; ++i) {
      heads[i] = heads[i - 1] + counts[i - 1];
    }

    for (int i = 0; i < radix_bin; ++i) {
      auto bucket_end = heads[i - 1] + counts[i];

      // Skip empty or already-sorted buckets
      if (heads[i] == bucket_end) {
        continue;
      }

      while (heads[i] != bucket_end) {
        uint8_t tag_key = kpi::unpack_key(*heads[i]);
        std::iter_swap(heads[i], heads[tag_key]);
        heads[tag_key]++;
      }
    }
  }

  const std::span<const std::string> patterns;
  allocation::base_allocator base_allocator;
  uint32_t num_of_states{};
};

}  // namespace yadaac::detail::construction

#endif  // !YADAAC_BUILDER_IMPL_HPP
