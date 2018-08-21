/* Copyright 2018 The Chromium OS Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "mem_hash_tree.h"

extern "C" {
#include <dcrypto.h>
}

/******************************************************************************/
/* Public member functions. */
/******************************************************************************/

bool MemHashTree::GetLeaf(uint64_t label,
                          uint8_t leaf_hash[PW_HASH_SIZE]) const {
  auto itr = hash_tree_.find(MaskedLabel(label, 0));
  if (itr == hash_tree_.end()) {
    memset(leaf_hash, 0, PW_HASH_SIZE);
    return false;
  } else {
    memcpy(leaf_hash, itr->second.data(), PW_HASH_SIZE);
    return true;
  }
}

size_t MemHashTree::GetPathHashes(uint64_t label,
                                  uint8_t path_hashes[][PW_HASH_SIZE]) const {
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
          MaskedLabel(shifted_parent_label | index, level));
      if (itr == hash_tree_.end()) {
        itr = hash_tree_.find(
            MaskedLabel(level, kEmptySubtree));
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
  return get_path_auxiliary_hash_count(&merkle_tree_) * PW_HASH_SIZE;
}


void MemHashTree::UpdatePathHashes(uint64_t label,
                                   const uint8_t path_hash[PW_HASH_SIZE]) {
  uint8_t height = merkle_tree_.height.v;
  uint8_t bits_per_level = merkle_tree_.bits_per_level.v;
  uint8_t fan_out = 1 << bits_per_level;
  uint8_t num_siblings = fan_out - 1;
  std::vector<uint8_t> hash(PW_HASH_SIZE, 0);
  if (path_hash == nullptr) {
    hash_tree_.erase(MaskedLabel(label, 0));
  } else {
    hash.assign(path_hash, path_hash + PW_HASH_SIZE);
    hash_tree_.insert(std::make_pair(MaskedLabel(label, 0), hash));
  }

  uint64_t shifted_parent_label = label;
  for (int level = 0; level < height; ++level) {
    shifted_parent_label &= ~((uint64_t)num_siblings);

    LITE_SHA256_CTX ctx;
    DCRYPTO_SHA256_init(&ctx, 1);
    for (int index = 0; index < fan_out; ++index) {
      auto itr = hash_tree_.find(
          MaskedLabel(shifted_parent_label | index, level));
      if (itr == hash_tree_.end()) {
        itr = hash_tree_.find(
            MaskedLabel(level, kEmptySubtree));
      }
      HASH_update(&ctx, itr->second.data(), itr->second.size());
    }
    shifted_parent_label = shifted_parent_label >> bits_per_level;

    const uint8_t* temp = HASH_final(&ctx);
    hash.assign(temp, temp + PW_HASH_SIZE);
    hash_tree_.insert(
        std::make_pair(MaskedLabel(shifted_parent_label, level), hash));
  }
}

void MemHashTree::Reset() {
  merkle_tree_ = {};
  hash_tree_.clear();
}

void MemHashTree::Reset(const struct merkle_tree_t& other) {
  merkle_tree_ = other;
  hash_tree_.clear();

  uint8_t height = merkle_tree_.height.v;
  uint8_t bits_per_level = merkle_tree_.bits_per_level.v;
  uint8_t fan_out = 1 << bits_per_level;
  std::vector<uint8_t> hash(PW_HASH_SIZE, 0);

  hash_tree_.insert(std::make_pair(MaskedLabel(0, kEmptySubtree), hash));
  for (int level = 1; level < height; ++level) {
    LITE_SHA256_CTX ctx;
    DCRYPTO_SHA256_init(&ctx, 1);
    for (int index = 0; index < fan_out; ++index) {
      HASH_update(&ctx, hash.data(), hash.size());
    }
    const uint8_t* temp = HASH_final(&ctx);
    hash.assign(temp, temp + PW_HASH_SIZE);
    hash_tree_.insert(std::make_pair(MaskedLabel(level, kEmptySubtree), hash));
  }
}

/******************************************************************************/
/* Private static fields. */
/******************************************************************************/

constexpr uint8_t MemHashTree::kEmptySubtree;
