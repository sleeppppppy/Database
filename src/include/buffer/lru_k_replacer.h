#pragma once

#include <cstddef>
#include <list>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <ctime>
#include <optional>
#include "common/config.h"

namespace bustub {

class LRUKReplacer {
 public:
  explicit LRUKReplacer(size_t num_frames, size_t k);
  ~LRUKReplacer() = default;

  auto Evict() -> std::optional<frame_id_t>;
  void RecordAccess(frame_id_t frame_id);
  void SetEvictable(frame_id_t frame_id, bool set_evictable);
  void Remove(frame_id_t frame_id);
  auto Size() -> size_t;

 private:
  struct FrameInfo {
    std::list<time_t> history;
    bool evictable;
    time_t earliest_time;
    
    FrameInfo() : evictable(false), earliest_time(0) {}
  };

  void RemoveFrame(frame_id_t frame_id);
  auto GetCurrentTime() -> time_t;

  size_t current_size_;
  size_t replacer_size_;
  size_t k_;
  std::mutex latch_;
  
  std::unordered_map<frame_id_t, FrameInfo> frame_table_;
  std::list<frame_id_t> history_list_;
  std::list<frame_id_t> cache_list_;
  std::unordered_map<frame_id_t, std::list<frame_id_t>::iterator> history_map_;
  std::unordered_map<frame_id_t, std::list<frame_id_t>::iterator> cache_map_;
};

}  // namespace bustub
