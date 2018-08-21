/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "PinweaverModel.h"

extern "C" {
#include <dcrypto.h>
}

namespace {

size_t assign_pw_field_from_bytes(
    const std::string bytes, uint8_t *destination, size_t dest_size) {
  if (bytes.size() >= dest_size) {
    memcpy(destination, bytes.data(), dest_size);
    return dest_size;
  } else {
    memcpy(destination, bytes.data(), bytes.size());
    memset(destination + bytes.size(), 0, dest_size - bytes.size());
    return bytes.size();
  }
}

struct pw_request_t* SerializeCommon(const fuzz::PinWeaver& pinweaver,
                                     uint8_t* buffer_) {
  struct pw_request_t* request = (struct pw_request_t*)buffer_;
  request->header.version = pinweaver.version();
  return request;
}

}  // namespace

/******************************************************************************/
/* Public member functions. */
/******************************************************************************/

void PinweaverModel::SendBuffer(uint8_t* buffer_) {
  struct pw_request_t* request = (struct pw_request_t*)buffer_;
  struct pw_response_t* response = (struct pw_response_t*)buffer_;
  pw_handle_request(&merkle_tree_, request, response);
}

size_t PinweaverModel::SerializePinweaver(const fuzz::PinWeaver& pinweaver,
                                          uint8_t* buffer_) {
  switch(pinweaver.request_case()) {
    case fuzz::PinWeaver::kResetTree:
      return SerializeResetTree(pinweaver, buffer_);
    case fuzz::PinWeaver::kInsertLeaf:
      return SerializeInsertLeaf(pinweaver, buffer_);
    case fuzz::PinWeaver::kRemoveLeaf:
      return SerializeRemoveLeaf(pinweaver, buffer_);
    case fuzz::PinWeaver::REQUEST_NOT_SET:
      break;
  }
  return 0;
}

void PinweaverModel::ApplyPinweaver(const fuzz::PinWeaver& pinweaver,
                                    uint8_t* buffer_) {
  SerializePinweaver(pinweaver, buffer_);
  std::unique_ptr<struct leaf_data> metadata;

  const struct pw_request_t* request = (const struct pw_request_t*)buffer_;
  struct pw_response_t* response = (struct pw_response_t*)buffer_;

  if (pinweaver.request_case() == fuzz::PinWeaver::kInsertLeaf) {
    metadata = std::unique_ptr<struct leaf_data>(new struct leaf_data());
    memcpy(&metadata->insert_leaf_, &request->data.insert_leaf,
           sizeof(request->data.insert_leaf));
  }

  pw_handle_request(&merkle_tree_, request, response);
  if (response->header.result_code != EC_SUCCESS) {
    return;
  }

  switch(pinweaver.request_case()) {
    case fuzz::PinWeaver::kResetTree:
      HandleResetTree(pinweaver, buffer_);
      break;
    case fuzz::PinWeaver::kInsertLeaf:
      HandleInsertLeaf(pinweaver, buffer_, std::move(metadata));
      break;
    case fuzz::PinWeaver::kRemoveLeaf:
      HandleRemoveLeaf(pinweaver, buffer_);
      break;
    case fuzz::PinWeaver::REQUEST_NOT_SET:
      return;
  }
}

void PinweaverModel::Reset() {
  memset(&merkle_tree_, 0, sizeof(merkle_tree_));
  leaf_metadata_.clear();
  hash_tree_.clear();
};

/******************************************************************************/
/* Private static fields. */
/******************************************************************************/

constexpr uint8_t PinweaverModel::EMPTY_SUBTREE;

/******************************************************************************/
/* Private member functions. */
/******************************************************************************/

void PinweaverModel::GetHmac(const std::string& fuzzer_hmac, uint64_t label,
                             uint8_t hmac[PW_HASH_SIZE]) {
  if (!fuzzer_hmac.empty()) {
    assign_pw_field_from_bytes(fuzzer_hmac, hmac, PW_HASH_SIZE);
    return;
  }

  auto itr = hash_tree_.find(masked_label(label, 0));
  if (itr == hash_tree_.end()) {
    memset(hmac, 0, PW_HASH_SIZE);
  } else {
    memcpy(hmac, itr->second.data(), PW_HASH_SIZE);
  }
}

