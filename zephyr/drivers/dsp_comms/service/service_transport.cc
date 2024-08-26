#include "cros/dsp/internal/service_transport.hh"

namespace cros::dsp::service {

pw::Status
TransportStateMachine::SetNewState(TransportStateMachine::State new_state) {
  if (state_ == new_state) {
    return pw::OkStatus();
  }
  for (auto &transition : allowed_transitions_) {
    if (transition.from == state_ && transition.to == new_state) {
      state_ = new_state;
      return pw::OkStatus();
    }
  }
  return pw::Status::InvalidArgument();
}

bool TransportStateMachine::StateAnyOf(
    std::vector<TransportStateMachine::State> states) const {
  return std::any_of(states.cbegin(), states.cend(),
                     [this](State other) -> bool { return state_ == other; });
}

} // namespace cros::dsp::service