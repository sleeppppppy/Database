#include <cassert>
#include <functional>
#include <list>
#include <utility>
#include <algorithm>

#include "container/hash/extendible_hash_table.h"
#include "storage/page/page.h"

namespace bustub {

template <typename K, typename V>
ExtendibleHashTable<K, V>::ExtendibleHashTable(size_t bucket_size)
    : global_depth_(0), bucket_size_(bucket_size), num_buckets_(1) {
  // ensure directory has one bucket
  dir_.clear();
  dir_.resize(1);
  dir_[0] = std::make_shared<Bucket>(bucket_size_, 0);
}

// Helper: compute index mask (uses global_depth_)
template <typename K, typename V>
inline size_t ExtendibleHashTable<K, V>::IndexMask() const {
  if (global_depth_ == 0) return 0;
  return (static_cast<size_t>(1) << global_depth_) - 1;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::IndexOf(const K &key) -> size_t {
  size_t mask = IndexMask();
  if (mask == 0) return 0;
  return std::hash<K>()(key) & mask;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetGlobalDepth() const -> int {
  std::scoped_lock<std::mutex> lock(latch_);
  return global_depth_;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetLocalDepth(int dir_index) const -> int {
  std::scoped_lock<std::mutex> lock(latch_);
  if (dir_index < 0 || static_cast<size_t>(dir_index) >= dir_.size() || dir_[dir_index] == nullptr) {
    return -1;
  }
  return dir_[dir_index]->GetDepth();
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetNumBuckets() const -> int {
  std::scoped_lock<std::mutex> lock(latch_);
  return num_buckets_;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Find(const K &key, V &value) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  if (dir_.empty()) return false;
  size_t idx = IndexOf(key);
  auto b = dir_[idx];
  if (!b) return false;
  return b->Find(key, value);
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Remove(const K &key) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  if (dir_.empty()) return false;
  size_t idx = IndexOf(key);
  auto b = dir_[idx];
  if (!b) return false;
  return b->Remove(key);
}

// Helper: double the directory (called when local depth == global depth)
template <typename K, typename V>
void ExtendibleHashTable<K, V>::DoubleDirectory() {
  size_t old_size = dir_.size();
  dir_.resize(old_size * 2);
  for (size_t i = 0; i < old_size; ++i) {
    dir_[i + old_size] = dir_[i];
  }
  ++global_depth_;
}

// Helper: split pointers for a bucket with old_local_depth
template <typename K, typename V>
void ExtendibleHashTable<K, V>::SplitBucketAt(std::shared_ptr<Bucket> bucket, int old_local_depth) {
  // create new bucket with local depth = old_local_depth + 1
  auto new_bucket = std::make_shared<Bucket>(bucket_size_, old_local_depth + 1);
  bucket->IncrementDepth();
  // reassign directory pointers: for entries that pointed to 'bucket', decide by bit at position old_local_depth
  size_t dir_size = dir_.size();
  for (size_t i = 0; i < dir_size; ++i) {
    if (dir_[i] == bucket) {
      size_t bit = (i >> old_local_depth) & 1;
      if (bit == 1) {
        dir_[i] = new_bucket;
      }
    }
  }
  ++num_buckets_;

  // redistribute items (move then reinsert to appropriate bucket)
  std::list<std::pair<K, V>> moved;
  moved.splice(moved.begin(), bucket->GetItems());
  for (auto &pr : moved) {
    size_t new_idx = IndexOf(pr.first);
    dir_[new_idx]->GetItems().emplace_back(std::move(pr));
  }
}

template <typename K, typename V>
void ExtendibleHashTable<K, V>::Insert(const K &key, const V &value) {
  std::scoped_lock<std::mutex> lock(latch_);

  while (true) {
    size_t idx = IndexOf(key);
    auto bucket = dir_[idx];
    if (!bucket) {
      // defensive: create a bucket if unexpectedly null
      bucket = std::make_shared<Bucket>(bucket_size_, 0);
      dir_[idx] = bucket;
      num_buckets_ = std::max(num_buckets_, 1);
    }

    // if key exists, update
    V found_val;
    if (bucket->Find(key, found_val)) {
      for (auto &p : bucket->GetItems()) {
        if (p.first == key) {
          p.second = value;
          return;
        }
      }
    }

    if (!bucket->IsFull()) {
      bucket->Insert(key, value);
      return;
    }

    // need to split
    int local_depth = bucket->GetDepth();
    if (local_depth == global_depth_) {
      DoubleDirectory();
    }
    // split this bucket
    SplitBucketAt(bucket, local_depth);
    // loop to retry insertion
  }
}

// Bucket methods
template <typename K, typename V>
ExtendibleHashTable<K, V>::Bucket::Bucket(size_t size, int depth) : size_(size), depth_(depth) {}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::Find(const K &key, V &value) -> bool {
  for (auto &it : list_) {
    if (it.first == key) {
      value = it.second;
      return true;
    }
  }
  return false;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::Remove(const K &key) -> bool {
  for (auto it = list_.begin(); it != list_.end(); ++it) {
    if (it->first == key) {
      list_.erase(it);
      return true;
    }
  }
  return false;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::Insert(const K &key, const V &value) -> bool {
  for (auto &p : list_) {
    if (p.first == key) {
      p.second = value;
      return true;
    }
  }
  if (IsFull()) return false;
  list_.emplace_back(key, value);
  return true;
}

// explicit instantiation
template class ExtendibleHashTable<page_id_t, Page *>;
template class ExtendibleHashTable<Page *, std::list<Page *>::iterator>;
template class ExtendibleHashTable<int, int>;
template class ExtendibleHashTable<int, std::string>;
template class ExtendibleHashTable<int, std::list<int>::iterator>;

}  // namespace bustub

