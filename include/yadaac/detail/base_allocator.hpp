#ifndef YADAAC_BASE_ALLOCATOR_HPP
#define YADAAC_BASE_ALLOCATOR_HPP

#include <array>
#include <cstdint>
#include <limits>
#include <span>

#include "yadaac/base.hpp"

namespace yadaac::detail::allocation {

constexpr uint32_t BLOCK_LEN = 256;  // 256 bits
constexpr uint32_t NUM_FREE = 16;    // Ring size
constexpr uint32_t RING_MASK = NUM_FREE - 1;

constexpr uint8_t WORDS_PER_BLOCK = 4;  // 256 / 64
constexpr uint32_t WORD_SHIFT = 2;
constexpr uint32_t WORD_MASK = 3;

struct alignas(64) allocator_block {
  std::array<uint64_t, WORDS_PER_BLOCK> base_mask;
  std::array<uint64_t, WORDS_PER_BLOCK> index_mask;
  void reset() {
    base_mask.fill(0);
    index_mask.fill(0);
  }
};

class base_allocator {
 public:
  base_allocator() : num_blocks{0}, label_cursor_cache{0} {
    push_block();  // Initialize with Block 0

    // Reserve for root
    --free_counts[0];
    blocks[0].base_mask[0] |= 1ULL;
    blocks[0].index_mask[0] |= 1ULL;
  }

  YADAAC_INLINE uint32_t allocate_base(std::span<const uint8_t> labels) {
    uint32_t min_valid_block = (num_blocks > NUM_FREE) ? (num_blocks - NUM_FREE) : 0;
    uint32_t cursor = min_valid_block << WORD_SHIFT;

    // We must satisfy the intersection of all labels. Therefore, we start
    // at the maximum cursor position among all involved labels.
    for (auto label : labels) {
      cursor = std::max(cursor, label_cursor_cache[label]);
    }

    while (true) {
      uint32_t block_idx = cursor >> WORD_SHIFT;
      // Ring buffer reuses storage modulo NUM_FREE; logical block index keeps monotonic progress.
      uint32_t ring_idx = block_idx & RING_MASK;

      if (block_idx >= num_blocks) push_block();

      if (free_counts[ring_idx] < labels.size()) {
        cursor = (block_idx + 1) << WORD_SHIFT;
        continue;
      }

      uint8_t start_word = cursor & WORD_MASK;
      // Search inside this 256-slot block for one base that can host all labels at once.
      uint32_t block_offset = scan_block_for_labels(blocks[ring_idx], start_word, labels);

      if (block_offset != std::numeric_limits<uint32_t>::max()) {
        free_counts[ring_idx] -= labels.size();
        return (block_idx * BLOCK_LEN) + block_offset;
      }

      // We just increment the cursor. We do not update label_block_cache here
      // because we don't know specifically which label caused the collision.
      // Also, a label can have free space in previous blocks, so we don't want
      // to discard them if some other label caused the cursor to jump to a higher block.
      cursor = (block_idx + 1) << WORD_SHIFT;
    }
  }

  YADAAC_INLINE uint32_t allocate_base(uint8_t label) {
    uint32_t min_valid_block = (num_blocks > NUM_FREE) ? (num_blocks - NUM_FREE) : 0;
    uint32_t min_valid_cursor = min_valid_block << WORD_SHIFT;
    uint32_t cursor = std::max(label_cursor_cache[label], min_valid_cursor);

    while (true) {
      uint32_t block_idx = cursor >> WORD_SHIFT;
      // Ring buffer reuses storage modulo NUM_FREE; logical block index keeps monotonic progress.
      uint32_t ring_idx = block_idx & RING_MASK;

      if (block_idx >= num_blocks) {
        push_block();
      }

      if (free_counts[ring_idx] == 0) {
        // Jump cursor to the start of the next block
        cursor = (block_idx + 1) << WORD_SHIFT;

        // We know this block is dead for everyone, so save this label future work.
        label_cursor_cache[label] = cursor;
        continue;
      }

      uint32_t word_idx = cursor & WORD_MASK;
      uint32_t block_offset = scan_block_for_label(blocks[ring_idx], word_idx, label);

      if (block_offset != std::numeric_limits<uint32_t>::max()) {
        uint32_t success_word = block_offset >> 6;

        // Keep scanning from the successful word next time to exploit local free space.
        label_cursor_cache[label] = (block_idx << WORD_SHIFT) | success_word;
        free_counts[ring_idx]--;

        return (block_idx * BLOCK_LEN) + block_offset;
      }

      cursor = (block_idx + 1) << WORD_SHIFT;

      // If we failed, this specific word is effectively full for this label.
      // Update the cache so we never scan this word for this label again.
      label_cursor_cache[label] = cursor;
    }
  }

