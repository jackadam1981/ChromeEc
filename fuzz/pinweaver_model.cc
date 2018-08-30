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
    case fuzz::PinWeaver::kTryAuth:
      return SerializeTryAuth(pinweaver, buffer_);
    case fuzz::PinWeaver::kResetAuth:
      return SerializeResetAuth(pinweaver, buffer_);
    case fuzz::PinWeaver::kGetLog:
      return SerializeGetLog(pinweaver, buffer_);
    case fuzz::PinWeaver::kLogReplay:
      return SerializeLogReplay(pinweaver, buffer_);
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
  if (response->header.result_code != EC_SUCCESS &&
      pinweaver.request_case() != fuzz::PinWeaver::kTryAuth) {
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
    case fuzz::PinWeaver::kTryAuth:
      HandleTryAuth(pinweaver, buffer_);
      break;
    case fuzz::PinWeaver::kResetAuth:
      HandleResetAuth(pinweaver, buffer_);
      break;
    case fuzz::PinWeaver::kGetLog:
      HandleGetLog(pinweaver, buffer_);
      break;
    case fuzz::PinWeaver::kLogReplay:
      HandleLogReplay(pinweaver, buffer_);
      break;
    case fuzz::PinWeaver::REQUEST_NOT_SET:
      return;
  }
}

void PinweaverModel::Reset() {
  memset(&merkle_tree_, 0, sizeof(merkle_tree_));
  leaf_metadata_.clear();
  mem_hash_tree_.Reset();
  root_history_.clear();
  root_history_head_ = 0;
};

/******************************************************************************/
/* Private static fields. */
/******************************************************************************/

constexpr uint8_t PinweaverModel::kNullRootHash[PW_HASH_SIZE];

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

