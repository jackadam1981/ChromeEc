/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#pragma once

#include <vector>

#include "pw_status/status.h"

namespace cros::dsp::service {

/**
 * @brief Provide transitions for the various service states.
 *
 * The following states are supported:
 * - IDLE: the service is ready to receive a status and/or a response
 * - HAS_STATUS: the service has a status which needs to be sent to the client.
 * - HAS_STATUS_AND_RESPONSE: the service has both a status and a response that need to be sent to
 * the client. The status will be sent first followed by a response (since the status includes the
 * response size).
 * - HAS_RESPONSE: the client already received the status with the response size and should be
 * requesting a read for the response.
 */
class TransportStateMachine {
 public:
  TransportStateMachine() = default;

  /** Current state of the service. */
  enum State {
    /**
     * The service is idle. We can either store a status bit or a response + response_ready status.
     */
    IDLE,

    /**
     * The service has a pending status bit set. A response may still be added, but the client was
     * notified that there's a pending status.
     */
    HAS_STATUS,

    /**
     * The service has both a pending status and a response. The client was notified of the pending
     * status and should schedule a read for the status followed by a read for the response.
     */
    HAS_STATUS_AND_RESPONSE,

    /**
     * The service has a pending response. The client MUST have already read the status and know the
     * response length. The next read should be for the response.
     */
    HAS_RESPONSE,
  };

  /**
   * @brief State transition
   *
   * Update the state (assuming a valid state transition).
   *
   * @param new_state The desired new state.
   * @return pw::OkStatus() on success or pw::Status::InvalidArgument() if the transition is
   * invalid.
   */
  pw::Status SetNewState(State new_state);

  /**
   * @return The current state.
   */
  inline State current_state() const { return state_; }

  /**
   * @brief Short hand function to check if the current_state() is one of the provided states.
   *
   * @param states The states to check.
   * @return true if the current_state() matches 1 of the provided states.
   */
  bool StateAnyOf(std::vector<State> states) const;

 private:
  struct AllowedTransitions {
    State from;
    State to;
  };
  const AllowedTransitions allowed_transitions_[7] = {
      {.from = State::IDLE, .to = State::HAS_STATUS},
      {.from = State::IDLE, .to = State::HAS_STATUS_AND_RESPONSE},
      {.from = State::HAS_STATUS, .to = State::IDLE},
      {.from = State::HAS_STATUS, .to = State::HAS_STATUS_AND_RESPONSE},
      {.from = State::HAS_STATUS_AND_RESPONSE, .to = State::HAS_RESPONSE},
      {.from = State::HAS_RESPONSE, .to = State::HAS_STATUS},
      {.from = State::HAS_RESPONSE, .to = State::IDLE},
  };
  State state_ = State::IDLE;
};

}  // namespace cros::dsp::service