 private:
  YADAAC_INLINE uint32_t scan_block_for_labels(allocator_block& block, uint8_t start_word,
                                               std::span<const uint8_t> labels) {
    for (uint8_t w = start_word; w < WORDS_PER_BLOCK; ++w) {
      uint64_t valid = ~block.base_mask[w];

      // If this word is full, try the next one.
      if (valid == 0) continue;

      uint32_t curr_target_idx = std::numeric_limits<uint32_t>::max();
      uint64_t curr_child_mask = 0;

      for (uint8_t c : labels) {
        uint8_t hi = c >> 6;
        uint8_t target_idx = w ^ hi;

        if (target_idx != curr_target_idx) {
          curr_target_idx = target_idx;
          curr_child_mask = block.index_mask[target_idx];

          // If the child word is completely full, we can't place anything.
          if (curr_child_mask == std::numeric_limits<uint64_t>::max()) {
            valid = 0;
            break;
          }
        }

        // xor_permute projects occupied child bits back to incompatible base bits for this label.
        valid &= ~xor_permute(curr_child_mask, c & 63);

       if (valid == 0) break;
      }

      if (valid != 0) {
        // If we get here, any remaining bit in valid satisfies ALL labels.
        uint32_t bit_idx = std::countr_zero(valid);

        // Update Base Mask
        block.base_mask[w] |= (1ULL << bit_idx);

        // Update Index Masks
        for (uint8_t c : labels) {
          uint8_t target_idx = w ^ (c >> 6);
          uint8_t child_rel_bit = (c & 63) ^ bit_idx;
          block.index_mask[target_idx] |= (1ULL << child_rel_bit);
        }

        return ((w << 6) | bit_idx);
      }
    }

    return std::numeric_limits<uint32_t>::max();
  }

  YADAAC_INLINE uint32_t scan_block_for_label(allocator_block& block, uint8_t start_word, uint8_t label) {
    // Pre-calculate label masks once per block
    uint8_t label_high = label >> 6;
    uint8_t label_low = label & 63;

    for (uint8_t w = start_word; w < WORDS_PER_BLOCK; ++w) {
      uint64_t valid = ~block.base_mask[w];

      // Skip if word is full
      if (valid == 0) continue;

      uint8_t target_word_idx = w ^ label_high;
      uint64_t child_occupied = block.index_mask[target_word_idx];

      // Convert child occupancy mask into invalid base-bit candidates for this label.
      valid &= ~xor_permute(child_occupied, label_low);

      if (valid != 0) {
        uint32_t bit_idx = std::countr_zero(valid);

        block.base_mask[w] |= (1ULL << bit_idx);
        uint8_t child_bit = bit_idx ^ label_low;
        block.index_mask[target_word_idx] |= (1ULL << child_bit);

        return ((w << 6) | bit_idx);
      }
    }

    return std::numeric_limits<uint32_t>::max();
  }

  YADAAC_INLINE void push_block() {
    uint32_t ring_idx = num_blocks & RING_MASK;

    free_counts[ring_idx] = BLOCK_LEN;
    blocks[ring_idx].reset();

    num_blocks++;
  }

  YADAAC_INLINE uint64_t xor_permute(uint64_t mask, uint8_t shift) const {
    // Reindexes bits by XOR with `shift`: out[i] = in[i ^ shift].
    // This is used to map child occupancy constraints onto base-bit positions.
    // Performs a swap of bits separated by 'dist' if the corresponding bit in 'shift' is set.
    const auto delta_swap = [&](uint64_t x, uint64_t k_mask, int dist, int shift_bit_index) {
      // Create a mask of all 1s if the bit is set, 0s otherwise
      uint64_t enabled = -((uint64_t)((shift >> shift_bit_index) & 1));

      // Calculate the swap delta
      // x ^ (x >> dist) finds bits that are different between position i and i+dist
      uint64_t t = ((x >> dist) ^ x) & k_mask & enabled;

      // Apply the delta to swap the bits
      return x ^ t ^ (t << dist);
    };

    mask = delta_swap(mask, 0x5555555555555555ULL, 1, 0);
    mask = delta_swap(mask, 0x3333333333333333ULL, 2, 1);
    mask = delta_swap(mask, 0x0F0F0F0F0F0F0F0FULL, 4, 2);
    mask = delta_swap(mask, 0x00FF00FF00FF00FFULL, 8, 3);
    mask = delta_swap(mask, 0x0000FFFF0000FFFFULL, 16, 4);
    mask = delta_swap(mask, 0x00000000FFFFFFFFULL, 32, 5);

    return mask;
  }

  size_t count_vacant_in_block(const allocator_block& block) const {
    size_t occupied_count = 0;
    for (uint64_t word : block.index_mask) {
      occupied_count += std::popcount(word);
    }
    return BLOCK_LEN - occupied_count;
  }

 private:
  uint32_t num_blocks = 0;

  // Caches the next search position for each label (0-255).
  // This allows us to resume searching exactly where we left off, skipping
  // blocks and words we've already proven are full for a specific label.
  std::array<uint32_t, 256> label_cursor_cache;
  std::array<uint16_t, NUM_FREE> free_counts;
  std::array<allocator_block, NUM_FREE> blocks;
};

}  // namespace yadaac::detail::allocation

#endif  // !YADAAC_BASE_ALLOCATOR_HPP
