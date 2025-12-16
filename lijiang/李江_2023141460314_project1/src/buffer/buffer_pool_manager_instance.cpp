#include "buffer/buffer_pool_manager_instance.h"

#include "common/exception.h"

namespace bustub {

BufferPoolManagerInstance::BufferPoolManagerInstance(size_t pool_size, DiskManager *disk_manager, size_t replacer_k,
                                                     LogManager *log_manager)
    : pool_size_(pool_size), pages_(nullptr), disk_manager_(disk_manager), log_manager_(log_manager) {
  pages_ = new Page[pool_size_];
  page_table_ = new ExtendibleHashTable<page_id_t, frame_id_t>(bucket_size_);
  replacer_ = new LRUKReplacer(pool_size_, replacer_k);

  for (size_t i = 0; i < pool_size_; ++i) {
    pages_[i].page_id_ = INVALID_PAGE_ID;
    pages_[i].pin_count_ = 0;
    pages_[i].is_dirty_ = false;
    free_list_.push_back(static_cast<int>(i));
  }
}

BufferPoolManagerInstance::~BufferPoolManagerInstance() {
  delete[] pages_;
  delete page_table_;
  delete replacer_;
}

// Helper: obtain a victim frame id (from free_list_ or replacer). Return -1 if none.
frame_id_t FindVictimFrameHelper(std::list<frame_id_t> &free_list, LRUKReplacer *replacer) {
  if (!free_list.empty()) {
    frame_id_t fid = free_list.front();
    free_list.pop_front();
    return fid;
  }
  frame_id_t victim = -1;
  if (replacer->Evict(&victim)) {
    return victim;
  }
  return -1;
}

auto BufferPoolManagerInstance::NewPgImp(page_id_t *page_id) -> Page * {
  std::scoped_lock<std::mutex> lock(latch_);

  frame_id_t fid = FindVictimFrameHelper(free_list_, replacer_);
  if (fid == -1) return nullptr;

  // if existing page present, flush if dirty and remove mapping
  Page *frame = &pages_[fid];
  if (frame->GetPageId() != INVALID_PAGE_ID) {
    if (frame->IsDirty()) {
      disk_manager_->WritePage(frame->GetPageId(), frame->GetData());
      frame->is_dirty_ = false;
    }
    page_table_->Remove(frame->GetPageId());
  }

  // allocate new page id and initialize frame
  *page_id = AllocatePage();
  frame->ResetMemory();
  frame->page_id_ = *page_id;
  frame->pin_count_ = 1;
  frame->is_dirty_ = false;

  page_table_->Insert(*page_id, fid);
  replacer_->RecordAccess(fid);
  replacer_->SetEvictable(fid, false);

  return frame;
}

auto BufferPoolManagerInstance::FetchPgImp(page_id_t page_id) -> Page * {
  std::scoped_lock<std::mutex> lock(latch_);
  frame_id_t fid;
  if (page_table_->Find(page_id, fid)) {
    Page *p = &pages_[fid];
    p->pin_count_++;
    replacer_->RecordAccess(fid);
    replacer_->SetEvictable(fid, false);
    return p;
  }

  frame_id_t victim = FindVictimFrameHelper(free_list_, replacer_);
  if (victim == -1) return nullptr;

  Page *frame = &pages_[victim];
  if (frame->GetPageId() != INVALID_PAGE_ID) {
    if (frame->IsDirty()) {
      disk_manager_->WritePage(frame->GetPageId(), frame->GetData());
      frame->is_dirty_ = false;
    }
    page_table_->Remove(frame->GetPageId());
  }

  // read requested page into frame
  disk_manager_->ReadPage(page_id, frame->GetData());
  frame->page_id_ = page_id;
  frame->pin_count_ = 1;
  frame->is_dirty_ = false;

  page_table_->Insert(page_id, victim);
  replacer_->RecordAccess(victim);
  replacer_->SetEvictable(victim, false);

  return frame;
}

auto BufferPoolManagerInstance::UnpinPgImp(page_id_t page_id, bool is_dirty) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  frame_id_t fid;
  if (!page_table_->Find(page_id, fid)) return false;
  Page *p = &pages_[fid];
  if (p->pin_count_ <= 0) return false;
  p->pin_count_--;
  if (is_dirty) p->is_dirty_ = true;
  if (p->pin_count_ == 0) replacer_->SetEvictable(fid, true);
  return true;
}

auto BufferPoolManagerInstance::FlushPgImp(page_id_t page_id) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  frame_id_t fid;
  if (!page_table_->Find(page_id, fid)) return false;
  Page *p = &pages_[fid];
  disk_manager_->WritePage(page_id, p->GetData());
  p->is_dirty_ = false;
  return true;
}

void BufferPoolManagerInstance::FlushAllPgsImp() {
  std::scoped_lock<std::mutex> lock(latch_);
  for (size_t i = 0; i < pool_size_; ++i) {
    Page *p = &pages_[i];
    if (p->GetPageId() != INVALID_PAGE_ID && p->IsDirty()) {
      disk_manager_->WritePage(p->GetPageId(), p->GetData());
      p->is_dirty_ = false;
    }
  }
}

auto BufferPoolManagerInstance::DeletePgImp(page_id_t page_id) -> bool {
  std::scoped_lock<std::mutex> lock(latch_);
  frame_id_t fid;
  if (!page_table_->Find(page_id, fid)) {
    DeallocatePage(page_id);
    return true;
  }

  Page *p = &pages_[fid];
  if (p->pin_count_ > 0) return false;

  page_table_->Remove(page_id);
  try {
    replacer_->SetEvictable(fid, true);
    replacer_->Remove(fid);
  } catch (...) {
    // ignore
  }

  p->ResetMemory();
  p->page_id_ = INVALID_PAGE_ID;
  p->pin_count_ = 0;
  p->is_dirty_ = false;
  free_list_.push_back(fid);

  DeallocatePage(page_id);
  return true;
}

auto BufferPoolManagerInstance::AllocatePage() -> page_id_t { return next_page_id_++; }

}  // namespace bustub
// 补充缺失的实现
void bustub::BufferPoolManagerInstance::DeallocatePage(int page_id) {
  // 暂时为空实现（根据项目需求后续补充）
}

