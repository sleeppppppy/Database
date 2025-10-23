#include "buffer/buffer_pool_manager_instance.h"

namespace bustub {

BufferPoolManagerInstance::BufferPoolManagerInstance(size_t pool_size, DiskManager *disk_manager, size_t replacer_k)
    : pool_size_(pool_size), disk_manager_(disk_manager) {
  pages_ = new Page[pool_size_];
  page_table_ = std::make_unique<ExtendibleHashTable<page_id_t, frame_id_t>>();
  replacer_ = std::make_unique<LRUKReplacer>(pool_size, replacer_k);
  
  for (size_t i = 0; i < pool_size_; i++) {
    free_list_.push_back(static_cast<frame_id_t>(i));
  }
}

BufferPoolManagerInstance::~BufferPoolManagerInstance() {
  delete[] pages_;
}

auto BufferPoolManagerInstance::NewPgImp(page_id_t *page_id) -> Page * {
  std::scoped_lock lock(latch_);
  
  *page_id = AllocatePage();
  
  frame_id_t frame_id;
  if (!GetAvailableFrame(&frame_id)) {
    return nullptr;
  }
  
  Page *page = &pages_[frame_id];
  page->ResetMemory();
  page->page_id_ = *page_id;
  page->pin_count_ = 1;
  page->is_dirty_ = false;
  
  page_table_->Insert(*page_id, frame_id);
  replacer_->RecordAccess(frame_id);
  replacer_->SetEvictable(frame_id, false);
  
  return page;
}

auto BufferPoolManagerInstance::FetchPgImp(page_id_t page_id) -> Page * {
  std::scoped_lock lock(latch_);
  
  frame_id_t frame_id;
  
  if (page_table_->Find(page_id, frame_id)) {
    Page *page = &pages_[frame_id];
    page->pin_count_++;
    replacer_->RecordAccess(frame_id);
    replacer_->SetEvictable(frame_id, false);
    return page;
  }
  
  if (!GetAvailableFrame(&frame_id)) {
    return nullptr;
  }
  
  Page *page = &pages_[frame_id];
  page->ResetMemory();
  disk_manager_->ReadPage(page_id, page->GetData());
  page->page_id_ = page_id;
  page->pin_count_ = 1;
  page->is_dirty_ = false;
  
  page_table_->Insert(page_id, frame_id);
  replacer_->RecordAccess(frame_id);
  replacer_->SetEvictable(frame_id, false);
  
  return page;
}

auto BufferPoolManagerInstance::UnpinPgImp(page_id_t page_id, bool is_dirty) -> bool {
  std::scoped_lock lock(latch_);
  
  frame_id_t frame_id;
  if (!page_table_->Find(page_id, frame_id)) {
    return false;
  }
  
  Page *page = &pages_[frame_id];
  
  if (page->pin_count_ == 0) {
    return false;
  }
  
  page->pin_count_--;
  
  if (is_dirty) {
    page->is_dirty_ = true;
  }
  
  if (page->pin_count_ == 0) {
    replacer_->SetEvictable(frame_id, true);
  }
  
  return true;
}

auto BufferPoolManagerInstance::FlushPgImp(page_id_t page_id) -> bool {
  std::scoped_lock lock(latch_);
  
  frame_id_t frame_id;
  if (!page_table_->Find(page_id, frame_id)) {
    return false;
  }
  
  Page *page = &pages_[frame_id];
  disk_manager_->WritePage(page_id, page->GetData());
  page->is_dirty_ = false;
  
  return true;
}

auto BufferPoolManagerInstance::DeletePgImp(page_id_t page_id) -> bool {
  std::scoped_lock lock(latch_);
  
  frame_id_t frame_id;
  if (!page_table_->Find(page_id, frame_id)) {
    return true;
  }
  
  Page *page = &pages_[frame_id];
  
  if (page->pin_count_ > 0) {
    return false;
  }
  
  if (page->is_dirty_) {
    disk_manager_->WritePage(page_id, page->GetData());
  }
  
  page_table_->Remove(page_id);
  replacer_->Remove(frame_id);
  
  page->ResetMemory();
  page->page_id_ = INVALID_PAGE_ID;
  page->pin_count_ = 0;
  page->is_dirty_ = false;
  
  free_list_.push_back(frame_id);
  
  DeallocatePage(page_id);
  
  return true;
}

auto BufferPoolManagerInstance::FlushAllPgsImp() -> void {
  std::scoped_lock lock(latch_);
  
  for (size_t i = 0; i < pool_size_; i++) {
    if (pages_[i].page_id_ != INVALID_PAGE_ID && pages_[i].is_dirty_) {
      disk_manager_->WritePage(pages_[i].page_id_, pages_[i].GetData());
      pages_[i].is_dirty_ = false;
    }
  }
}

auto BufferPoolManagerInstance::GetAvailableFrame(frame_id_t *frame_id) -> bool {
  if (!free_list_.empty()) {
    *frame_id = free_list_.front();
    free_list_.pop_front();
    return true;
  }
  
  if (replacer_->Evict(frame_id)) {
    Page *page = &pages_[*frame_id];
    
    if (page->is_dirty_) {
      disk_manager_->WritePage(page->page_id_, page->GetData());
      page->is_dirty_ = false;
    }
    
    page_table_->Remove(page->page_id_);
    
    page->ResetMemory();
    page->page_id_ = INVALID_PAGE_ID;
    page->pin_count_ = 0;
    
    return true;
  }
  
  return false;
}

}  // namespace bustub
