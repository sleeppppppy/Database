#include <string>

#include "common/exception.h"
#include "common/logger.h"
#include "common/rid.h"
#include "storage/index/b_plus_tree.h"
#include "storage/page/header_page.h"

namespace bustub {
INDEX_TEMPLATE_ARGUMENTS
BPLUSTREE_TYPE::BPlusTree(std::string name, BufferPoolManager *buffer_pool_manager, const KeyComparator &comparator,
                          int leaf_max_size, int internal_max_size)
    : index_name_(std::move(name)),
      root_page_id_(INVALID_PAGE_ID),
      buffer_pool_manager_(buffer_pool_manager),
      comparator_(comparator),
      leaf_max_size_(leaf_max_size),
      internal_max_size_(internal_max_size) {}

/*
 * Helper function to determine whether current b+ tree is empty
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::IsEmpty() const -> bool { return root_page_id_ == INVALID_PAGE_ID; }
/*****************************************************************************
 * SEARCH
 *****************************************************************************/
/*
 * Return the only value that associated with input key
 * This method is used for point query
 * @return : true means key exists
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetValue(const KeyType &key, std::vector<ValueType> *result, Transaction *transaction) -> bool {
  root_page_id_latch_.RLock();
  auto leaf_page = FindLeaf(key, Operation::SEARCH, transaction);
  auto *leaf_node = reinterpret_cast<LeafPage *>(leaf_page->GetData());

  ValueType value;
  auto key_exists = leaf_node->Lookup(key, &value, comparator_);

  leaf_page->RUnlatch();
  buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);

  if (!key_exists) {
    return false;
  }

  result->push_back(value);
  return true;
}

/*****************************************************************************
 * INSERTION
 *****************************************************************************/
/*
 * Insert constant key & value pair into b+ tree
 * if current tree is empty, start a new tree, update root page id and insert
 * entry, otherwise insert into leaf page.
 * @return: since we only support unique key, if user try to insert duplicate
 * keys return false, otherwise return true.
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Insert(const KeyType &key, const ValueType &value, Transaction *transaction) -> bool {
  root_page_id_latch_.WLock();
  transaction->AddIntoPageSet(nullptr);  // nullptr represents root_page_id_latch_
  if (IsEmpty()) {
    StartNewTree(key, value);
    ReleaseLatchFromQueue(transaction);
    return true;
  }
  return InsertIntoLeaf(key, value, transaction);
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::StartNewTree(const KeyType &key, const ValueType &value) {
  auto *new_page = buffer_pool_manager_->NewPage(&root_page_id_);

  if (new_page == nullptr) {
    throw Exception(ExceptionType::OUT_OF_MEMORY, "Cannot allocate new page");
  }

  auto *leaf = reinterpret_cast<LeafPage *>(new_page->GetData());
  leaf->Init(root_page_id_, INVALID_PAGE_ID, leaf_max_size_);

  leaf->Insert(key, value, comparator_);

  buffer_pool_manager_->UnpinPage(new_page->GetPageId(), true);

  // UpdateRootPageId(1);
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::InsertIntoLeaf(const KeyType &key, const ValueType &value, Transaction *transaction) -> bool {
  auto leaf_page = FindLeaf(key, Operation::INSERT, transaction);
  auto *leaf_node = reinterpret_cast<LeafPage *>(leaf_page->GetData());

  auto original_size = leaf_node->GetSize();
  auto updated_size = leaf_node->Insert(key, value, comparator_);

  // Duplicate key
  if (updated_size == original_size) {
    ReleaseLatchFromQueue(transaction);
    leaf_page->WUnlatch();
    buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
    return false;
  }

  // Leaf node is not full
  if (updated_size < leaf_max_size_) {
    ReleaseLatchFromQueue(transaction);
    leaf_page->WUnlatch();
    buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), true);
    return true;
  }

  // Leaf node is full, need to split
  auto *split_leaf_node = Split(leaf_node);
  split_leaf_node->SetNextPageId(leaf_node->GetNextPageId());
  leaf_node->SetNextPageId(split_leaf_node->GetPageId());

  auto rising_key = split_leaf_node->KeyAt(0);
  InsertIntoParent(leaf_node, rising_key, split_leaf_node, transaction);

  leaf_page->WUnlatch();
  buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), true);
  buffer_pool_manager_->UnpinPage(split_leaf_node->GetPageId(), true);
  return true;
}

INDEX_TEMPLATE_ARGUMENTS
template <typename N>
auto BPLUSTREE_TYPE::Split(N *node) -> N * {
  page_id_t new_page_id;
  auto *new_page = buffer_pool_manager_->NewPage(&new_page_id);

  if (new_page == nullptr) {
    throw Exception(ExceptionType::OUT_OF_MEMORY, "Cannot allocate new page");
  }

  N *new_node = reinterpret_cast<N *>(new_page->GetData());
  new_node->SetPageType(node->GetPageType());

  if (node->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(node);
    auto *new_leaf = reinterpret_cast<LeafPage *>(new_node);

    new_leaf->Init(new_page->GetPageId(), node->GetParentPageId(), leaf_max_size_);
    leaf->MoveHalfTo(new_leaf);
  } else {
    auto *internal = reinterpret_cast<InternalPage *>(node);
    auto *new_internal = reinterpret_cast<InternalPage *>(new_node);

    new_internal->Init(new_page->GetPageId(), node->GetParentPageId(), internal_max_size_);
    internal->MoveHalfTo(new_internal, buffer_pool_manager_);
  }

  return new_node;
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertIntoParent(BPlusTreePage *old_node, const KeyType &key, BPlusTreePage *new_node,
                                      Transaction *transaction) {
  if (old_node->IsRootPage()) {
    auto *new_page = buffer_pool_manager_->NewPage(&root_page_id_);

    if (new_page == nullptr) {
      throw Exception(ExceptionType::OUT_OF_MEMORY, "Cannot allocate new page");
    }

    auto *new_root = reinterpret_cast<InternalPage *>(new_page->GetData());
    new_root->Init(root_page_id_, INVALID_PAGE_ID, internal_max_size_);

    new_root->PopulateNewRoot(old_node->GetPageId(), key, new_node->GetPageId());

    old_node->SetParentPageId(new_root->GetPageId());
    new_node->SetParentPageId(new_root->GetPageId());

    buffer_pool_manager_->UnpinPage(new_page->GetPageId(), true);

    UpdateRootPageId(0);

    ReleaseLatchFromQueue(transaction);
    return;
  }
  auto *parent_page = buffer_pool_manager_->FetchPage(old_node->GetParentPageId());
  auto *parent_node = reinterpret_cast<InternalPage *>(parent_page->GetData());

  if (parent_node->GetSize() < internal_max_size_) {
    parent_node->InsertNodeAfter(old_node->GetPageId(), key, new_node->GetPageId());
    ReleaseLatchFromQueue(transaction);
    buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
    return;
  }
  auto *temporary_memory = new char[INTERNAL_PAGE_HEADER_SIZE + sizeof(MappingType) * (parent_node->GetSize() + 1)];
  auto *copied_parent_node = reinterpret_cast<InternalPage *>(temporary_memory);
  std::memcpy(temporary_memory, parent_page->GetData(), INTERNAL_PAGE_HEADER_SIZE + sizeof(MappingType) * (parent_node->GetSize()));
  copied_parent_node->InsertNodeAfter(old_node->GetPageId(), key, new_node->GetPageId());
  auto *split_parent_sibling_node = Split(copied_parent_node);
  KeyType new_key = split_parent_sibling_node->KeyAt(0);
  std::memcpy(parent_page->GetData(), temporary_memory,
              INTERNAL_PAGE_HEADER_SIZE + sizeof(MappingType) * copied_parent_node->GetMinSize());
  InsertIntoParent(parent_node, new_key, split_parent_sibling_node, transaction);
  buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
  buffer_pool_manager_->UnpinPage(split_parent_sibling_node->GetPageId(), true);
  delete[] temporary_memory;
}

/*****************************************************************************
 * REMOVE
 *****************************************************************************/
/*
 * Delete key & value pair associated with input key
 * If current tree is empty, return immediately.
 * If not, User needs to first find the right leaf page as deletion target, then
 * delete entry from leaf page. Remember to deal with redistribute or merge if
 * necessary.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Remove(const KeyType &key, Transaction *transaction) {
  root_page_id_latch_.WLock();
  transaction->AddIntoPageSet(nullptr);  // nullptr represents root_page_id_latch_

  if (IsEmpty()) {
    ReleaseLatchFromQueue(transaction);
    return;
  }

  auto leaf_page = FindLeaf(key, Operation::DELETE, transaction);
  auto *leaf_node = reinterpret_cast<LeafPage *>(leaf_page->GetData());

  if (leaf_node->GetSize() == leaf_node->RemoveAndDeleteRecord(key, comparator_)) {
    ReleaseLatchFromQueue(transaction);
    leaf_page->WUnlatch();
    buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), false);
    return;
  }

  auto node_should_delete = CoalesceOrRedistribute(leaf_node, transaction);
  leaf_page->WUnlatch();

  if (node_should_delete) {
    transaction->AddIntoDeletedPageSet(leaf_node->GetPageId());
  }

  buffer_pool_manager_->UnpinPage(leaf_page->GetPageId(), true);

  std::for_each(transaction->GetDeletedPageSet()->begin(), transaction->GetDeletedPageSet()->end(),
                [&bpm = buffer_pool_manager_](const page_id_t page_id) { bpm->DeletePage(page_id); });
  transaction->GetDeletedPageSet()->clear();
}

INDEX_TEMPLATE_ARGUMENTS
template <typename N>
auto BPLUSTREE_TYPE::CoalesceOrRedistribute(N *node, Transaction *transaction) -> bool {
  if (node->IsRootPage()) {
    auto root_should_delete = AdjustRoot(node);
    ReleaseLatchFromQueue(transaction);
    return root_should_delete;
  }

  if (node->GetSize() >= node->GetMinSize()) {
    ReleaseLatchFromQueue(transaction);
    return false;
  }

  auto parent_page = buffer_pool_manager_->FetchPage(node->GetParentPageId());
  auto *parent_node = reinterpret_cast<InternalPage *>(parent_page->GetData());
  auto index_in_parent = parent_node->ValueIndex(node->GetPageId());

  if (index_in_parent > 0) {
    auto sibling_page = buffer_pool_manager_->FetchPage(parent_node->ValueAt(index_in_parent - 1));
    sibling_page->WLatch();
    N *sibling_node = reinterpret_cast<N *>(sibling_page->GetData());

    if (sibling_node->GetSize() > sibling_node->GetMinSize()) {
      Redistribute(sibling_node, node, parent_node, index_in_parent, true);

      ReleaseLatchFromQueue(transaction);

      buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
      sibling_page->WUnlatch();
      buffer_pool_manager_->UnpinPage(sibling_page->GetPageId(), true);
      return false;
    }

    // Coalesce
    auto parent_node_should_delete = Coalesce(sibling_node, node, parent_node, index_in_parent, transaction);

    if (parent_node_should_delete) {
      transaction->AddIntoDeletedPageSet(parent_node->GetPageId());
    }
    buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
    sibling_page->WUnlatch();
    buffer_pool_manager_->UnpinPage(sibling_page->GetPageId(), true);
    return true;
  }

  if (index_in_parent != parent_node->GetSize() - 1) {
    auto sibling_page = buffer_pool_manager_->FetchPage(parent_node->ValueAt(index_in_parent + 1));
    sibling_page->WLatch();
    N *sibling_node = reinterpret_cast<N *>(sibling_page->GetData());

    if (sibling_node->GetSize() > sibling_node->GetMinSize()) {
      Redistribute(sibling_node, node, parent_node, index_in_parent, false);

      ReleaseLatchFromQueue(transaction);

      buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
      sibling_page->WUnlatch();
      buffer_pool_manager_->UnpinPage(sibling_page->GetPageId(), true);
      return false;
    }
    // Coalesce
    auto sibling_index = parent_node->ValueIndex(sibling_node->GetPageId());
    auto parent_node_should_delete = Coalesce(node, sibling_node, parent_node, sibling_index, transaction);  // NOLINT
    transaction->AddIntoDeletedPageSet(sibling_node->GetPageId());
    if (parent_node_should_delete) {
      transaction->AddIntoDeletedPageSet(parent_node->GetPageId());
    }
    buffer_pool_manager_->UnpinPage(parent_page->GetPageId(), true);
    sibling_page->WUnlatch();
    buffer_pool_manager_->UnpinPage(sibling_page->GetPageId(), true);
    return false;
  }

  return false;
}

INDEX_TEMPLATE_ARGUMENTS
template <typename N>
auto BPLUSTREE_TYPE::Coalesce(N *neighbor_node, N *node,
                              BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *parent, int index,
                              Transaction *transaction) -> bool {
  auto middle_key = parent->KeyAt(index);

  if (node->IsLeafPage()) {
    auto *leaf_node = reinterpret_cast<LeafPage *>(node);
    auto *prev_leaf_node = reinterpret_cast<LeafPage *>(neighbor_node);
    leaf_node->MoveAllTo(prev_leaf_node);
  } else {
    auto *internal_node = reinterpret_cast<InternalPage *>(node);
    auto *prev_internal_node = reinterpret_cast<InternalPage *>(neighbor_node);
    internal_node->MoveAllTo(prev_internal_node, middle_key, buffer_pool_manager_);
  }

  parent->Remove(index);

  return CoalesceOrRedistribute(parent, transaction);
}

INDEX_TEMPLATE_ARGUMENTS
template <typename N>
void BPLUSTREE_TYPE::Redistribute(N *neighbor_node, N *node,
                                  BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *parent, int index,
                                  bool from_prev) {
  if (node->IsLeafPage()) {
    auto *leaf_node = reinterpret_cast<LeafPage *>(node);
    auto *neighbor_leaf_node = reinterpret_cast<LeafPage *>(neighbor_node);

    if (!from_prev) {
      neighbor_leaf_node->MoveFirstToEndOf(leaf_node);
      parent->SetKeyAt(index + 1, neighbor_leaf_node->KeyAt(0));
    } else {
      neighbor_leaf_node->MoveLastToFrontOf(leaf_node);
      parent->SetKeyAt(index, leaf_node->KeyAt(0));
    }
  } else {
    auto *internal_node = reinterpret_cast<InternalPage *>(node);
    auto *neighbor_internal_node = reinterpret_cast<InternalPage *>(neighbor_node);

    if (!from_prev) {
      neighbor_internal_node->MoveFirstToEndOf(internal_node, parent->KeyAt(index + 1), buffer_pool_manager_);
      parent->SetKeyAt(index + 1, neighbor_internal_node->KeyAt(0));
    } else {
      neighbor_internal_node->MoveLastToFrontOf(internal_node, parent->KeyAt(index), buffer_pool_manager_);
      parent->SetKeyAt(index, internal_node->KeyAt(0));
    }
  }
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::AdjustRoot(BPlusTreePage *old_root_node) -> bool {
  if (!old_root_node->IsLeafPage() && old_root_node->GetSize() == 1) {
    auto *root_node = reinterpret_cast<InternalPage *>(old_root_node);
    auto only_child_page = buffer_pool_manager_->FetchPage(root_node->ValueAt(0));
    auto *only_child_node = reinterpret_cast<BPlusTreePage *>(only_child_page->GetData());
    only_child_node->SetParentPageId(INVALID_PAGE_ID);

    root_page_id_ = only_child_node->GetPageId();

    UpdateRootPageId(0);

    buffer_pool_manager_->UnpinPage(only_child_page->GetPageId(), true);
    return true;
  }

  if (old_root_node->IsLeafPage() && old_root_node->GetSize() == 0) {
    root_page_id_ = INVALID_PAGE_ID;
    return true;
  }
  return false;
}
/*****************************************************************************
 * INDEX ITERATOR
 *****************************************************************************/
/*
 * Input parameter is void, find the leftmost leaf page first, then construct
 * index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin() -> INDEXITERATOR_TYPE {
  if (root_page_id_ == INVALID_PAGE_ID) {
    return INDEXITERATOR_TYPE(nullptr, nullptr);
  }
  root_page_id_latch_.RLock();
  auto leftmost_page = FindLeaf(KeyType(), Operation::SEARCH, nullptr, true);
  return INDEXITERATOR_TYPE(buffer_pool_manager_, leftmost_page, 0);
}

/*
 * Input parameter is low key, find the leaf page containing the input key
 * first, then construct index iterator
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::Begin(const KeyType &key) -> INDEXITERATOR_TYPE {
  if (root_page_id_ == INVALID_PAGE_ID) {
    return INDEXITERATOR_TYPE(nullptr, nullptr);
  }
  root_page_id_latch_.RLock();
  auto leaf_page = FindLeaf(key, Operation::SEARCH);
  auto *leaf_node = reinterpret_cast<LeafPage *>(leaf_page->GetData());
  auto index_in_node = leaf_node->KeyIndex(key, comparator_);
  return INDEXITERATOR_TYPE(buffer_pool_manager_, leaf_page, index_in_node);
}

/*
 * Input parameter is void, construct an index iterator representing the end
 * of the key/value pair in the leaf node
 * @return : index iterator
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::End() -> INDEXITERATOR_TYPE {
  if (root_page_id_ == INVALID_PAGE_ID) {
    return INDEXITERATOR_TYPE(nullptr, nullptr);
  }
  root_page_id_latch_.RLock();
  auto rightmost_page = FindLeaf(KeyType(), Operation::SEARCH, nullptr, false, true);
  auto *leaf_node = reinterpret_cast<LeafPage *>(rightmost_page->GetData());
  return INDEXITERATOR_TYPE(buffer_pool_manager_, rightmost_page, leaf_node->GetSize());
}

INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::FindLeaf(const KeyType &key, Operation operation, Transaction *transaction, bool leftMost,
                              bool rightMost) -> Page * {
  assert(operation == Operation::SEARCH ? !(leftMost && rightMost) : transaction != nullptr);

  assert(root_page_id_ != INVALID_PAGE_ID);
  auto page = buffer_pool_manager_->FetchPage(root_page_id_);
  auto *node = reinterpret_cast<BPlusTreePage *>(page->GetData());
  if (operation == Operation::SEARCH) {
    root_page_id_latch_.RUnlock();
    page->RLatch();
  } else {
    page->WLatch();
    if (operation == Operation::DELETE && node->GetSize() > 2) {
      ReleaseLatchFromQueue(transaction);
    }
    if (operation == Operation::INSERT && node->IsLeafPage() && node->GetSize() < node->GetMaxSize() - 1) {
      ReleaseLatchFromQueue(transaction);
    }
    if (operation == Operation::INSERT && !node->IsLeafPage() && node->GetSize() < node->GetMaxSize()) {
      ReleaseLatchFromQueue(transaction);
    }
  }

  while (!node->IsLeafPage()) {
    auto *internal_node = reinterpret_cast<InternalPage *>(node);

    page_id_t child_node_page_id;
    if (leftMost) {
      child_node_page_id = internal_node->ValueAt(0);
    } else if (rightMost) {
      child_node_page_id = internal_node->ValueAt(internal_node->GetSize() - 1);
    } else {
      child_node_page_id = internal_node->Lookup(key, comparator_);
    }
    assert(child_node_page_id > 0);

    auto child_page = buffer_pool_manager_->FetchPage(child_node_page_id);
    auto child_node = reinterpret_cast<BPlusTreePage *>(child_page->GetData());

    if (operation == Operation::SEARCH) {
      child_page->RLatch();
      page->RUnlatch();
      buffer_pool_manager_->UnpinPage(page->GetPageId(), false);
    } else if (operation == Operation::INSERT) {
      child_page->WLatch();
      transaction->AddIntoPageSet(page);

      // Child node is safe, release all locks on ancestors
      if (child_node->IsLeafPage() && child_node->GetSize() < child_node->GetMaxSize() - 1) {
        ReleaseLatchFromQueue(transaction);
      }
      if (!child_node->IsLeafPage() && child_node->GetSize() < child_node->GetMaxSize()) {
        ReleaseLatchFromQueue(transaction);
      }
    } else if (operation == Operation::DELETE) {
      child_page->WLatch();
      transaction->AddIntoPageSet(page);

      // Child node is safe, release all locks on ancestors
      if (child_node->GetSize() > child_node->GetMinSize()) {
        ReleaseLatchFromQueue(transaction);
      }
    }

    page = child_page;
    node = child_node;
  }

  return page;
}

INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ReleaseLatchFromQueue(Transaction *transaction) {
  while (!transaction->GetPageSet()->empty()) {
    Page *page = transaction->GetPageSet()->front();
    transaction->GetPageSet()->pop_front();
    if (page == nullptr) {
      this->root_page_id_latch_.WUnlock();
    } else {
      page->WUnlatch();
      buffer_pool_manager_->UnpinPage(page->GetPageId(), false);
    }
  }
}

/**
 * @return page id of root page
 */
INDEX_TEMPLATE_ARGUMENTS
auto BPLUSTREE_TYPE::GetRootPageId() -> page_id_t {
  root_page_id_latch_.RLock();
  root_page_id_latch_.RUnlock();
  return root_page_id_;
}

/*****************************************************************************
 * UTILITIES AND DEBUG
 *****************************************************************************/
/*
 * Update/Insert root page id (root page id and header page id) in header page
 * (header page should be guaranteed to be valid in this method) whenever
 * root page id is changed.
 * @parameter: insert_record default value is false. When set to true,
 * create a new record (index_name, root_page_id) in header page instead of
 * updating it.
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::UpdateRootPageId(int insert_record) {
  auto *header_page = static_cast<HeaderPage *>(buffer_pool_manager_->FetchPage(HEADER_PAGE_ID));
  if (insert_record != 0) {
    // create a new record<index_name + root_page_id> in header page
    header_page->InsertRecord(index_name_, root_page_id_);
  } else {
    // update root_page_id in header page
    header_page->UpdateRecord(index_name_, root_page_id_);
  }
  buffer_pool_manager_->UnpinPage(HEADER_PAGE_ID, true);
}

/*
 * This method is used for test only
 * Read data from file and insert one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::InsertFromFile(const std::string &file_name, Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;

    KeyType index_key;
    index_key.SetFromInteger(key);
    RID rid(key);
    Insert(index_key, rid, transaction);
  }
}
/*
 * This method is used for test only
 * Read data from file and remove one by one
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::RemoveFromFile(const std::string &file_name, Transaction *transaction) {
  int64_t key;
  std::ifstream input(file_name);
  while (input) {
    input >> key;
    KeyType index_key;
    index_key.SetFromInteger(key);
    Remove(index_key, transaction);
  }
}

/**
 * This method is used for debug only, You don't need to modify
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Draw(BufferPoolManager *bpm, const std::string &outf) {
  if (IsEmpty()) {
    LOG_WARN("Drawing empty tree");
    return;
  }
  std::ofstream out(outf);
  out << "digraph G {" << std::endl;
  ToGraph(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(root_page_id_)->GetData()), bpm, out);
  out << "}" << std::endl;
  out.flush();
  out.close();
}

/**
 * This method is used for debug only, You don't need to modify
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::Print(BufferPoolManager *bpm) {
  if (IsEmpty()) {
    LOG_WARN("Printing empty tree");
    return;
  }
  ToString(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(root_page_id_)->GetData()), bpm);
}

/**
 * This method is used for debug only, You don't need to modify
 * @tparam KeyType
 * @tparam ValueType
 * @tparam KeyComparator
 * @param page
 * @param bpm
 * @param out
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ToGraph(BPlusTreePage *page, BufferPoolManager *bpm, std::ofstream &out) const {
  std::string leaf_prefix("LEAF_");
  std::string internal_prefix("INT_");
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    // Print node name
    out << leaf_prefix << leaf->GetPageId();
    // Print node properties
    out << "[shape=plain color=green ";
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">P=" << leaf->GetPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << leaf->GetSize() << "\">"
        << "max_size=" << leaf->GetMaxSize() << ",min_size=" << leaf->GetMinSize() << ",size=" << leaf->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < leaf->GetSize(); i++) {
      out << "<TD>" << leaf->KeyAt(i) << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Leaf node link if there is a next page
    if (leaf->GetNextPageId() != INVALID_PAGE_ID) {
      out << leaf_prefix << leaf->GetPageId() << " -> " << leaf_prefix << leaf->GetNextPageId() << ";\n";
      out << "{rank=same " << leaf_prefix << leaf->GetPageId() << " " << leaf_prefix << leaf->GetNextPageId() << "};\n";
    }

    // Print parent links if there is a parent
    if (leaf->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << leaf->GetParentPageId() << ":p" << leaf->GetPageId() << " -> " << leaf_prefix
          << leaf->GetPageId() << ";\n";
    }
  } else {
    auto *internal = reinterpret_cast<InternalPage *>(page);
    // Print node name
    out << internal_prefix << internal->GetPageId();
    // Print node properties
    out << "[shape=plain color=pink ";  // why not?
    // Print data of the node
    out << "label=<<TABLE BORDER=\"0\" CELLBORDER=\"1\" CELLSPACING=\"0\" CELLPADDING=\"4\">\n";
    // Print data
    out << "<TR><TD COLSPAN=\"" << internal->GetSize() << "\">P=" << internal->GetPageId() << "</TD></TR>\n";
    out << "<TR><TD COLSPAN=\"" << internal->GetSize() << "\">"
        << "max_size=" << internal->GetMaxSize() << ",min_size=" << internal->GetMinSize() << ",size=" << internal->GetSize()
        << "</TD></TR>\n";
    out << "<TR>";
    for (int i = 0; i < internal->GetSize(); i++) {
      out << "<TD PORT=\"p" << internal->ValueAt(i) << "\">";
      if (i > 0) {
        out << internal->KeyAt(i);
      } else {
        out << " ";
      }
      out << "</TD>\n";
    }
    out << "</TR>";
    // Print table end
    out << "</TABLE>>];\n";
    // Print Parent link
    if (internal->GetParentPageId() != INVALID_PAGE_ID) {
      out << internal_prefix << internal->GetParentPageId() << ":p" << internal->GetPageId() << " -> " << internal_prefix
          << internal->GetPageId() << ";\n";
    }
    // Print leaves
    for (int i = 0; i < internal->GetSize(); i++) {
      auto child_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(internal->ValueAt(i))->GetData());
      ToGraph(child_page, bpm, out);
      if (i > 0) {
        auto sibling_page = reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(internal->ValueAt(i - 1))->GetData());
        if (!sibling_page->IsLeafPage() && !child_page->IsLeafPage()) {
          out << "{rank=same " << internal_prefix << sibling_page->GetPageId() << " " << internal_prefix
              << child_page->GetPageId() << "};\n";
        }
        bpm->UnpinPage(sibling_page->GetPageId(), false);
      }
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

/**
 * This function is used for debug only, You don't need to modify
 * @tparam KeyType
 * @tparam ValueType
 * @tparam KeyComparator
 * @param page
 * @param bpm
 */
INDEX_TEMPLATE_ARGUMENTS
void BPLUSTREE_TYPE::ToString(BPlusTreePage *page, BufferPoolManager *bpm) const {
  if (page->IsLeafPage()) {
    auto *leaf = reinterpret_cast<LeafPage *>(page);
    std::cout << "Leaf Page: " << leaf->GetPageId() << "\tparent: " << leaf->GetParentPageId()
              << "\tnext: " << leaf->GetNextPageId() << std::endl;
    for (int i = 0; i < leaf->GetSize(); i++) {
      std::cout << leaf->KeyAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
  } else {
    auto *internal = reinterpret_cast<InternalPage *>(page);
    std::cout << "Internal Page: " << internal->GetPageId() << "\tparent: " << internal->GetParentPageId() << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      std::cout << internal->KeyAt(i) << ": " << internal->ValueAt(i) << ",";
    }
    std::cout << std::endl;
    std::cout << std::endl;
    for (int i = 0; i < internal->GetSize(); i++) {
      ToString(reinterpret_cast<BPlusTreePage *>(bpm->FetchPage(internal->ValueAt(i))->GetData()), bpm);
    }
  }
  bpm->UnpinPage(page->GetPageId(), false);
}

template class BPlusTree<GenericKey<4>, RID, GenericComparator<4>>;
template class BPlusTree<GenericKey<8>, RID, GenericComparator<8>>;
template class BPlusTree<GenericKey<16>, RID, GenericComparator<16>>;
template class BPlusTree<GenericKey<32>, RID, GenericComparator<32>>;
template class BPlusTree<GenericKey<64>, RID, GenericComparator<64>>;

}  // namespace bustub