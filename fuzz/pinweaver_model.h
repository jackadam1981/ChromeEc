/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/* Pinweaver specific model to facilitate fuzzing. */

#ifndef __FUZZ_PINWEAVER_MODEL_H
#define __FUZZ_PINWEAVER_MODEL_H

#include <memory>
#include <unordered_map>

extern "C" {
#define HIDE_EC_STDLIB
#include "include/pinweaver.h"
#include "include/pinweaver_types.h"
};

#include "fuzz/cr50_fuzz.pb.h"
#include "fuzz/mem_hash_tree.h"

/**
 * Provides enough state tracking to send valid PinWeaver requests. This is
 * necessary because of the authentication dependent fields used by the Merkle
 * tree such as HMACs and a set of sibling path hashes that must be correct to
 * reach some parts of the PinWeaver code.
 */
class PinweaverModel {
 public:
  void SendBuffer(uint8_t* buffer);
  size_t SerializePinweaver(const fuzz::PinWeaver& pinweaver,
                            uint8_t* buffer) const;
  /* Executes a request in the form of a fuzz::PinWeaver proto, and updates the
   * model, so that future requests will be valid.
   */
  void ApplyPinweaver(const fuzz::PinWeaver& pinweaver,
                      uint8_t* buffer);
  void Reset();

 private:
  static constexpr uint8_t kNullRootHash[PW_HASH_SIZE] = {};

  struct leaf_data {
    std::vector<uint8_t> wrapped_data_;
    struct pw_request_insert_leaf_t insert_leaf_;
  };

  struct merkle_tree_t merkle_tree_;
  MemHashTree mem_hash_tree_;
  std::vector<std::pair<std::vector<uint8_t>, uint64_t>> root_history_;
  size_t root_history_head_;
  std::unordered_map<uint64_t, std::unique_ptr<struct leaf_data>>
      leaf_metadata_;

  void GetHmac(const std::string& fuzzer_hmac, uint64_t label,
               uint8_t hmac[PW_HASH_SIZE]) const;
  size_t GetMetadata(uint64_t label,
                     struct unimported_leaf_data_t* unimported_leaf_data) const;
  size_t GetPathHashes(const std::string& fuzzer_hashes, uint64_t label,
                       uint8_t path_hashes[][PW_HASH_SIZE]) const;

  void LogRootHash(const uint8_t root_hash[PW_HASH_SIZE], uint64_t label);
  const uint8_t* GetRootHash(size_t index) const;
  uint64_t GetLabel(size_t index) const;

  size_t SerializeResetTree(const fuzz::PinWeaver& pinweaver,
                            uint8_t* buffer) const;
  size_t SerializeInsertLeaf(const fuzz::PinWeaver& pinweaver,
                             uint8_t* buffer) const;
  size_t SerializeRemoveLeaf(const fuzz::PinWeaver& pinweaver,
                             uint8_t* buffer) const;
  size_t SerializeTryAuth(const fuzz::PinWeaver& pinweaver,
                          uint8_t* buffer) const;
  size_t SerializeResetAuth(const fuzz::PinWeaver& pinweaver,
                            uint8_t* buffer) const;
  size_t SerializeGetLog(const fuzz::PinWeaver& pinweaver,
                         uint8_t* buffer) const;
  size_t SerializeLogReplay(const fuzz::PinWeaver& pinweaver,
                            uint8_t* buffer) const;

  void UpdateMetadata(
      uint64_t label, const struct pw_response_header_t* header,
      const struct unimported_leaf_data_t* unimported_leaf_data,
      std::unique_ptr<struct leaf_data> insert);

  void HandleResetTree(const fuzz::PinWeaver& pinweaver, uint8_t* buffer);
  void HandleInsertLeaf(
      const fuzz::PinWeaver& pinweaver, uint8_t* buffer,
      std::unique_ptr<struct leaf_data> metadata);
  void HandleRemoveLeaf(const fuzz::PinWeaver& pinweaver, uint8_t* buffer);
  void HandleTryAuth(const fuzz::PinWeaver& pinweaver, uint8_t* buffer);
  void HandleResetAuth(const fuzz::PinWeaver& pinweaver, uint8_t* buffer);
};

#endif  // __FUZZ_PINWEAVER_MODEL_H