size_t PinweaverModel::GetPathHashes(
    const std::string& fuzzer_hashes, uint64_t label,
    uint8_t path_hashes[][PW_HASH_SIZE]) {
  size_t path_hashes_size =
      get_path_auxiliary_hash_count(&merkle_tree_) * PW_HASH_SIZE;
  if (!fuzzer_hashes.empty()) {
    return assign_pw_field_from_bytes(fuzzer_hashes, (uint8_t*)path_hashes,
                                      path_hashes_size);
  }

  uint8_t height = merkle_tree_.height.v;
  uint8_t bits_per_level = merkle_tree_.bits_per_level.v;
  uint8_t fan_out = 1 << bits_per_level;
  uint8_t num_siblings = fan_out - 1;
  uint64_t shifted_parent_label = label;
  for (uint8_t level = 0; level < height; ++level) {
    uint8_t label_index = shifted_parent_label & num_siblings;
    shifted_parent_label ^= label_index;
    for (uint8_t index = 0; index < fan_out; ++index) {
      if (index == label_index) {
        continue;
      }
      auto itr = hash_tree_.find(
          masked_label(shifted_parent_label | index, level));
      if (itr == hash_tree_.end()) {
        itr = hash_tree_.find(
            masked_label(level, EMPTY_SUBTREE));
      }
      if (index < label_index) {
        memcpy(path_hashes[level * num_siblings + index], itr->second.data(),
               PW_HASH_SIZE);
      } else {
        memcpy(path_hashes[level * num_siblings + index - 1],
               itr->second.data(), PW_HASH_SIZE);
      }
    }
    shifted_parent_label = shifted_parent_label >> bits_per_level;
  }
  return path_hashes_size;
}

void PinweaverModel::UpdatePathHashes(uint64_t label,
                                      uint8_t path_hash[PW_HASH_SIZE]) {
  uint8_t height = merkle_tree_.height.v;
  uint8_t bits_per_level = merkle_tree_.bits_per_level.v;
  uint8_t fan_out = 1 << bits_per_level;
  uint8_t num_siblings = fan_out - 1;
  std::vector<uint8_t> hash(PW_HASH_SIZE, 0);
  if (path_hash == nullptr) {
    hash_tree_.erase(masked_label(label, 0));
  } else {
    hash.assign(path_hash, path_hash + PW_HASH_SIZE);
    hash_tree_.insert(std::make_pair(masked_label(label, 0), hash));
  }

  uint64_t shifted_parent_label = label;
  for (int level = 0; level < height; ++level) {
    shifted_parent_label &= ~((uint64_t)num_siblings);

    LITE_SHA256_CTX ctx;
    DCRYPTO_SHA256_init(&ctx, 1);
    for (int index = 0; index < fan_out; ++index) {
      auto itr = hash_tree_.find(
          masked_label(shifted_parent_label | index, level));
      if (itr == hash_tree_.end()) {
        itr = hash_tree_.find(
            masked_label(level, EMPTY_SUBTREE));
      }
      HASH_update(&ctx, itr->second.data(), itr->second.size());
    }
    shifted_parent_label = shifted_parent_label >> bits_per_level;

    const uint8_t* temp = HASH_final(&ctx);
    hash.assign(temp, temp + PW_HASH_SIZE);
    hash_tree_.insert(
        std::make_pair(masked_label(shifted_parent_label, level), hash));
  }
}

size_t PinweaverModel::SerializeResetTree(const fuzz::PinWeaver& pinweaver,
                                          uint8_t* buffer_) {
  const fuzz::PWResetTree& fuzzer_data = pinweaver.reset_tree();
  struct pw_request_t* request = SerializeCommon(pinweaver, buffer_);
  struct pw_request_reset_tree_t* req_data = &request->data.reset_tree;

  request->header.data_length = sizeof(*req_data);
  req_data->bits_per_level.v = fuzzer_data.bits_per_level();
  req_data->height.v = fuzzer_data.height();

  return request->header.data_length + sizeof(request->header);
}

