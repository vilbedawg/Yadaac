#ifndef YADAAC_STATE_HPP
#define YADAAC_STATE_HPP

#include <cstdint>

namespace yadaac::detail {

struct state {
  uint32_t base{};
  uint32_t fail{};
  uint32_t output_and_check{};
  uint32_t trans_filter{};

  [[nodiscard]] uint8_t get_check() const { return output_and_check & 0xFF; }

  [[nodiscard]] uint32_t get_output_index() const { return output_and_check >> 8; }

  void set_check(uint8_t c) { output_and_check = (output_and_check & 0xFFFFFF00) | c; }

  void set_output_index(uint32_t idx) {
    output_and_check = (output_and_check & 0x000000FF) | (idx << 8);
  }

  void add_transition_to_filter(uint8_t c) { trans_filter |= (1U << (c & 31)); }

  [[nodiscard]] bool might_have_transition(uint8_t c) const {
    return trans_filter & (1U << (c & 31));
  }

  [[nodiscard]] bool is_leaf() const { return trans_filter == 0; }
};

struct output {
  uint32_t value{};
  uint32_t parent{};
  uint16_t length{};
};

}  // namespace yadaac::detail

#endif  // !YADAAC_STATE_HPP
