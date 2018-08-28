// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fuzzer for the TPM2 and vendor specific Cr50 commands.

#include <unistd.h>

#include <cstdint>
#include <cstring>
#include <unordered_map>
#include <vector>

#include <src/libfuzzer/libfuzzer_macro.h>
#include <src/mutator.h>

#include "fuzz/pinweaver_model.h"
#include "fuzz/cr50_fuzz.pb.h"
#include "fuzz/span.h"

extern "C" {
#define HIDE_EC_STDLIB
#include "fuzz_config.h"
#include "nvmem.h"
#include "nvmem_vars.h"
#include "persistence.h"
#include "pinweaver.h"
}

#define NVMEM_TPM_SIZE ((sizeof((nvmem_partition *)0)->buffer) \
  - NVMEM_CR50_SIZE)

using protobuf_mutator::libfuzzer::LoadProtoInput;

extern "C" uint32_t nvmem_user_sizes[NVMEM_NUM_USERS] = {
  NVMEM_TPM_SIZE,
  NVMEM_CR50_SIZE
};

extern "C" void rand_bytes(void *buffer, size_t len) {
  size_t x = 0;

  for (; x < len; ++x)
    ((uint8_t *)buffer)[x] = rand();
}

extern "C" void get_storage_seed(void *buf, size_t *len) {
  memset(buf, 0x77, *len);
}

extern "C" void run_test(void) {
  REGISTER_PROTO_FIELD_MUTATOR(
      fuzz::SubAction, pinweaver,
      [](google::protobuf::Message* message) {
        std::vector<uint8_t> buffer(PW_MAX_MESSAGE_SIZE, 0);
        auto* pinweaver_model = PinweaverModel::Get();
        if (pinweaver_model == nullptr)
          return;
        if (message->GetDescriptor() != fuzz::SubAction::descriptor())
          return;
        fuzz::SubAction* sub_action = dynamic_cast<fuzz::SubAction*>(message);
        if (!sub_action->has_pinweaver())
          return;
        size_t num_bytes =
            pinweaver_model->SerializePinweaver(
                sub_action->pinweaver(),
                fuzz::span<uint8_t>(buffer.data(), buffer.size()));
        sub_action->mutable_random_bytes()->set_value(buffer.data(), num_bytes);
      }
  );
}

void apply_random_bytes(const fuzz::RandomBytes& random_bytes,
                        fuzz::span<uint8_t> buffer) {
  const auto& value = random_bytes.value();
  buffer.FillWith(
      fuzz::span<const uint8_t>((const uint8_t*)value.data(), value.size()), 0);
}

DEFINE_CUSTOM_PROTO_MUTATOR_IMPL(false, fuzz::FuzzerInput)
DEFINE_CUSTOM_PROTO_CROSSOVER_IMPL(false, fuzz::FuzzerInput)

extern "C" int test_fuzz_one_input(const uint8_t *data, unsigned int size) {
  PinweaverModel pinweaver_model;
  fuzz::FuzzerInput input;

  if (!LoadProtoInput(false, data, size, &input)) {
    return 0;
  }

  memset(__host_flash, 0xff, sizeof(__host_flash));
  srand(0);
  std::vector<uint8_t> buffer(PW_MAX_MESSAGE_SIZE, 0);
  fuzz::span<uint8_t> buffer_view(buffer.data(), buffer.size());
  for (const fuzz::SubAction& action : input.sub_actions()) {
    switch(action.sub_action_case()) {
    case fuzz::SubAction::kRandomBytes:
      apply_random_bytes(action.random_bytes(), buffer_view);
      pinweaver_model.SendBuffer(buffer_view);
      break;
    case fuzz::SubAction::kPinweaver:
      pinweaver_model.ApplyPinweaver(action.pinweaver(), buffer_view);
      break;
    case fuzz::SubAction::SUB_ACTION_NOT_SET:
      break;
    }
  }
  return 0;
}
