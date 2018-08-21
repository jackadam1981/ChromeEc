/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "pinweaver_model.h"

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
                                     uint8_t* buffer) {
  struct pw_request_t* request = (struct pw_request_t*)buffer;
  request->header.version = pinweaver.version();
  return request;
}

}  // namespace

/******************************************************************************/
/* Public member functions. */
/******************************************************************************/

void PinweaverModel::SendBuffer(uint8_t* buffer) {
  struct pw_request_t* request = (struct pw_request_t*)buffer;
  struct pw_response_t* response = (struct pw_response_t*)buffer;
  pw_handle_request(&merkle_tree_, request, response);
}

size_t PinweaverModel::SerializePinweaver(const fuzz::PinWeaver& pinweaver,
                                          uint8_t* buffer) {
  switch(pinweaver.request_case()) {
    case fuzz::PinWeaver::kResetTree:
      return SerializeResetTree(pinweaver, buffer);
    case fuzz::PinWeaver::kInsertLeaf:
      return SerializeInsertLeaf(pinweaver, buffer);
    case fuzz::PinWeaver::kRemoveLeaf:
      return SerializeRemoveLeaf(pinweaver, buffer);
    case fuzz::PinWeaver::REQUEST_NOT_SET:
      break;
  }
  return 0;
}

void PinweaverModel::ApplyPinweaver(const fuzz::PinWeaver& pinweaver,
                                    uint8_t* buffer) {
  SerializePinweaver(pinweaver, buffer);
  std::unique_ptr<struct leaf_data> metadata;

  const struct pw_request_t* request = (const struct pw_request_t*)buffer;
  struct pw_response_t* response = (struct pw_response_t*)buffer;

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
      HandleResetTree(pinweaver, buffer);
      break;
    case fuzz::PinWeaver::kInsertLeaf:
      HandleInsertLeaf(pinweaver, buffer, std::move(metadata));
      break;
    case fuzz::PinWeaver::kRemoveLeaf:
      HandleRemoveLeaf(pinweaver, buffer);
      break;
    case fuzz::PinWeaver::REQUEST_NOT_SET:
      return;
  }
}

void PinweaverModel::Reset() {
  memset(&merkle_tree_, 0, sizeof(merkle_tree_));
  leaf_metadata_.clear();
  mem_hash_tree_.Reset();
};

/******************************************************************************/
/* Private member functions. */
/******************************************************************************/

void PinweaverModel::GetHmac(const std::string& fuzzer_hmac, uint64_t label,
                             uint8_t hmac[PW_HASH_SIZE]) {
  if (!fuzzer_hmac.empty()) {
    assign_pw_field_from_bytes(fuzzer_hmac, hmac, PW_HASH_SIZE);
    return;
  }

  mem_hash_tree_.GetLeaf(label, hmac);
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
  return mem_hash_tree_.GetPathHashes(label, path_hashes);
}

size_t PinweaverModel::SerializeResetTree(const fuzz::PinWeaver& pinweaver,
                                          uint8_t* buffer) {
  const fuzz::PWResetTree& fuzzer_data = pinweaver.reset_tree();
  struct pw_request_t* request = SerializeCommon(pinweaver, buffer);
  struct pw_request_reset_tree_t* req_data = &request->data.reset_tree;

  request->header.data_length = sizeof(*req_data);
  req_data->bits_per_level.v = fuzzer_data.bits_per_level();
  req_data->height.v = fuzzer_data.height();

  return request->header.data_length + sizeof(request->header);
}

size_t PinweaverModel::SerializeInsertLeaf(const fuzz::PinWeaver& pinweaver,
                                           uint8_t* buffer) {
  const fuzz::PWInsertLeaf& fuzzer_data = pinweaver.insert_leaf();
  struct pw_request_t* request = SerializeCommon(pinweaver, buffer);
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
                                           uint8_t* buffer) {
  const fuzz::PWRemoveLeaf& fuzzer_data = pinweaver.remove_leaf();
  struct pw_request_t* request = SerializeCommon(pinweaver, buffer);
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
                                     uint8_t* buffer) {
  leaf_metadata_.clear();
  mem_hash_tree_.Reset(merkle_tree_);
}

void PinweaverModel::HandleInsertLeaf(
    const fuzz::PinWeaver& pinweaver, uint8_t* buffer,
    std::unique_ptr<struct leaf_data> metadata) {
  struct pw_response_t* response = (struct pw_response_t*)buffer;
  struct pw_response_insert_leaf_t* resp = &response->data.insert_leaf;

  uint64_t label = pinweaver.remove_leaf().label();
  uint8_t* data = (uint8_t*)&resp->unimported_leaf_data;
  size_t length = response->header.data_length;
  metadata->wrapped_data_.resize(length);
  metadata->wrapped_data_.assign(data, data + length);
  leaf_metadata_.insert(std::make_pair(label, std::move(metadata)));
  mem_hash_tree_.UpdatePathHashes(label, resp->unimported_leaf_data.hmac);
}

void PinweaverModel::HandleRemoveLeaf(const fuzz::PinWeaver& pinweaver,
                                      uint8_t* buffer) {
  uint64_t label = pinweaver.remove_leaf().label();
  leaf_metadata_.erase(label);
  mem_hash_tree_.UpdatePathHashes(label, nullptr);
}
