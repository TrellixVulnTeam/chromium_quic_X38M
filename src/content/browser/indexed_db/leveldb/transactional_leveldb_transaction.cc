// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/indexed_db/leveldb/transactional_leveldb_transaction.h"

#include <utility>

#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "content/browser/indexed_db/indexed_db_tracing.h"
#include "content/browser/indexed_db/leveldb/leveldb_write_batch.h"
#include "content/browser/indexed_db/leveldb/transactional_leveldb_database.h"
#include "content/browser/indexed_db/leveldb/transactional_leveldb_iterator.h"
#include "content/browser/indexed_db/scopes/leveldb_scope.h"
#include "content/browser/indexed_db/scopes/leveldb_scopes.h"
#include "third_party/leveldatabase/env_chromium.h"
#include "third_party/leveldatabase/src/include/leveldb/db.h"
#include "third_party/leveldatabase/src/include/leveldb/iterator.h"

using base::StringPiece;

namespace content {

TransactionalLevelDBTransaction::TransactionalLevelDBTransaction(
    TransactionalLevelDBDatabase* db,
    std::unique_ptr<LevelDBScope> scope)
    : db_(db), scope_(std::move(scope)), weak_factory_(this) {
  DCHECK(db_);
  DCHECK(scope_);
}

TransactionalLevelDBTransaction::~TransactionalLevelDBTransaction() = default;

leveldb::Status TransactionalLevelDBTransaction::Put(const StringPiece& key,
                                                     std::string* value) {
  leveldb::Status s = scope_->Put(leveldb_env::MakeSlice(key), *value);
  EvictLoadedIterators();
  return s;
}

leveldb::Status TransactionalLevelDBTransaction::Remove(
    const StringPiece& key) {
  leveldb::Status s = scope_->Delete(leveldb_env::MakeSlice(key));
  EvictLoadedIterators();
  return s;
}

leveldb::Status TransactionalLevelDBTransaction::RemoveRange(
    const StringPiece& begin,
    const StringPiece& end,
    LevelDBScopeDeletionMode deletion_mode) {
  // The renderer-side code always issues range deletions even in the case of
  // single key, so handle that case here to avoid doing sub-optimal range
  // deleting.
  if (begin.compare(end) == 0 &&
      deletion_mode ==
          LevelDBScopeDeletionMode::kImmediateWithRangeEndInclusive) {
    return Remove(begin);
  }
  leveldb::Status s =
      scope_->DeleteRange(leveldb_env::MakeSlice(begin),
                          leveldb_env::MakeSlice(end), deletion_mode);
  EvictLoadedIterators();
  return s;
}

leveldb::Status TransactionalLevelDBTransaction::Get(const StringPiece& key,
                                                     std::string* value,
                                                     bool* found) {
  *found = false;
#if DCHECK_IS_ON()
  DCHECK(!finished_);
  const std::vector<uint8_t>& prefix = db_->scopes()->metadata_key_prefix();
  DCHECK(!key.starts_with(base::StringPiece(
      reinterpret_cast<const char*>(prefix.data()), prefix.size())));
#endif
  leveldb::Status s = scope_->WriteChangesAndUndoLog();
  if (!s.ok() && !s.IsNotFound())
    return s;
  return db_->Get(key, value, found);
}

leveldb::Status TransactionalLevelDBTransaction::Commit() {
  DCHECK(!finished_);
  IDB_TRACE("LevelDBTransaction::Commit");

  finished_ = true;
  return db_->scopes()->Commit(std::move(scope_),
                               std::move(commit_cleanup_complete_callback_));
}

void TransactionalLevelDBTransaction::RollbackAndMaybeTearDown() {
  DCHECK(!finished_);
  finished_ = true;
  // Resetting |scope_| can result in a revert. In single-sequence mode, the
  // scopes system will synchronously execute the revert. If the revert errors,
  // this can cause the IndexedDBOriginState to be deleted. Hence the method
  // name, RollbackAndMaybeTearDown().
  scope_.reset();
}

std::unique_ptr<TransactionalLevelDBIterator>
TransactionalLevelDBTransaction::CreateIterator() {
  leveldb::Status s = scope_->WriteChangesAndUndoLog();
  if (!s.ok() && !s.IsNotFound())
    return nullptr;
  std::unique_ptr<TransactionalLevelDBIterator> it = db_->CreateIterator(
      weak_factory_.GetWeakPtr(), db_->DefaultReadOptions());
  loaded_iterators_.insert(it.get());
  return it;
}

uint64_t TransactionalLevelDBTransaction::GetTransactionSize() const {
  return scope_->GetMemoryUsage() + scope_->GetApproximateBytesWritten();
}

leveldb::Status TransactionalLevelDBTransaction::ForceWriteChangesAndUndoLog() {
  return scope_->WriteChangesAndUndoLog();
}

void TransactionalLevelDBTransaction::OnIteratorLoaded(
    TransactionalLevelDBIterator* iterator) {
  DCHECK(evicted_iterators_.find(iterator) != evicted_iterators_.end());
  loaded_iterators_.insert(iterator);
  evicted_iterators_.erase(iterator);
}

void TransactionalLevelDBTransaction::OnIteratorEvicted(
    TransactionalLevelDBIterator* iterator) {
  DCHECK(loaded_iterators_.find(iterator) != loaded_iterators_.end() ||
         is_evicting_all_loaded_iterators_);
  loaded_iterators_.erase(iterator);
  evicted_iterators_.insert(iterator);
}

void TransactionalLevelDBTransaction::OnIteratorDestroyed(
    TransactionalLevelDBIterator* iterator) {
  DCHECK(evicted_iterators_.find(iterator) != evicted_iterators_.end() ||
         loaded_iterators_.find(iterator) != loaded_iterators_.end());
  evicted_iterators_.erase(iterator);
  loaded_iterators_.erase(iterator);
}

void TransactionalLevelDBTransaction::EvictLoadedIterators() {
  if (loaded_iterators_.empty())
    return;
  is_evicting_all_loaded_iterators_ = true;
  base::flat_set<TransactionalLevelDBIterator*> to_be_evicted =
      std::move(loaded_iterators_);
  for (TransactionalLevelDBIterator* iter : to_be_evicted) {
    iter->EvictLevelDBIterator();
  }
  is_evicting_all_loaded_iterators_ = false;
}

LevelDBDirectTransaction::LevelDBDirectTransaction(
    TransactionalLevelDBDatabase* db)
    : db_(db), write_batch_(LevelDBWriteBatch::Create()) {
  DCHECK(db_);
}

LevelDBDirectTransaction::~LevelDBDirectTransaction() = default;

leveldb::Status LevelDBDirectTransaction::Put(const StringPiece& key,
                                              const std::string* value) {
  DCHECK(!IsFinished());
  write_batch_->Put(key, *value);
  return leveldb::Status::OK();
}

leveldb::Status LevelDBDirectTransaction::Get(const StringPiece& key,
                                              std::string* value,
                                              bool* found) {
  *found = false;
#if DCHECK_IS_ON()
  DCHECK(!IsFinished());
  const std::vector<uint8_t>& prefix = db_->scopes()->metadata_key_prefix();
  DCHECK(!key.starts_with(base::StringPiece(
      reinterpret_cast<const char*>(prefix.data()), prefix.size())));
#endif
  leveldb::Status s = db_->Get(key, value, found);
  DCHECK(s.ok() || !*found);
  return s;
}

void LevelDBDirectTransaction::Remove(const StringPiece& key) {
  DCHECK(!IsFinished());
  write_batch_->Remove(key);
}

leveldb::Status LevelDBDirectTransaction::Commit() {
  DCHECK(!IsFinished());
  IDB_TRACE("LevelDBDirectTransaction::Commit");

  leveldb::Status s = db_->Write(write_batch_.get());
  if (s.ok())
    write_batch_.reset();
  return s;
}

}  // namespace content
