#pragma once

#include <memory>
#include <mutex>  // NOLINT
#include <vector>
#include <list>
#include <utility>

namespace bustub {

/**
 * Extendible Hash Table Implementation.
 * Thread-safe with internal latch_.
 */
template <typename K, typename V>
class ExtendibleHashTable {
 public:
  /** Construct a new Extendible Hash Table instance. */
  explicit ExtendibleHashTable(size_t bucket_size);

  /** Disable copy/move. */
  ExtendibleHashTable(const ExtendibleHashTable &) = delete;
  ExtendibleHashTable &operator=(const ExtendibleHashTable &) = delete;

  /** Get the global depth of directory. */
  auto GetGlobalDepth() const -> int;
  /** Get the local depth of a specific directory bucket. */
  auto GetLocalDepth(int dir_index) const -> int;
  /** Get total number of buckets. */
  auto GetNumBuckets() const -> int;

  /** Find the value associated with a key. Return false if absent. */
  auto Find(const K &key, V &value) -> bool;

  /** Remove a key-value pair. Return false if not found. */
  auto Remove(const K &key) -> bool;

  /** Insert or update a key-value pair. */
  void Insert(const K &key, const V &value);

 private:
  // === Internal bucket structure ===
  class Bucket {
   public:
    explicit Bucket(size_t array_size, int depth);
    auto IsFull() const -> bool { return list_.size() >= size_; }
    auto GetDepth() const -> int { return depth_; }
    void IncrementDepth() { depth_++; }
    auto GetItems() -> std::list<std::pair<K, V>> & { return list_; }

    auto Find(const K &key, V &value) -> bool;
    auto Remove(const K &key) -> bool;
    auto Insert(const K &key, const V &value) -> bool;

   private:
    size_t size_;
    int depth_;
    std::list<std::pair<K, V>> list_;
  };

  // === Private helpers ===
  auto IndexOf(const K &key) -> size_t;
  auto IndexMask() const -> size_t;
  void DoubleDirectory();
  void SplitBucketAt(std::shared_ptr<Bucket> bucket, int old_local_depth);

  // === Internal getters (thread-unsafe) ===
  auto GetGlobalDepthInternal() const -> int { return global_depth_; }
  auto GetLocalDepthInternal(int dir_index) const -> int;
  auto GetNumBucketsInternal() const -> int { return num_buckets_; }

  // === Data members ===
  int global_depth_{0};              // number of bits used for indexing
  size_t bucket_size_;               // maximum number of pairs in one bucket
  int num_buckets_{0};               // number of unique buckets
  std::vector<std::shared_ptr<Bucket>> dir_;  // directory mapping index → bucket
  mutable std::mutex latch_;         // thread safety
};

}  // namespace bustub

