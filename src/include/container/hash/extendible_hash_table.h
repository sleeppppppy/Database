#pragma once

#include <list>
#include <memory>
#include <mutex>
#include <vector>
#include <unordered_set>
#include "common/config.h"

namespace bustub {

#define BUCKET_SIZE 2

template <typename K, typename V>
class ExtendibleHashTable {
 public:
  class Bucket {
   public:
    explicit Bucket(size_t depth);
    auto IsFull() const -> bool;
    auto IsEmpty() const -> bool;
    auto GetDepth() const -> int;
    auto IncreaseDepth() -> void;
    auto Find(const K &key, V &value) -> bool;
    auto Remove(const K &key) -> bool;
    auto Insert(const K &key, const V &value) -> bool;
    auto GetItems() -> std::list<std::pair<K, V>> &;

   private:
    int depth_;
    std::list<std::pair<K, V>> items_;
    mutable std::mutex latch_;
  };

  explicit ExtendibleHashTable(size_t bucket_size = BUCKET_SIZE);
  auto Find(const K &key, V &value) -> bool;
  auto Insert(const K &key, const V &value) -> bool;
  auto Remove(const K &key) -> bool;
  auto GetGlobalDepth() const -> int;
  auto GetLocalDepth(int directory_index) const -> int;
  auto GetNumBuckets() const -> int;

 private:
  void SplitBucket(int directory_index);
  auto IndexOf(const K &key) const -> int;

  size_t bucket_size_;
  int global_depth_;
  mutable std::mutex latch_;
  std::vector<std::shared_ptr<Bucket>> directory_;
};

}  // namespace bustub