size_t PinweaverModel::SerializeInsertLeaf(const fuzz::PinWeaver& pinweaver,
                                           uint8_t* buffer_) {
  const fuzz::PWInsertLeaf& fuzzer_data = pinweaver.insert_leaf();
  struct pw_request_t* request = SerializeCommon(pinweaver, buffer_);
  struct pw_request_insert_leaf_t* req_data = &request->data.insert_leaf;

  req_data->label.v = fuzzer_data.label();
  assign_pw_field_from_bytes(fuzzer_data.delay_schedule(),
                             (uint8_t*) req_data->delay_schedule,
                             sizeof(req_data->delay_schedule));
  assign_pw_field_from_bytes(fuzzer_data.low_entropy_secret(),
                             req_data->low_entropy_secret,
                             sizeof(req_data->low_entropy_secret));
  assign_pw_field_from_bytes(fuzzer_data.high_entropy_secret(),
                             req_data->high_entropy_secret,
                             sizeof(req_data->high_entropy_secret));
  assign_pw_field_from_bytes(fuzzer_data.reset_secret(),
                             req_data->reset_secret,
                             sizeof(req_data->reset_secret));
  size_t path_hash_size =
      GetPathHashes(fuzzer_data.path_hashes(), fuzzer_data.label(),
                    req_data->path_hashes);
  request->header.data_length = sizeof(*req_data) + path_hash_size;

  return request->header.data_length + sizeof(request->header);
}

size_t PinweaverModel::SerializeRemoveLeaf(const fuzz::PinWeaver& pinweaver,
                                           uint8_t* buffer_) {
  const fuzz::PWRemoveLeaf& fuzzer_data = pinweaver.remove_leaf();
  struct pw_request_t* request = SerializeCommon(pinweaver, buffer_);
  struct pw_request_remove_leaf_t* req_data = &request->data.remove_leaf;

  req_data->leaf_location.v = fuzzer_data.label();
  GetHmac(fuzzer_data.leaf_hmac(), fuzzer_data.label(), req_data->leaf_hmac);
  size_t path_hash_size =
      GetPathHashes(fuzzer_data.path_hashes(), fuzzer_data.label(),
                    req_data->path_hashes);
  request->header.data_length = sizeof(*req_data) + path_hash_size;

  return request->header.data_length + sizeof(request->header);
}

void PinweaverModel::HandleResetTree(const fuzz::PinWeaver& pinweaver,
                                     uint8_t* buffer_) {
  leaf_metadata_.clear();
  hash_tree_.clear();

  uint8_t height = merkle_tree_.height.v;
  uint8_t bits_per_level = merkle_tree_.bits_per_level.v;
  uint8_t fan_out = 1 << bits_per_level;
  std::vector<uint8_t> hash(PW_HASH_SIZE, 0);

  hash_tree_.insert(std::make_pair(masked_label(0, EMPTY_SUBTREE), hash));
  for (int level = 1; level < height; ++level) {
    LITE_SHA256_CTX ctx;
    DCRYPTO_SHA256_init(&ctx, 1);
    for (int index = 0; index < fan_out; ++index) {
      HASH_update(&ctx, hash.data(), hash.size());
    }
    const uint8_t* temp = HASH_final(&ctx);
    hash.assign(temp, temp + PW_HASH_SIZE);
    hash_tree_.insert(std::make_pair(masked_label(level, EMPTY_SUBTREE), hash));
  }
}

void PinweaverModel::HandleInsertLeaf(
    const fuzz::PinWeaver& pinweaver, uint8_t* buffer_,
    std::unique_ptr<struct leaf_data> metadata) {
  struct pw_response_t* response = (struct pw_response_t*)buffer_;
  struct pw_response_insert_leaf_t* resp = &response->data.insert_leaf;

  uint64_t label = pinweaver.remove_leaf().label();
  uint8_t* data = (uint8_t*)&resp->unimported_leaf_data;
  size_t length = response->header.data_length;
  metadata->wrapped_data_.resize(length);
  metadata->wrapped_data_.assign(data, data + length);
  leaf_metadata_.insert(std::make_pair(label, std::move(metadata)));
  UpdatePathHashes(label, resp->unimported_leaf_data.hmac);
}

void PinweaverModel::HandleRemoveLeaf(const fuzz::PinWeaver& pinweaver,
                                      uint8_t* buffer_) {
  uint64_t label = pinweaver.remove_leaf().label();
  leaf_metadata_.erase(label);
  UpdatePathHashes(label, nullptr);
}
