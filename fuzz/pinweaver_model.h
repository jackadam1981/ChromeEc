// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Pinweaver specific model to facilitate fuzzing.

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
#include "fuzz/span.h"

// Provides enough state tracking to send valid PinWeaver requests. This is
// necessary because of the authentication dependent fields used by the Merkle
// tree such as HMACs and a set of sibling path hashes that must be correct to
// reach some parts of the PinWeaver code.
class PinweaverModel {
 public:
  PinweaverModel();

  void SendBuffer(fuzz::span<uint8_t> buffer);

  // Converts the logical representation of a request used in fuzzing into bytes
  // that can be processed by the pinweaver code for fuzzing.
  size_t SerializePinweaver(const fuzz::PinWeaver& pinweaver,
                            fuzz::span<uint8_t> buffer) const;

  // Executes a request in the form of a fuzz::PinWeaver proto, and updates the
  // model, so that future requests will be valid.
  void ApplyPinweaver(const fuzz::PinWeaver& pinweaver,
                      fuzz::span<uint8_t> buffer);

  // Clears any state. This shoudl be called at the beginning of each fuzzing
  // iteration. 
  void Reset();

 private:
  static constexpr uint8_t kNullRootHash[PW_HASH_SIZE] = {};

  struct LeafData {
    std::vector<uint8_t> wrapped_data;
    pw_request_insert_leaf_t insert_leaf;
  };

  // Functions for retrieving the current state of the metadata.
  void GetHmac(const std::string& fuzzer_hmac, uint64_t label,
               fuzz::span<uint8_t> hmac) const;
  size_t GetMetadata(uint64_t label,
                     unimported_leaf_data_t* unimported_leaf_data,
                     fuzz::span<uint8_t> buffer) const;
  size_t GetPath(const std::string& fuzzer_hashes, uint64_t label,
                 fuzz::span<uint8_t> path_hashes) const;

  // Store copies of the root hash of the Merkle tree, and label of the leaf
  // associated with a request so that valid get log requests can be generated.
  void LogRootHash(fuzz::span<const uint8_t> root_hash, uint64_t label);
  // Retrieve a root hash from the log at the given index.
  fuzz::span<const uint8_t> GetRootHash(size_t index) const;
  // Retrieve a leaf label from the log at the given index.
  uint64_t GetLabel(size_t index) const;

  // Helper functions used by SerializePinweaver to convert
  size_t SerializeResetTree(const fuzz::PinWeaver& pinweaver,
                            fuzz::span<uint8_t> buffer) const;
  size_t SerializeInsertLeaf(const fuzz::PinWeaver& pinweaver,
                             fuzz::span<uint8_t> buffer) const;
  size_t SerializeRemoveLeaf(const fuzz::PinWeaver& pinweaver,
                             fuzz::span<uint8_t> buffer) const;
  size_t SerializeTryAuth(const fuzz::PinWeaver& pinweaver,
                          fuzz::span<uint8_t> buffer) const;
  size_t SerializeResetAuth(const fuzz::PinWeaver& pinweaver,
                            fuzz::span<uint8_t> buffer) const;
  size_t SerializeGetLog(const fuzz::PinWeaver& pinweaver,
                         fuzz::span<uint8_t> buffer) const;
  size_t SerializeLogReplay(const fuzz::PinWeaver& pinweaver,
                            fuzz::span<uint8_t> buffer) const;

  // Updates the metadata storage for a particular leaf. |insert| is required
  // only insert operations so the metadata for the leaf can be retrieved to
  // generate valid authentication requests.
  void UpdateMetadata(
      uint64_t label, const pw_response_header_t* header,
      const unimported_leaf_data_t* unimported_leaf_data,
      const pw_request_insert_leaf_t* insert);

  // Helper functions for updating the state when responses are received.
  void ApplyResetTree(const fuzz::PinWeaver& pinweaver,
                      fuzz::span<uint8_t> buffer);
  void ApplyInsertLeaf(
      const fuzz::PinWeaver& pinweaver, fuzz::span<uint8_t> buffer,
      const pw_request_insert_leaf_t* metadata);
  void ApplyRemoveLeaf(const fuzz::PinWeaver& pinweaver,
                       fuzz::span<uint8_t> buffer);
  void ApplyTryAuth(const fuzz::PinWeaver& pinweaver,
                    fuzz::span<uint8_t> buffer);
  void ApplyResetAuth(const fuzz::PinWeaver& pinweaver,
                      fuzz::span<uint8_t> buffer);

  merkle_tree_t merkle_tree_;
  size_t root_history_head_;

  MemHashTree mem_hash_tree_;
  std::vector<std::pair<std::vector<uint8_t>, uint64_t>> root_history_;
  std::unordered_map<uint64_t, LeafData>
      leaf_metadata_;
};

#endif  // __FUZZ_PINWEAVER_MODEL_H
