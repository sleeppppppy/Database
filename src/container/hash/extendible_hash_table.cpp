#include "container/hash/extendible_hash_table.h"
#include <functional>
#include <iostream>
#include "common/config.h"

namespace bustub {

template <typename K, typename V>
ExtendibleHashTable<K, V>::Bucket::Bucket(size_t depth) : depth_(depth) {}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::IsFull() const -> bool {
  return items_.size() >= BUCKET_SIZE;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::IsEmpty() const -> bool {
  return items_.empty();
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::GetDepth() const -> int {
  return depth_;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::IncreaseDepth() -> void {
  depth_++;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::Find(const K &key, V &value) -> bool {
  std::scoped_lock lock(latch_);
  for (const auto &item : items_) {
    if (item.first == key) {
      value = item.second;
      return true;
    }
  }
  return false;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::Remove(const K &key) -> bool {
  std::scoped_lock lock(latch_);
  for (auto it = items_.begin(); it != items_.end(); ++it) {
    if (it->first == key) {
      items_.erase(it);
      return true;
    }
  }
  return false;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::Insert(const K &key, const V &value) -> bool {
  std::scoped_lock lock(latch_);
  
  for (auto &item : items_) {
    if (item.first == key) {
      item.second = value;
      return true;
    }
  }
  
  if (!IsFull()) {
    items_.emplace_back(key, value);
    return true;
  }
  
  return false;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Bucket::GetItems() -> std::list<std::pair<K, V>> & {
  return items_;
}

template <typename K, typename V>
ExtendibleHashTable<K, V>::ExtendibleHashTable(size_t bucket_size)
    : bucket_size_(bucket_size), global_depth_(0) {
  directory_.push_back(std::make_shared<Bucket>(global_depth_));
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Find(const K &key, V &value) -> bool {
  std::scoped_lock lock(latch_);
  int index = IndexOf(key);
  auto bucket = directory_[index];
  return bucket->Find(key, value);
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Insert(const K &key, const V &value) -> bool {
  std::scoped_lock lock(latch_);
  
  while (true) {
    int index = IndexOf(key);
    auto bucket = directory_[index];
    
    if (bucket->Insert(key, value)) {
      return true;
    }
    
    SplitBucket(index);
  }
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::Remove(const K &key) -> bool {
  std::scoped_lock lock(latch_);
  int index = IndexOf(key);
  auto bucket = directory_[index];
  return bucket->Remove(key);
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetGlobalDepth() const -> int {
  std::scoped_lock lock(latch_);
  return global_depth_;
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetLocalDepth(int directory_index) const -> int {
  std::scoped_lock lock(latch_);
  return directory_[directory_index]->GetDepth();
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::GetNumBuckets() const -> int {
  std::scoped_lock lock(latch_);
  std::unordered_set<std::shared_ptr<Bucket>> unique_buckets;
  for (auto &bucket : directory_) {
    unique_buckets.insert(bucket);
  }
  return unique_buckets.size();
}

template <typename K, typename V>
void ExtendibleHashTable<K, V>::SplitBucket(int directory_index) {
  auto old_bucket = directory_[directory_index];
  int local_depth = old_bucket->GetDepth();
  
  if (local_depth == global_depth_) {
    size_t old_size = directory_.size();
    directory_.resize(old_size * 2);
    
    for (size_t i = 0; i < old_size; i++) {
      directory_[i + old_size] = directory_[i];
    }
    global_depth_++;
  }
  
  auto new_bucket = std::make_shared<Bucket>(local_depth + 1);
  old_bucket->IncreaseDepth();
  
  int high_bit = 1 << local_depth;
  for (size_t i = 0; i < directory_.size(); i++) {
    if (directory_[i] == old_bucket && (i & high_bit)) {
      directory_[i] = new_bucket;
    }
  }
  
  auto &items = old_bucket->GetItems();
  std::list<std::pair<K, V>> new_items;
  
  for (auto it = items.begin(); it != items.end(); ) {
    int new_index = IndexOf(it->first);
    if (directory_[new_index] == new_bucket) {
      new_items.splice(new_items.end(), items, it++);
    } else {
      ++it;
    }
  }
  
  for (auto &item : new_items) {
    new_bucket->Insert(item.first, item.second);
  }
}

template <typename K, typename V>
auto ExtendibleHashTable<K, V>::IndexOf(const K &key) const -> int {
  return std::hash<K>()(key) & ((1 << global_depth_) - 1);
}

template class ExtendibleHashTable<int, int>;
template class ExtendibleHashTable<page_id_t, frame_id_t>;

}  // namespace bustub
