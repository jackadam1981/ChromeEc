// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pinweaver_model.h"

#include <dcrypto.h>

namespace {

struct pw_request_t* SerializeCommon(const fuzz::PinWeaver& pinweaver,
                                     fuzz::span<uint8_t> buffer) {
  struct pw_request_t* request =
      reinterpret_cast<struct pw_request_t*>(buffer.begin());
  request->header.version = pinweaver.version();
  return request;
}

}  // namespace

//******************************************************************************
// Public member functions.
//******************************************************************************

PinweaverModel::PinweaverModel() {
  Reset();
}

void PinweaverModel::SendBuffer(fuzz::span<uint8_t> buffer) {
  assert(sizeof(pw_request_t) < buffer.size());
  assert(sizeof(pw_response_t) < buffer.size());
  pw_request_t* request = reinterpret_cast<pw_request_t*>(buffer.begin());
  pw_response_t* response = reinterpret_cast<pw_response_t*>(buffer.begin());
  pw_handle_request(&merkle_tree_, request, response);
}

size_t PinweaverModel::SerializePinweaver(const fuzz::PinWeaver& pinweaver,
                                          fuzz::span<uint8_t> buffer) const {
  switch(pinweaver.request_case()) {
    case fuzz::PinWeaver::kResetTree:
      return SerializeResetTree(pinweaver, buffer);
    case fuzz::PinWeaver::kInsertLeaf:
      return SerializeInsertLeaf(pinweaver, buffer);
    case fuzz::PinWeaver::kRemoveLeaf:
      return SerializeRemoveLeaf(pinweaver, buffer);
    case fuzz::PinWeaver::kTryAuth:
      return SerializeTryAuth(pinweaver, buffer);
    case fuzz::PinWeaver::kResetAuth:
      return SerializeResetAuth(pinweaver, buffer);
    case fuzz::PinWeaver::kGetLog:
      return SerializeGetLog(pinweaver, buffer);
    case fuzz::PinWeaver::kLogReplay:
      return SerializeLogReplay(pinweaver, buffer);
    case fuzz::PinWeaver::REQUEST_NOT_SET:
      break;
  }
  return 0;
}

