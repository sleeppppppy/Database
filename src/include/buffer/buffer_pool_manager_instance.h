#pragma once

#include <list>
#include <mutex>
#include <unordered_map>
#include "buffer/buffer_pool_manager.h"
#include "container/hash/extendible_hash_table.h"
#include "buffer/lru_k_replacer.h"

namespace bustub {

class BufferPoolManagerInstance : public BufferPoolManager {
 public:
  BufferPoolManagerInstance(size_t pool_size, DiskManager *disk_manager, size_t replacer_k = LRUK_REPLACER_K);

  ~BufferPoolManagerInstance() override;

  auto NewPgImp(page_id_t *page_id) -> Page * override;
  auto FetchPgImp(page_id_t page_id) -> Page * override;
  auto UnpinPgImp(page_id_t page_id, bool is_dirty) -> bool override;
  auto FlushPgImp(page_id_t page_id) -> bool override;
  auto DeletePgImp(page_id_t page_id) -> bool override;
  auto FlushAllPgsImp() -> void override;

  auto GetAvailableFrame(frame_id_t *frame_id) -> bool;

 protected:
  size_t pool_size_;
  Page *pages_;
  DiskManager *disk_manager_;
  std::unique_ptr<ExtendibleHashTable<page_id_t, frame_id_t>> page_table_;
  std::unique_ptr<LRUKReplacer> replacer_;
  std::list<frame_id_t> free_list_;
  std::mutex latch_;
};

}  // namespace bustub
