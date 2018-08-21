#ifndef __FUZZ_MEM_HASH_TREE_H
#define __FUZZ_MEM_HASH_TREE_H

#include <unistd.h>

#include <cstdint>
#include <unordered_map>
#include <vector>

extern "C" {
#define HIDE_EC_STDLIB
#include "include/pinweaver.h"
#include "include/pinweaver_types.h"
}

/* MaskedLabel.first is the label path, this is shifted to the right by the
 *   (bits_per_level * level)
 * MaskedLabel.second is the level of the label (0 for leaf, height for root)
 */
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

  bool GetLeaf(uint64_t label, uint8_t leaf_hash[PW_HASH_SIZE]) const;
  size_t GetPathHashes(uint64_t label,
                       uint8_t path_hashes[][PW_HASH_SIZE]) const;
  void UpdatePathHashes(uint64_t label, const uint8_t path_hash[PW_HASH_SIZE]);

  void Reset();
  void Reset(const struct merkle_tree_t& other);
 private:
  static constexpr uint8_t kEmptySubtree = 0xff;

  struct merkle_tree_t merkle_tree_;

  /* If the level is kEmptySubtree, the label represents the level of the tree
   * (from the leaves to the root) and the value is the hash if all leaves under
   * a node at that level are empty. This allows for a sparse representation of
   * the hash cache.
   */
  std::unordered_map<MaskedLabel, std::vector<uint8_t>>
      hash_tree_;
};

#endif  // __FUZZ_MEM_HASH_TREE_H
