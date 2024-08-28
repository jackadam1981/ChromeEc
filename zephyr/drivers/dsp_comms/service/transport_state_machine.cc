/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "cros/dsp/service/transport_state_machine.hh"

#include <algorithm>

namespace cros::dsp::service {

pw::Status TransportStateMachine::SetNewState(
    TransportStateMachine::State new_state) {
  if (state_ == new_state) {
    return pw::OkStatus();
  }
  for (auto& transition : allowed_transitions_) {
    if (transition.from == state_ && transition.to == new_state) {
      state_ = new_state;
      return pw::OkStatus();
    }
  }
  return pw::Status::InvalidArgument();
}

bool TransportStateMachine::StateAnyOf(
    std::vector<TransportStateMachine::State> states) const {
  return std::any_of(states.cbegin(),
                     states.cend(),
                     [this](State other) -> bool { return state_ == other; });
}

}  // namespace cros::dsp::service
