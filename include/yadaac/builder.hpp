#ifndef YADAAC_BUILDER_HPP
#define YADAAC_BUILDER_HPP

#include <cstdint>
#include <string_view>
#include <vector>

#include "detail/builder_impl.hpp"

namespace yadaac {

class daac_stream;

class daac {
 public:
  explicit daac(const std::vector<std::string>& patterns) : daac(detail::construction::builder_impl(patterns)) {}

  inline daac_stream make_stream(std::string_view haystack) const;

  inline uint32_t num_of_states() const { return number_of_states; }
  inline uint32_t num_of_transitions() const { return states.size(); }

 private:
  daac(detail::construction::builder_impl&& b)
      : states(std::move(b.states)), outputs(std::move(b.outputs)), number_of_states(b.get_state_count()) {}

 private:
  friend class daac_stream;
  const std::vector<detail::state> states;
  const std::vector<detail::output> outputs;
  const uint32_t number_of_states;
};

}  // namespace yadaac

#endif  // !YADAAC_BUILDER_HPP
