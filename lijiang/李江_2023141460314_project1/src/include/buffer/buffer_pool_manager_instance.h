#pragma once

#include <list>
#include <mutex>  // NOLINT
#include <atomic>

#include "buffer/buffer_pool_manager.h"
#include "buffer/lru_k_replacer.h"
#include "common/config.h"
#include "container/hash/extendible_hash_table.h"
#include "recovery/log_manager.h"
#include "storage/disk/disk_manager.h"
#include "storage/page/page.h"

namespace bustub {

/**
 * A concrete buffer pool manager instance.
 */
class BufferPoolManagerInstance : public BufferPoolManager {
 public:
  BufferPoolManagerInstance(size_t pool_size, DiskManager *disk_manager, size_t replacer_k = LRUK_REPLACER_K,
                            LogManager *log_manager = nullptr);

  ~BufferPoolManagerInstance() override;

  auto GetPoolSize() -> size_t override { return pool_size_; }
  auto GetPages() -> Page * { return pages_; }

 protected:
  // === Core operations ===
  auto NewPgImp(page_id_t *page_id) -> Page * override;
  auto FetchPgImp(page_id_t page_id) -> Page * override;
  auto UnpinPgImp(page_id_t page_id, bool is_dirty) -> bool override;
  auto FlushPgImp(page_id_t page_id) -> bool override;
  void FlushAllPgsImp() override;
  auto DeletePgImp(page_id_t page_id) -> bool override;

  // === Internal allocation ===
  auto AllocatePage() -> page_id_t;
  void DeallocatePage(page_id_t page_id);

 private:
  // === Config ===
  const size_t pool_size_;
  const size_t bucket_size_ = 4;

  // === Memory space ===
  Page *pages_;
  std::atomic<page_id_t> next_page_id_{0};

  // === Managers ===
  DiskManager *disk_manager_;
  LogManager *log_manager_;
  ExtendibleHashTable<page_id_t, frame_id_t> *page_table_;
  LRUKReplacer *replacer_;

  // === Frame tracking ===
  std::list<frame_id_t> free_list_;

  // === Synchronization ===
  std::mutex latch_;
};

}  // namespace bustub

