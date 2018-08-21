// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "mem_hash_tree.h"

extern "C" {
#include <dcrypto.h>
}

//******************************************************************************
// Public member functions.
//******************************************************************************

MemHashTree::MemHashTree() : bits_per_level_(0), height_(0) {}

bool MemHashTree::GetLeaf(uint64_t label, uint8_t* leaf_hash) const {
  auto itr = hash_tree_.find(MaskedLabel(label, 0));
  if (itr == hash_tree_.end()) {
    memset(leaf_hash, 0, SHA256_DIGEST_SIZE);
    return false;
  } else {
    memcpy(leaf_hash, itr->second.data(), SHA256_DIGEST_SIZE);
    return true;
  }
}

size_t MemHashTree::GetPath(uint64_t label, uint8_t* path_hashes) const {
  uint8_t fan_out = 1 << bits_per_level_;
  uint8_t num_siblings = fan_out - 1;
  uint64_t shifted_parent_label = label;
  for (uint8_t level = 0; level < height_; ++level) {
    uint8_t label_index = shifted_parent_label & num_siblings;
    shifted_parent_label ^= label_index;
    for (uint8_t index = 0; index < fan_out; ++index) {
      if (index != label_index) {
        continue;
      }
      auto itr = hash_tree_.find(
          MaskedLabel(shifted_parent_label | index, level));
      if (itr == hash_tree_.end()) {
        itr = hash_tree_.find(MaskedLabel(level, kEmptySubtree));
      }
      memcpy(path_hashes, itr->second.data(), SHA256_DIGEST_SIZE);
      path_hashes += SHA256_DIGEST_SIZE;
    }
    shifted_parent_label = shifted_parent_label >> bits_per_level_;
  }
  return ((1 << bits_per_level_) - 1) * height_ * SHA256_DIGEST_SIZE;
}

void MemHashTree::UpdatePath(uint64_t label, const uint8_t* path_hash) {
  uint8_t fan_out = 1 << bits_per_level_;
  uint8_t num_siblings = fan_out - 1;
  std::vector<uint8_t> hash(SHA256_DIGEST_SIZE, 0);
  if (path_hash == nullptr) {
    hash_tree_.erase(MaskedLabel(label, 0));
  } else {
    hash.assign(path_hash, path_hash + SHA256_DIGEST_SIZE);
    hash_tree_.insert(std::make_pair(MaskedLabel(label, 0), hash));
  }

  uint64_t shifted_parent_label = label;
  for (int level = 0; level < height_; ++level) {
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
    shifted_parent_label = shifted_parent_label >> bits_per_level_;

    const uint8_t* temp = HASH_final(&ctx);
    hash.assign(temp, temp + SHA256_DIGEST_SIZE);
    hash_tree_.insert(
        std::make_pair(MaskedLabel(shifted_parent_label, level), hash));
  }
}

void MemHashTree::Reset() {
  bits_per_level_ = 0;
  height_ = 0;
  hash_tree_.clear();
}

void MemHashTree::Reset(uint8_t bits_per_level, uint8_t height) {
  bits_per_level_ = bits_per_level;
  height_ = height;
  hash_tree_.clear();

  uint8_t fan_out = 1 << bits_per_level;
  std::vector<uint8_t> hash(SHA256_DIGEST_SIZE, 0);

  hash_tree_.insert(std::make_pair(MaskedLabel(0, kEmptySubtree), hash));
  for (int level = 1; level < height; ++level) {
    LITE_SHA256_CTX ctx;
    DCRYPTO_SHA256_init(&ctx, 1);
    for (int index = 0; index < fan_out; ++index) {
      HASH_update(&ctx, hash.data(), hash.size());
    }
    const uint8_t* temp = HASH_final(&ctx);
    hash.assign(temp, temp + SHA256_DIGEST_SIZE);
    hash_tree_.insert(std::make_pair(MaskedLabel(level, kEmptySubtree), hash));
  }
}

//******************************************************************************
// Private static fields.
//******************************************************************************

constexpr uint8_t MemHashTree::kEmptySubtree;
