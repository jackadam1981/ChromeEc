// Copyright 2018 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef __FUZZ_MEM_HASH_TREE_H
#define __FUZZ_MEM_HASH_TREE_H

#include <unistd.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

// MaskedLabel.first is the label path, this is shifted to the right by the
//   (bits_per_level * level)
// MaskedLabel.second is the level of the label (0 for leaf, height for root)
typedef std::pair<uint64_t, uint8_t> MaskedLabel;

namespace std {
template<> struct hash<MaskedLabel> {
  size_t operator()(const MaskedLabel& lbl) const {
    static const auto hash_first = hash<uint64_t>();
    static const auto hash_second = hash<uint8_t>();
    return hash_first(lbl.first) * hash_second(lbl.second);
  }
};
}

class MemHashTree {
 public:
  MemHashTree();

  bool GetLeaf(uint64_t label, uint8_t* leaf_hash) const;
  // Writes the result to |path_hashes| and returns the size in bytes of the
  // returned path for use in serializers that report how much buffer was used.
  size_t GetPath(uint64_t label, uint8_t* path_hashes) const;
  void UpdatePath(uint64_t label, const uint8_t* path_hash);

  void Reset();
  void Reset(uint8_t bits_per_level, uint8_t height);

 private:
  static constexpr uint8_t kEmptySubtree = 0xff;

  uint8_t bits_per_level_;
  uint8_t height_;

  // If the level is kEmptySubtree, the label represents the level of the tree
  // (from the leaves to the root) and the value is the hash if all leaves under
  // a node at that level are empty. This allows for a sparse representation of
  // the hash cache.
  std::unordered_map<MaskedLabel, std::vector<uint8_t>>
      hash_tree_;
};

#endif  // __FUZZ_MEM_HASH_TREE_H
