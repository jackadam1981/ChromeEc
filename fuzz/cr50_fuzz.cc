/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 *
 * Fuzzer for the TPM2 and vendor specific Cr50 commands.
 */
#include <unistd.h>

#include <cstdlib>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include <src/libfuzzer/libfuzzer_macro.h>
#include <src/mutator.h>

#include "fuzz/PinweaverModel.h"
#include "fuzz/cr50_fuzz.pb.h"

extern "C" {
#define __stdlib_compat(...)
#include "fuzz_config.h"
#include "nvmem.h"
#include "nvmem_vars.h"
#include "persistence.h"
#include "pinweaver.h"
}

#define NVMEM_TPM_SIZE ((sizeof((struct nvmem_partition *)0)->buffer) \
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

/* Prevent this from being stack allocated. */
static uint8_t buffer_[PW_MAX_MESSAGE_SIZE];
static PinweaverModel pinweaver_;

extern "C" void run_test(void) {
  protobuf_mutator::Mutator::RegisterCustomMutation(
      fuzz::SubAction::descriptor()->FindFieldByName("pinweaver"),
      [](google::protobuf::Message* message) {
        if (message->GetDescriptor() != fuzz::SubAction::descriptor())
          return;
        fuzz::SubAction* sub_action = dynamic_cast<fuzz::SubAction*>(message);
        if (!sub_action->has_pinweaver())
          return;
        size_t num_bytes =
            pinweaver_.SerializePinweaver(sub_action->pinweaver(), buffer_);
        sub_action->mutable_random_bytes()->set_value(buffer_, num_bytes);
      }
  );
}

void apply_random_bytes(const fuzz::RandomBytes& random_bytes) {
  const auto& value = random_bytes.value();
  if (value.size() >= ARRAY_SIZE(buffer_)) {
    memcpy(buffer_, value.data(), ARRAY_SIZE(buffer_));
  } else {
    memcpy(buffer_, value.data(), value.size());
    memset(buffer_ + value.size(), 0, ARRAY_SIZE(buffer_) - value.size());
  }
}

extern "C" size_t LLVMFuzzerCustomMutator(
    uint8_t* data, size_t size, size_t max_size, unsigned int seed) {
  using protobuf_mutator::libfuzzer::CustomProtoMutator;
  fuzz::FuzzerInput input;
  return CustomProtoMutator(false /*use_binary*/, data, size, max_size, seed,
                            &input);
}
DEFINE_CUSTOM_PROTO_CROSSOVER_IMPL(false, fuzz::FuzzerInput)

extern "C" int test_fuzz_one_input(const uint8_t *data, unsigned int size) {
  fuzz::FuzzerInput input;

  if (!LoadProtoInput(false, data, size, &input)) {
    return 0;
  }

  memset(__host_flash, 0xff, sizeof(__host_flash));
  srand(0);
  memset(buffer_, 0, sizeof(buffer_));
  pinweaver_.Reset();
  for (const fuzz::SubAction& action : input.sub_actions()) {
    switch(action.sub_action_case()) {
    case fuzz::SubAction::kRandomBytes:
      apply_random_bytes(action.random_bytes());
      pinweaver_.SendBuffer(buffer_);
      break;
    case fuzz::SubAction::kPinweaver:
      pinweaver_.ApplyPinweaver(action.pinweaver(), buffer_);
      break;
    case fuzz::SubAction::SUB_ACTION_NOT_SET:
      break;
    }
  }
  return 0;
}