size_t PinweaverModel::GetMetadata(
    uint64_t label, struct unimported_leaf_data_t* unimported_leaf_data) {
  auto itr = leaf_metadata_.find(label);
  if (itr == leaf_metadata_.end()) {
    return 0;
  }

  const std::vector<uint8_t>& data = itr->second->wrapped_data_;
  memcpy(&unimported_leaf_data, data.data(), data.size());

  uint8_t (*path_hashes)[][PW_HASH_SIZE] =
      reinterpret_cast<uint8_t(*)[][PW_HASH_SIZE]>(
          (uint8_t*)&unimported_leaf_data + data.size());
  return data.size() + mem_hash_tree_.GetPathHashes(label, *path_hashes);
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

void PinweaverModel::LogRootHash(const uint8_t root_hash[PW_HASH_SIZE],
                                 uint64_t label) {
  std::pair<std::vector<uint8_t>, uint64_t> entry{
    {root_hash, root_hash + PW_HASH_SIZE}, label};
  if (root_history_.size() < PW_LOG_ENTRY_COUNT) {
    root_history_.push_back(std::move(entry));
  } else {
    root_history_[root_history_head_].swap(entry);
    ++root_history_head_;
    if (root_history_head_ >= root_history_.size()) {
      root_history_head_ = 0;
    }
  }
}

const uint8_t* PinweaverModel::GetRootHash(size_t index) {
  if (index < root_history_.size()) {
    // root_history_ is ordered first to last, but retrieval is last to first.
    index = root_history_.size() - 1 - index + root_history_head_;
    if (index >= root_history_.size()) {
      index -= root_history_.size();
    }
    return root_history_[index].first.data();
  } else {
    return kNullRootHash;
  }
}

uint64_t PinweaverModel::GetLabel(size_t index) {
  if (index < root_history_.size()) {
    // root_history_ is ordered first to last, but retrieval is last to first.
    index = root_history_.size() - 1 - index + root_history_head_;
    if (index >= root_history_.size()) {
      index -= root_history_.size();
    }
    return root_history_[index].second;
  } else {
    return 0;
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

size_t PinweaverModel::SerializeTryAuth(const fuzz::PinWeaver& pinweaver,
                                        uint8_t* buffer_) {
  const fuzz::PWTryAuth& fuzzer_data = pinweaver.try_auth();
  struct pw_request_t* request = SerializeCommon(pinweaver, buffer_);
  struct pw_request_try_auth_t* req_data = &request->data.try_auth;

  assign_pw_field_from_bytes(fuzzer_data.low_entropy_secret(),
                             req_data->low_entropy_secret,
                             sizeof(req_data->low_entropy_secret));
  request->header.data_length =
      sizeof(*req_data) +
      GetMetadata(fuzzer_data.label(), &req_data->unimported_leaf_data);

  return request->header.data_length + sizeof(request->header);
}

size_t PinweaverModel::SerializeResetAuth(const fuzz::PinWeaver& pinweaver,
                                          uint8_t* buffer_) {
  const fuzz::PWResetAuth& fuzzer_data = pinweaver.reset_auth();
  struct pw_request_t* request = SerializeCommon(pinweaver, buffer_);
  struct pw_request_reset_auth_t* req_data = &request->data.reset_auth;

  assign_pw_field_from_bytes(fuzzer_data.reset_secret(),
                             req_data->reset_secret,
                             sizeof(req_data->reset_secret));
  request->header.data_length =
      sizeof(*req_data) +
          GetMetadata(fuzzer_data.label(), &req_data->unimported_leaf_data);

  return request->header.data_length + sizeof(request->header);
}

size_t PinweaverModel::SerializeGetLog(const fuzz::PinWeaver& pinweaver,
                                       uint8_t* buffer_) {
  const fuzz::PWGetLog& fuzzer_data = pinweaver.get_log();
  struct pw_request_t* request = SerializeCommon(pinweaver, buffer_);
  struct pw_request_get_log_t* req_data = &request->data.get_log;

  memcpy(req_data->root, GetRootHash(fuzzer_data.index_of_root()),
         PW_HASH_SIZE);
  request->header.data_length = sizeof(*req_data);

  return request->header.data_length + sizeof(request->header);
}

size_t PinweaverModel::SerializeLogReplay(const fuzz::PinWeaver& pinweaver,
                                          uint8_t* buffer_) {
  const fuzz::PWLogReplay& fuzzer_data = pinweaver.log_replay();
  struct pw_request_t* request = SerializeCommon(pinweaver, buffer_);
  struct pw_request_log_replay_t* req_data = &request->data.log_replay;

  memcpy(req_data->log_root, GetRootHash(fuzzer_data.index_of_root()),
         PW_HASH_SIZE);
  request->header.data_length =
      sizeof(*req_data) +
          GetMetadata(GetLabel(fuzzer_data.index_of_root()),
                      &req_data->unimported_leaf_data);

  return request->header.data_length + sizeof(request->header);
}


void PinweaverModel::HandleResetTree(const fuzz::PinWeaver& pinweaver,
                                     uint8_t* buffer_) {
  leaf_metadata_.clear();
  mem_hash_tree_.Reset(merkle_tree_);
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

  LogRootHash(response->header.root, label);
  mem_hash_tree_.UpdatePathHashes(label, resp->unimported_leaf_data.hmac);
}

void PinweaverModel::HandleRemoveLeaf(const fuzz::PinWeaver& pinweaver,
                                      uint8_t* buffer_) {
  struct pw_response_t* response = (struct pw_response_t*)buffer_;
  uint64_t label = pinweaver.remove_leaf().label();
  leaf_metadata_.erase(label);

  LogRootHash(response->header.root, label);
  mem_hash_tree_.UpdatePathHashes(label, nullptr);
}

void PinweaverModel::HandleTryAuth(const fuzz::PinWeaver& pinweaver,
                                   uint8_t* buffer_) {
}

void PinweaverModel::HandleResetAuth(const fuzz::PinWeaver& pinweaver,
                                     uint8_t* buffer_) {
}

void PinweaverModel::HandleGetLog(const fuzz::PinWeaver& pinweaver,
                                  uint8_t* buffer_) {
}

void PinweaverModel::HandleLogReplay(const fuzz::PinWeaver& pinweaver,
                                     uint8_t* buffer_) {
}
