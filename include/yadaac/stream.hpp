#ifndef YADAAC_STREAM_HPP
#define YADAAC_STREAM_HPP

#include <span>
#include <string_view>

#include "base.hpp"
#include "yadaac/builder.hpp"

namespace yadaac {

struct match {
  size_t start;
  size_t end;
  uint32_t value;
};

class daac_stream {
 public:
  using value_type = match;

  inline daac_stream(const daac* pma, std::string_view haystack)
      : states(pma->states),
        outputs(pma->outputs),
        text_curr(haystack.data()),
        text_end(haystack.data() + haystack.size()) {}

  YADAAC_INLINE bool consume(match& out) {
    while (true) {
      if (pending_output != 0) {
        const auto& output = outputs[pending_output - 1];
        pending_output = output.parent;
        out.start = pos - output.length;
        out.end = pos;
        out.value = output.value;
        return true;
      }

      if (text_curr == text_end) return false;

      uint8_t c = static_cast<uint8_t>(*text_curr++);
      pos++;

      while (true) {
        if (states[state_idx].might_have_transition(c)) {
          uint32_t next_node = states[state_idx].base ^ c;
          if (states[next_node].get_check() == c) {
            state_idx = next_node;
            break;
          }
        }

        if (state_idx == 0) break;

        state_idx = states[state_idx].fail;
      }

      pending_output = states[state_idx].get_output_index();
    }
  }

 private:
  const std::span<const detail::state> states;
  const std::span<const detail::output> outputs;
  const char* text_curr;
  const char* text_end;
  uint32_t pos = 0;
  uint32_t state_idx = 0;
  uint32_t pending_output = 0;
};

daac_stream daac::make_stream(std::string_view haystack) const { return {this, haystack}; }

}  // namespace yadaac

#endif