void PinweaverModel::ApplyPinweaver(const fuzz::PinWeaver& pinweaver,
                                    fuzz::span<uint8_t> buffer) {
  SerializePinweaver(pinweaver, buffer);
  pw_request_insert_leaf_t metadata;

  pw_request_t* request = reinterpret_cast<pw_request_t*>(buffer.begin());
  pw_response_t* response = reinterpret_cast<pw_response_t*>(buffer.begin());

  if (pinweaver.request_case() == fuzz::PinWeaver::kInsertLeaf) {
    metadata = request->data.insert_leaf;
  }

  pw_handle_request(&merkle_tree_, request, response);
  if (response->header.result_code != EC_SUCCESS &&
      pinweaver.request_case() != fuzz::PinWeaver::kTryAuth) {
    return;
  }

  switch(pinweaver.request_case()) {
    case fuzz::PinWeaver::kResetTree:
      ApplyResetTree(pinweaver, buffer);
      break;
    case fuzz::PinWeaver::kInsertLeaf:
      ApplyInsertLeaf(pinweaver, buffer, &metadata);
      break;
    case fuzz::PinWeaver::kRemoveLeaf:
      ApplyRemoveLeaf(pinweaver, buffer);
      break;
    case fuzz::PinWeaver::kTryAuth:
      ApplyTryAuth(pinweaver, buffer);
      break;
    case fuzz::PinWeaver::kResetAuth:
      ApplyResetAuth(pinweaver, buffer);
      break;
    // GetLog and LogReplay have no side-effects so the model doesn't need to be
    // updated.
    case fuzz::PinWeaver::kGetLog:
    case fuzz::PinWeaver::kLogReplay:
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

//******************************************************************************
// Private static fields.
//******************************************************************************

constexpr uint8_t PinweaverModel::kNullRootHash[PW_HASH_SIZE];

//******************************************************************************
// Private member functions.
//******************************************************************************

void PinweaverModel::GetHmac(const std::string& fuzzer_hmac, uint64_t label,
                             fuzz::span<uint8_t> hmac) const {
  assert(hmac.size() == PW_HASH_SIZE);
  if (!fuzzer_hmac.empty()) {
    fuzz::CopyWithPadding(fuzzer_hmac, hmac, 0);
    return;
  }

  mem_hash_tree_.GetLeaf(label, hmac);
}

size_t PinweaverModel::GetMetadata(
    uint64_t label, unimported_leaf_data_t* unimported_leaf_data,
    fuzz::span<uint8_t> buffer) const {
  auto itr = leaf_metadata_.find(label);
  if (itr == leaf_metadata_.end()) {
    return 0;
  }

  const std::vector<uint8_t>& data = itr->second.wrapped_data;
  memcpy(&unimported_leaf_data, data.data(), data.size());

  fuzz::span<uint8_t> path_hashes(
      reinterpret_cast<uint8_t*>(&unimported_leaf_data) + data.size(),
      buffer.end());
  return data.size() + mem_hash_tree_.GetPath(label, path_hashes);
}

size_t PinweaverModel::GetPath(
    const std::string& fuzzer_hashes, uint64_t label,
    fuzz::span<uint8_t> path_hashes) const {
  if (!fuzzer_hashes.empty()) {
    return fuzz::CopyWithPadding(fuzzer_hashes, path_hashes, 0);
  }
  return mem_hash_tree_.GetPath(label, path_hashes);
}

void PinweaverModel::LogRootHash(fuzz::span<const uint8_t> root_hash,
                                 uint64_t label) {
  assert(root_hash.size() == PW_HASH_SIZE);
  std::pair<std::vector<uint8_t>, uint64_t> entry{
    {root_hash.begin(), root_hash.end()}, label};
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

fuzz::span<const uint8_t> PinweaverModel::GetRootHashFromLog(
    size_t index) const {
  if (index < root_history_.size()) {
    // root_history_ is ordered first to last, but retrieval is last to first.
    index = root_history_.size() - 1 - index + root_history_head_;
    if (index >= root_history_.size()) {
      index -= root_history_.size();
    }
    assert(root_history_[index].first.size() == PW_HASH_SIZE);
    return root_history_[index].first;
  } else {
    return TO_CONST_SPAN(kNullRootHash);
  }
}

uint64_t PinweaverModel::GetLabelFromLog(size_t index) const {
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
                                          fuzz::span<uint8_t> buffer) const {
  const fuzz::PWResetTree& fuzzer_data = pinweaver.reset_tree();
  pw_request_t* request = SerializeCommon(pinweaver, buffer);
  pw_request_reset_tree_t* req_data = &request->data.reset_tree;

  request->header.data_length = sizeof(*req_data);
  req_data->bits_per_level.v = fuzzer_data.bits_per_level();
  req_data->height.v = fuzzer_data.height();

  return request->header.data_length + sizeof(request->header);
}

size_t PinweaverModel::SerializeInsertLeaf(const fuzz::PinWeaver& pinweaver,
                                           fuzz::span<uint8_t> buffer) const {
  const fuzz::PWInsertLeaf& fuzzer_data = pinweaver.insert_leaf();
  pw_request_t* request = SerializeCommon(pinweaver, buffer);
  pw_request_insert_leaf_t* req_data = &request->data.insert_leaf;

  req_data->label.v = fuzzer_data.label();
  fuzz::CopyWithPadding(fuzzer_data.delay_schedule(),
                        TO_SPAN(req_data->delay_schedule), 0);
  fuzz::CopyWithPadding(fuzzer_data.low_entropy_secret(),
                        TO_SPAN(req_data->low_entropy_secret), 0);
  fuzz::CopyWithPadding(fuzzer_data.high_entropy_secret(),
                        TO_SPAN(req_data->high_entropy_secret), 0);
  fuzz::CopyWithPadding(fuzzer_data.reset_secret(),
                        TO_SPAN(req_data->reset_secret), 0);

  fuzz::span<uint8_t> path_hashes(
      reinterpret_cast<uint8_t*>(req_data->path_hashes), buffer.end());
  size_t path_hash_size = GetPath(fuzzer_data.path_hashes(),
                                  fuzzer_data.label(), path_hashes);
  request->header.data_length = sizeof(*req_data) + path_hash_size;

  return request->header.data_length + sizeof(request->header);
}

size_t PinweaverModel::SerializeRemoveLeaf(const fuzz::PinWeaver& pinweaver,
                                           fuzz::span<uint8_t> buffer) const {
  const fuzz::PWRemoveLeaf& fuzzer_data = pinweaver.remove_leaf();
  pw_request_t* request = SerializeCommon(pinweaver, buffer);
  pw_request_remove_leaf_t* req_data = &request->data.remove_leaf;

  req_data->leaf_location.v = fuzzer_data.label();
  GetHmac(fuzzer_data.leaf_hmac(), fuzzer_data.label(),
          TO_SPAN(req_data->leaf_hmac));

  fuzz::span<uint8_t> path_hashes(
      reinterpret_cast<uint8_t*>(req_data->path_hashes), buffer.end());
  size_t path_hash_size =
      GetPath(fuzzer_data.path_hashes(), fuzzer_data.label(), path_hashes);
  request->header.data_length = sizeof(*req_data) + path_hash_size;

  return request->header.data_length + sizeof(request->header);
}

size_t PinweaverModel::SerializeTryAuth(const fuzz::PinWeaver& pinweaver,
                                        fuzz::span<uint8_t> buffer) const {
  const fuzz::PWTryAuth& fuzzer_data = pinweaver.try_auth();
  pw_request_t* request = SerializeCommon(pinweaver, buffer);
  pw_request_try_auth_t* req_data = &request->data.try_auth;

  fuzz::CopyWithPadding(fuzzer_data.low_entropy_secret(),
                        TO_SPAN(req_data->low_entropy_secret), 0);
  request->header.data_length =
      sizeof(*req_data) +
      GetMetadata(fuzzer_data.label(), &req_data->unimported_leaf_data, buffer);

  return request->header.data_length + sizeof(request->header);
}

size_t PinweaverModel::SerializeResetAuth(const fuzz::PinWeaver& pinweaver,
                                          fuzz::span<uint8_t> buffer) const {
  const fuzz::PWResetAuth& fuzzer_data = pinweaver.reset_auth();
  pw_request_t* request = SerializeCommon(pinweaver, buffer);
  pw_request_reset_auth_t* req_data = &request->data.reset_auth;

  fuzz::CopyWithPadding(fuzzer_data.reset_secret(),
                        TO_SPAN(req_data->reset_secret), 0);
  request->header.data_length =
      sizeof(*req_data) +
          GetMetadata(fuzzer_data.label(), &req_data->unimported_leaf_data,
                      buffer);

  return request->header.data_length + sizeof(request->header);
}

size_t PinweaverModel::SerializeGetLog(const fuzz::PinWeaver& pinweaver,
                                       fuzz::span<uint8_t> buffer) const {
  const fuzz::PWGetLog& fuzzer_data = pinweaver.get_log();
  pw_request_t* request = SerializeCommon(pinweaver, buffer);
  pw_request_get_log_t* req_data = &request->data.get_log;

  memcpy(req_data->root,
         GetRootHashFromLog(fuzzer_data.index_of_root()).begin(),
         PW_HASH_SIZE);
  request->header.data_length = sizeof(*req_data);

  return request->header.data_length + sizeof(request->header);
}

size_t PinweaverModel::SerializeLogReplay(const fuzz::PinWeaver& pinweaver,
                                          fuzz::span<uint8_t> buffer) const {
  const fuzz::PWLogReplay& fuzzer_data = pinweaver.log_replay();
  pw_request_t* request = SerializeCommon(pinweaver, buffer);
  pw_request_log_replay_t* req_data = &request->data.log_replay;

  memcpy(req_data->log_root,
         GetRootHashFromLog(fuzzer_data.index_of_root()).begin(),
         PW_HASH_SIZE);
  request->header.data_length =
      sizeof(*req_data) +
          GetMetadata(GetLabelFromLog(fuzzer_data.index_of_root()),
                      &req_data->unimported_leaf_data, buffer);

  return request->header.data_length + sizeof(request->header);
}

void PinweaverModel::UpdateMetadata(
    uint64_t label, const pw_response_header_t* header,
    const unimported_leaf_data_t* unimported_leaf_data,
    const pw_request_insert_leaf_t* insert) {
  LogRootHash(TO_CONST_SPAN(header->root), label);
  if (unimported_leaf_data) {
    const uint8_t* data =
        reinterpret_cast<const uint8_t*>(unimported_leaf_data);
    size_t length = header->data_length;
    if (insert) {
      leaf_metadata_.insert(
          std::make_pair(
              label,
              LeafData{std::vector<uint8_t>(data, data + length), *insert}));
    } else {
      leaf_metadata_.at(label).wrapped_data.assign(data, data + length);
    }
    mem_hash_tree_.UpdatePath(label, TO_CONST_SPAN(unimported_leaf_data->hmac));
  } else {
    leaf_metadata_.erase(label);
    mem_hash_tree_.UpdatePath(label, fuzz::span<const uint8_t>() /*path_hash*/);
  }
}

void PinweaverModel::ApplyResetTree(const fuzz::PinWeaver& pinweaver,
                                    fuzz::span<uint8_t> buffer) {
  leaf_metadata_.clear();
  mem_hash_tree_.Reset(merkle_tree_.bits_per_level.v, merkle_tree_.height.v);
}

void PinweaverModel::ApplyInsertLeaf(
    const fuzz::PinWeaver& pinweaver, fuzz::span<uint8_t> buffer,
    const pw_request_insert_leaf_t* metadata) {
  pw_response_t* response = reinterpret_cast<pw_response_t*>(buffer.begin());
  pw_response_insert_leaf_t* resp = &response->data.insert_leaf;
  UpdateMetadata(pinweaver.insert_leaf().label(), &response->header,
                 &resp->unimported_leaf_data, metadata);
}

void PinweaverModel::ApplyRemoveLeaf(const fuzz::PinWeaver& pinweaver,
                                     fuzz::span<uint8_t> buffer) {
  pw_response_t* response = reinterpret_cast<pw_response_t*>(buffer.begin());
  UpdateMetadata(pinweaver.remove_leaf().label(), &response->header,
                 nullptr /*unimported_leaf_data*/, nullptr /*insert*/);
}

void PinweaverModel::ApplyTryAuth(const fuzz::PinWeaver& pinweaver,
                                  fuzz::span<uint8_t> buffer) {
  pw_response_t* response = reinterpret_cast<pw_response_t*>(buffer.begin());
  pw_response_try_auth_t* resp = &response->data.try_auth;

  if (response->header.result_code != EC_SUCCESS &&
      response->header.result_code != PW_ERR_LOWENT_AUTH_FAILED){
    return;
  }
  UpdateMetadata(pinweaver.try_auth().label(), &response->header,
                 &resp->unimported_leaf_data, nullptr /*insert*/);
}

void PinweaverModel::ApplyResetAuth(const fuzz::PinWeaver& pinweaver,
                                    fuzz::span<uint8_t> buffer) {
  pw_response_t* response = reinterpret_cast<pw_response_t*>(buffer.begin());
  pw_response_reset_auth_t* resp = &response->data.reset_auth;
  UpdateMetadata(pinweaver.reset_auth().label(), &response->header,
                 &resp->unimported_leaf_data, nullptr /*insert*/);
}
