//===----------------------------------------------------------------------===//
//
//                         CMU-DB Project (15-445/645)
//                         ***DO NO SHARE PUBLICLY***
//
// Identification: src/include/index/b_plus_tree.h
//
// Copyright (c) 2018, Carnegie Mellon University Database Group
//
//===----------------------------------------------------------------------===//
#pragma once

#include <queue>
#include <string>
#include <vector>

#include "concurrency/transaction.h"
#include "storage/index/index_iterator.h"
#include "storage/page/b_plus_tree_internal_page.h"
#include "storage/page/b_plus_tree_leaf_page.h"

#include "common/rwlatch.h"

namespace bustub {

#define BPLUSTREE_TYPE BPlusTree<KeyType, ValueType, KeyComparator>

/**
 * @brief B+树支持的操作类型
 */
enum class Operation { SEARCH, INSERT, DELETE };

/**
 * Main class providing the API for the Interactive B+ Tree.
 *
 * Implementation of simple b+ tree data structure where internal pages direct
 * the search and leaf pages contain actual data.
 * (1) We only support unique key
 * (2) support insert & remove
 * (3) The structure should shrink and grow dynamically
 * (4) Implement index iterator for range scan
 */
INDEX_TEMPLATE_ARGUMENTS
class BPlusTree {
  using InternalPage = BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator>;
  using LeafPage = BPlusTreeLeafPage<KeyType, ValueType, KeyComparator>;

 public:
  explicit BPlusTree(std::string name, BufferPoolManager *buffer_pool_manager, const KeyComparator &comparator,
                     int leaf_max_size = LEAF_PAGE_SIZE, int internal_max_size = INTERNAL_PAGE_SIZE);

  // Returns true if this B+ tree has no keys and values.
  auto IsEmpty() const -> bool;

  // Insert a key-value pair into this B+ tree.
  auto Insert(const KeyType &key, const ValueType &value, Transaction *transaction = nullptr) -> bool;

  // Remove a key and its value from this B+ tree.
  void Remove(const KeyType &key, Transaction *transaction = nullptr);

  // return the value associated with a given key
  auto GetValue(const KeyType &key, std::vector<ValueType> *result, Transaction *transaction = nullptr) -> bool;

  // return the page id of the root node
  auto GetRootPageId() -> page_id_t;

  // index iterator
  auto Begin() -> INDEXITERATOR_TYPE;
  auto Begin(const KeyType &key) -> INDEXITERATOR_TYPE;
  auto End() -> INDEXITERATOR_TYPE;

  // print the B+ tree
  void Print(BufferPoolManager *bpm);

  // draw the B+ tree
  void Draw(BufferPoolManager *bpm, const std::string &outf);

  // read data from file and insert one by one
  void InsertFromFile(const std::string &file_name, Transaction *transaction = nullptr);

  // read data from file and remove one by one
  void RemoveFromFile(const std::string &file_name, Transaction *transaction = nullptr);

  /**
   * @brief 查找包含指定键的叶节点页面
   * @param key 查找的键
   * @param operation 操作类型
   * @param transaction 事务指针
   * @param leftMost 是否查找最左侧叶节点
   * @param rightMost 是否查找最右侧叶节点
   * @return 包含该键的叶节点页面
   */
  auto FindLeaf(const KeyType &key, Operation operation, Transaction *transaction = nullptr, bool leftMost = false,
                bool rightMost = false) -> Page *;
  
  /**
   * @brief 从队列中释放锁
   * @param transaction 事务指针
   */
  void ReleaseLatchFromQueue(Transaction *transaction);

 private:
  void UpdateRootPageId(int insert_record = 0);

  /* Debug Routines for FREE!! */
  void ToGraph(BPlusTreePage *page, BufferPoolManager *bpm, std::ofstream &out) const;

  void ToString(BPlusTreePage *page, BufferPoolManager *bpm) const;

  /**
   * @brief 创建新的B+树
   * @param key 键
   * @param value 值
   */
  void StartNewTree(const KeyType &key, const ValueType &value);

  /**
   * @brief 插入键值对到叶节点
   * @param key 键
   * @param value 值
   * @param transaction 事务指针
   * @return 插入成功返回true，键已存在返回false
   */
  auto InsertIntoLeaf(const KeyType &key, const ValueType &value, Transaction *transaction = nullptr) -> bool;

  /**
   * @brief 插入节点到父节点中
   * @param old_node 原节点
   * @param key 分裂键
   * @param new_node 新节点
   * @param transaction 事务指针
   */
  void InsertIntoParent(BPlusTreePage *old_node, const KeyType &key, BPlusTreePage *new_node,
                        Transaction *transaction = nullptr);

  /**
   * @brief 分裂节点
   * @param node 待分裂节点
   * @return 分裂出的新节点
   */
  template <typename N>
  auto Split(N *node) -> N *;

  /**
   * @brief 节点合并或重分配
   * @param node 待处理节点
   * @param transaction 事务指针
   * @return 是否需要删除节点
   */
  template <typename N>
  auto CoalesceOrRedistribute(N *node, Transaction *transaction = nullptr) -> bool;

  /**
   * @brief 合并相邻节点
   * @param neighbor_node 邻居节点
   * @param node 当前节点
   * @param parent 父节点
   * @param index 当前节点在父节点中的索引
   * @param transaction 事务指针
   * @return 是否需要删除父节点
   */
  template <typename N>
  auto Coalesce(N *neighbor_node, N *node, BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *parent, int index,
                Transaction *transaction = nullptr) -> bool;

  /**
   * @brief 重分配节点数据
   * @param neighbor_node 邻居节点
   * @param node 当前节点
   * @param parent 父节点
   * @param index 当前节点在父节点中的索引
   * @param from_prev 是否从前面的节点借数据
   */
  template <typename N>
  void Redistribute(N *neighbor_node, N *node, BPlusTreeInternalPage<KeyType, page_id_t, KeyComparator> *parent,
                    int index, bool from_prev);

  /**
   * @brief 调整根节点
   * @param node 旧的根节点
   * @return 是否需要删除旧根节点
   */
  auto AdjustRoot(BPlusTreePage *node) -> bool;
  
  // 成员变量
  std::string index_name_;                             // 索引名称
  page_id_t root_page_id_;                             // 根页面ID
  BufferPoolManager *buffer_pool_manager_;             // 缓冲池管理器
  KeyComparator comparator_;                           // 键比较器
  int leaf_max_size_;                                  // 叶节点最大大小
  int internal_max_size_;                              // 内部节点最大大小
  ReaderWriterLatch root_page_id_latch_;               // 根页面ID读写锁
};

}  // namespace bustub