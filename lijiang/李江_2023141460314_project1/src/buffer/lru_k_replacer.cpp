#include "buffer/lru_k_replacer.h"
#include "common/exception.h"

#include <limits>
#include <utility>

namespace bustub {

LRUKReplacer::LRUKReplacer(size_t num_frames, size_t k)
    : current_timestamp_(0), curr_size_(0), replacer_size_(num_frames), k_(k) {}

void LRUKReplacer::RecordAccess(frame_id_t frame_id) {
  std::scoped_lock<std::mutex> lock(latch_);  // 确保线程安全
  if (frame_id < 0 || static_cast<size_t>(frame_id) >= replacer_size_) {
    throw Exception("RecordAccess: invalid frame id");
  }

  ++current_timestamp_;
  auto &dq = access_history_[frame_id];  // 自动创建新条目（若不存在）
  dq.push_back(current_timestamp_);
  if (dq.size() > k_) {
    dq.pop_front();  // 只保留最近k个时间戳
  }

  // 首次访问的帧默认设为「不可淘汰」，且不统计到curr_size_
  if (evictable_.find(frame_id) == evictable_.end()) {
    evictable_[frame_id] = false;
  }
}

void LRUKReplacer::SetEvictable(frame_id_t frame_id, bool set_evictable) {
  std::scoped_lock<std::mutex> lock(latch_);  // 全局锁确保并发安全
  if (frame_id < 0 || static_cast<size_t>(frame_id) >= replacer_size_) {
    throw Exception("SetEvictable: invalid frame id");
  }

  // 帧必须有访问记录才参与统计（修复Size测试）
  if (access_history_.find(frame_id) == access_history_.end()) {
    evictable_[frame_id] = set_evictable;
    return;
  }

  bool prev_evictable = evictable_[frame_id];  // 默认为false（未记录时）
  evictable_[frame_id] = set_evictable;

  // 严格按状态变化更新curr_size_（修复计数错误）
  if (!prev_evictable && set_evictable) {
    curr_size_++;  // 不可 → 可：+1
  } else if (prev_evictable && !set_evictable) {
    if (curr_size_ > 0) {
      curr_size_--;  // 可 → 不可：-1（防溢出）
    }
  }
}

void LRUKReplacer::Remove(frame_id_t frame_id) {
  std::scoped_lock<std::mutex> lock(latch_);
  auto history_it = access_history_.find(frame_id);
  if (history_it == access_history_.end()) {
    return;  // 帧不存在，直接返回
  }

  auto evict_it = evictable_.find(frame_id);
  if (evict_it == evictable_.end() || !evict_it->second) {
    throw Exception("Remove: frame is not evictable");  // 不可淘汰帧不能删除
  }

  // 移除帧并更新计数
  access_history_.erase(history_it);
  evictable_.erase(evict_it);
  if (curr_size_ > 0) {
    curr_size_--;
  }
}

auto LRUKReplacer::Evict(frame_id_t *frame_id) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  if (curr_size_ == 0) {
    return false;  // 无可淘汰帧
  }

  frame_id_t best_frame = -1;
  size_t best_ts = std::numeric_limits<size_t>::max();
  bool has_incomplete = false;  // 是否有「访问次数<k」的帧

  // 第一阶段：优先淘汰访问次数<k的帧（按最早访问时间）
  for (const auto &[fid, ts_deque] : access_history_) {
    if (!evictable_[fid]) {
      continue;  // 跳过不可淘汰帧
    }
    if (ts_deque.size() < k_) {
      has_incomplete = true;
      size_t first_ts = ts_deque.front();
      if (first_ts < best_ts) {
        best_ts = first_ts;
        best_frame = fid;
      }
    }
  }

  // 第二阶段：若没有访问次数<k的帧，淘汰访问次数>=k的帧（按第k次访问时间）
  if (!has_incomplete) {
    for (const auto &[fid, ts_deque] : access_history_) {
      if (!evictable_[fid]) {
        continue;
      }
      if (ts_deque.size() >= k_) {
        size_t kth_ts = ts_deque.front();  // 第k次访问时间（队首是最早的）
        if (kth_ts < best_ts) {
          best_ts = kth_ts;
          best_frame = fid;
        }
      }
    }
  }

  // 执行淘汰
  if (best_frame != -1 && frame_id != nullptr) {
    *frame_id = best_frame;
    access_history_.erase(best_frame);
    evictable_.erase(best_frame);
    curr_size_--;
    return true;
  }

  return false;
}

auto LRUKReplacer::Size() -> size_t {
  std::scoped_lock<std::mutex> lock(latch_);  // 确保并发时返回正确值
  return curr_size_;
}

}  // namespace bustub

