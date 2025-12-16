#pragma once

#include "common/macros.h" 
#include <unordered_map>
#include <deque>
#include <mutex>  // NOLINT
#include "common/config.h"

namespace bustub {

/**
 * LRU-K replacer.
 * Keeps track of access history (up to k accesses per frame).
 */
class LRUKReplacer {
 public:
  explicit LRUKReplacer(size_t num_frames, size_t k);

  LRUKReplacer(const LRUKReplacer &) = delete;
  LRUKReplacer &operator=(const LRUKReplacer &) = delete;
  ~LRUKReplacer() = default;

  /** Evict one frame based on LRU-K policy. Return true if success. */
  auto Evict(frame_id_t *frame_id) -> bool;

  /** Record that frame_id is accessed. */
  void RecordAccess(frame_id_t frame_id);

  /** Set evictable / non-evictable for a frame. */
  void SetEvictable(frame_id_t frame_id, bool set_evictable);

  /** Remove specified frame (must be evictable). */
  void Remove(frame_id_t frame_id);

  /** Return number of evictable frames. */
  auto Size() -> size_t;

 private:
  // === Core structures ===
  std::unordered_map<frame_id_t, std::deque<size_t>> access_history_;  // time history
  std::unordered_map<frame_id_t, bool> evictable_;                     // evictable status

  // === Internal counters ===
  size_t current_timestamp_{0};  // monotonically increasing time counter
  size_t curr_size_{0};          // number of evictable frames
  size_t replacer_size_;         // total frame capacity
  size_t k_;                     // lookback distance (LRU-K)

  // === Thread safety ===
  std::mutex latch_;
};

}  // namespace bustub

