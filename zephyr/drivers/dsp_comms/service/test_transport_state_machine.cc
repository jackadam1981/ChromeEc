/* Copyright 2024 The ChromiumOS Authors
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <gtest/gtest.h>

#include "cros/dsp/service/transport_state_machine.hh"
#include "pw_status/status.h"

#define EXPECT_OK(check) EXPECT_EQ(pw::OkStatus(), check);
#define ASSERT_OK(check) ASSERT_EQ(pw::OkStatus(), check);

namespace cros::dsp::service {

namespace {

TEST(TransportStateMachine, CheckTransitionsFromIdle) {
  {
    TransportStateMachine state_machine;
    EXPECT_OK(state_machine.SetNewState(TransportStateMachine::IDLE));
    EXPECT_EQ(state_machine.current_state(), TransportStateMachine::IDLE);
  }
  {
    TransportStateMachine state_machine;
    EXPECT_OK(state_machine.SetNewState(TransportStateMachine::HAS_STATUS));
    EXPECT_EQ(state_machine.current_state(), TransportStateMachine::HAS_STATUS);
  }
  {
    TransportStateMachine state_machine;
    EXPECT_OK(state_machine.SetNewState(
        TransportStateMachine::HAS_STATUS_AND_RESPONSE));
    EXPECT_EQ(state_machine.current_state(),
              TransportStateMachine::HAS_STATUS_AND_RESPONSE);
  }
  {
    TransportStateMachine state_machine;
    EXPECT_EQ(pw::Status::InvalidArgument(),
              state_machine.SetNewState(TransportStateMachine::HAS_RESPONSE));
    EXPECT_EQ(state_machine.current_state(), TransportStateMachine::IDLE);
  }
}

TEST(TransportStateMachine, CheckTransitionFromHasStatus) {
  {
    TransportStateMachine state_machine;
    ASSERT_OK(state_machine.SetNewState(TransportStateMachine::HAS_STATUS));
    EXPECT_OK(state_machine.SetNewState(TransportStateMachine::IDLE));
    EXPECT_EQ(state_machine.current_state(), TransportStateMachine::IDLE);
  }
  {
    TransportStateMachine state_machine;
    ASSERT_OK(state_machine.SetNewState(TransportStateMachine::HAS_STATUS));
    EXPECT_OK(state_machine.SetNewState(TransportStateMachine::HAS_STATUS));
    EXPECT_EQ(state_machine.current_state(), TransportStateMachine::HAS_STATUS);
  }
  {
    TransportStateMachine state_machine;
    ASSERT_OK(state_machine.SetNewState(TransportStateMachine::HAS_STATUS));
    EXPECT_OK(state_machine.SetNewState(
        TransportStateMachine::HAS_STATUS_AND_RESPONSE));
    EXPECT_EQ(state_machine.current_state(),
              TransportStateMachine::HAS_STATUS_AND_RESPONSE);
  }
  {
    TransportStateMachine state_machine;
    ASSERT_OK(state_machine.SetNewState(TransportStateMachine::HAS_STATUS));
    EXPECT_EQ(pw::Status::InvalidArgument(),
              state_machine.SetNewState(TransportStateMachine::HAS_RESPONSE));
    EXPECT_EQ(state_machine.current_state(), TransportStateMachine::HAS_STATUS);
  }
}

TEST(TransportStateMachine, CheckTransitionFromHasStatusAndResponse) {
  {
    TransportStateMachine state_machine;
    ASSERT_OK(state_machine.SetNewState(
        TransportStateMachine::HAS_STATUS_AND_RESPONSE));
    EXPECT_EQ(pw::Status::InvalidArgument(),
              state_machine.SetNewState(TransportStateMachine::IDLE));
    EXPECT_EQ(state_machine.current_state(),
              TransportStateMachine::HAS_STATUS_AND_RESPONSE);
  }
  {
    TransportStateMachine state_machine;
    ASSERT_OK(state_machine.SetNewState(
        TransportStateMachine::HAS_STATUS_AND_RESPONSE));
    EXPECT_EQ(pw::Status::InvalidArgument(),
              state_machine.SetNewState(TransportStateMachine::HAS_STATUS));
    EXPECT_EQ(state_machine.current_state(),
              TransportStateMachine::HAS_STATUS_AND_RESPONSE);
  }
  {
    TransportStateMachine state_machine;
    ASSERT_OK(state_machine.SetNewState(
        TransportStateMachine::HAS_STATUS_AND_RESPONSE));
    EXPECT_OK(state_machine.SetNewState(
        TransportStateMachine::HAS_STATUS_AND_RESPONSE));
    EXPECT_EQ(state_machine.current_state(),
              TransportStateMachine::HAS_STATUS_AND_RESPONSE);
  }
  {
    TransportStateMachine state_machine;
    ASSERT_OK(state_machine.SetNewState(
        TransportStateMachine::HAS_STATUS_AND_RESPONSE));
    EXPECT_OK(state_machine.SetNewState(TransportStateMachine::HAS_RESPONSE));
    EXPECT_EQ(state_machine.current_state(),
              TransportStateMachine::HAS_RESPONSE);
  }
}

TEST(TransportStateMachine, CheckTransitionFromHasResponse) {
  {
    TransportStateMachine state_machine;
    ASSERT_OK(state_machine.SetNewState(
        TransportStateMachine::HAS_STATUS_AND_RESPONSE));
    ASSERT_OK(state_machine.SetNewState(TransportStateMachine::HAS_RESPONSE));
    EXPECT_OK(state_machine.SetNewState(TransportStateMachine::IDLE));
    EXPECT_EQ(state_machine.current_state(), TransportStateMachine::IDLE);
  }
  {
    TransportStateMachine state_machine;
    ASSERT_OK(state_machine.SetNewState(
        TransportStateMachine::HAS_STATUS_AND_RESPONSE));
    ASSERT_OK(state_machine.SetNewState(TransportStateMachine::HAS_RESPONSE));
    EXPECT_OK(state_machine.SetNewState(TransportStateMachine::HAS_STATUS));
    EXPECT_EQ(state_machine.current_state(), TransportStateMachine::HAS_STATUS);
  }
  {
    TransportStateMachine state_machine;
    ASSERT_OK(state_machine.SetNewState(
        TransportStateMachine::HAS_STATUS_AND_RESPONSE));
    ASSERT_OK(state_machine.SetNewState(TransportStateMachine::HAS_RESPONSE));
    EXPECT_EQ(pw::Status::InvalidArgument(),
              state_machine.SetNewState(
                  TransportStateMachine::HAS_STATUS_AND_RESPONSE));
    EXPECT_EQ(state_machine.current_state(),
              TransportStateMachine::HAS_RESPONSE);
  }
  {
    TransportStateMachine state_machine;
    ASSERT_OK(state_machine.SetNewState(
        TransportStateMachine::HAS_STATUS_AND_RESPONSE));
    ASSERT_OK(state_machine.SetNewState(TransportStateMachine::HAS_RESPONSE));
    EXPECT_OK(state_machine.SetNewState(TransportStateMachine::HAS_RESPONSE));
    EXPECT_EQ(state_machine.current_state(),
              TransportStateMachine::HAS_RESPONSE);
  }
}

TEST(TransportStateMachine, CheckStateAnyOf) {
  TransportStateMachine state_machine;

  EXPECT_TRUE(state_machine.StateAnyOf({TransportStateMachine::IDLE}));
  EXPECT_TRUE(state_machine.StateAnyOf(
      {TransportStateMachine::HAS_RESPONSE, TransportStateMachine::IDLE}));
  EXPECT_FALSE(state_machine.StateAnyOf({TransportStateMachine::HAS_RESPONSE,
                                         TransportStateMachine::HAS_STATUS}));
}

}  // namespace
}  // namespace cros::dsp::service
