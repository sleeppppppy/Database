#include "buffer/lru_k_replacer.h"
#include <algorithm>
#include <iostream>
#include <limits>
#include <chrono>

namespace bustub {

LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k) 
    : current_size_(0), replacer_size_(num_frames), k_(k) {}

auto LRUKReplacer::Evict() -> std::optional<frame_id_t> {
  std::scoped_lock lock(latch_);
  
  // First, try to evict from cache list (frames with k or more accesses)
  if (!cache_list_.empty()) {
    // Find the frame with the earliest k-th access (back of the list is LRU)
    for (auto it = cache_list_.rbegin(); it != cache_list_.rend(); ++it) {
      frame_id_t frame_id = *it;
      if (frame_table_[frame_id].evictable) {
        // Found an evictable frame in cache list
        RemoveFrame(frame_id);
        return frame_id;
      }
    }
  }
  
  // If no evictable frame in cache list, try history list (frames with less than k accesses)
  if (!history_list_.empty()) {
    // Find the frame with the earliest first access
    frame_id_t candidate = -1;
    time_t earliest_time = std::numeric_limits<time_t>::max();
    
    for (auto frame : history_list_) {
      auto &frame_info = frame_table_[frame];
      if (frame_info.evictable && frame_info.earliest_time < earliest_time) {
        candidate = frame;
        earliest_time = frame_info.earliest_time;
      }
    }
    
    if (candidate != -1) {
      RemoveFrame(candidate);
      return candidate;
    }
  }
  
  return std::nullopt;
}

void LRUKReplacer::RecordAccess(frame_id_t frame_id) {
  std::scoped_lock lock(latch_);
  
  if (static_cast<size_t>(frame_id) >= replacer_size_) {
    return;
  }
  
  auto &frame_info = frame_table_[frame_id];
  time_t current_time = GetCurrentTime();
  
  // Record this access
  frame_info.history.push_back(current_time);
  
  // If this is the first access, set earliest time
  if (frame_info.history.size() == 1) {
    frame_info.earliest_time = current_time;
  }
  
  // Maintain only k most recent accesses
  if (frame_info.history.size() > k_) {
    frame_info.history.pop_front();
  }
  
  // Update lists based on access count
  if (frame_info.history.size() < k_) {
    // Still in history list (less than k accesses)
    if (history_map_.find(frame_id) == history_map_.end()) {
      // Add to history list if not already there
      history_list_.push_front(frame_id);
      history_map_[frame_id] = history_list_.begin();
    } else {
      // Move to front of history list (most recent)
      history_list_.erase(history_map_[frame_id]);
      history_list_.push_front(frame_id);
      history_map_[frame_id] = history_list_.begin();
    }
    
    // Remove from cache list if it was there
    if (cache_map_.find(frame_id) != cache_map_.end()) {
      cache_list_.erase(cache_map_[frame_id]);
      cache_map_.erase(frame_id);
    }
  } else {
    // Moved to cache list (k or more accesses)
    if (cache_map_.find(frame_id) == cache_map_.end()) {
      // Add to cache list if not already there
      cache_list_.push_front(frame_id);
      cache_map_[frame_id] = cache_list_.begin();
    } else {
      // Move to front of cache list (most recent)
      cache_list_.erase(cache_map_[frame_id]);
      cache_list_.push_front(frame_id);
      cache_map_[frame_id] = cache_list_.begin();
    }
    
    // Remove from history list if it was there
    if (history_map_.find(frame_id) != history_map_.end()) {
      history_list_.erase(history_map_[frame_id]);
      history_map_.erase(frame_id);
    }
  }
}

void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
  std::scoped_lock lock(latch_);
  
  if (frame_table_.find(frame_id) == frame_table_.end()) {
    return;
  }
  
  auto &frame_info = frame_table_[frame_id];
  
  if (frame_info.evictable && !set_evictable) {
    current_size_--;
  } else if (!frame_info.evictable && set_evictable) {
    current_size_++;
  }
  
  frame_info.evictable = set_evictable;
}

void LRUKReplacer::Remove(frame_id_t frame_id) {
  std::scoped_lock lock(latch_);
  
  if (frame_table_.find(frame_id) == frame_table_.end()) {
    return;
  }
  
  if (!frame_table_[frame_id].evictable) {
    return;
  }
  
  RemoveFrame(frame_id);
}

auto LRUKReplacer::Size() -> size_t {
  std::scoped_lock lock(latch_);
  return current_size_;
}

void LRUKReplacer::RemoveFrame(frame_id_t frame_id) {
  if (history_map_.find(frame_id) != history_map_.end()) {
    history_list_.erase(history_map_[frame_id]);
    history_map_.erase(frame_id);
  }
  if (cache_map_.find(frame_id) != cache_map_.end()) {
    cache_list_.erase(cache_map_[frame_id]);
    cache_map_.erase(frame_id);
  }
  
  if (frame_table_[frame_id].evictable) {
    current_size_--;
  }
  frame_table_.erase(frame_id);
}

auto LRUKReplacer::GetCurrentTime() -> time_t {
  return std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::now());
}

}  // namespace bustub
