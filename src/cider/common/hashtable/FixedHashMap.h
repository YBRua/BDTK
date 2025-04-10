/*
 * Copyright (c) 2022 Intel Corporation.
 * Copyright (c) 2016-2022 ClickHouse, Inc.
 *
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#pragma once

#include <common/hashtable/FixedHashTable.h>
#include <common/hashtable/HashMap.h>

template <typename Key, typename TMapped, typename TState = HashTableNoState>
struct FixedHashMapCell {
  using Mapped = TMapped;
  using State = TState;

  using value_type = PairNoInit<Key, Mapped>;
  using mapped_type = TMapped;

  bool full;
  Mapped mapped;

  FixedHashMapCell() {}  //-V730 /// NOLINT
  FixedHashMapCell(const Key&, const State&) : full(true) {}
  FixedHashMapCell(const value_type& value_, const State&)
      : full(true), mapped(value_.second) {}

  const VoidKey getKey() const { return {}; }  /// NOLINT
  Mapped& getMapped() { return mapped; }
  const Mapped& getMapped() const { return mapped; }

  bool isZero(const State&) const { return !full; }
  void setZero() { full = false; }

  /// Similar to FixedHashSetCell except that we need to contain a pointer to the Mapped
  /// field.
  ///  Note that we have to assemble a continuous layout for the value_type on each call
  ///  of getValue().
  struct CellExt {
    CellExt() {}  //-V730 /// NOLINT
    CellExt(Key&& key_, const FixedHashMapCell* ptr_)
        : key(key_), ptr(const_cast<FixedHashMapCell*>(ptr_)) {}
    void update(Key&& key_, const FixedHashMapCell* ptr_) {
      key = key_;
      ptr = const_cast<FixedHashMapCell*>(ptr_);
    }
    Key key;
    FixedHashMapCell* ptr;

    const Key& getKey() const { return key; }
    Mapped& getMapped() { return ptr->mapped; }
    const Mapped& getMapped() const { return ptr->mapped; }
    const value_type getValue() const { return {key, ptr->mapped}; }  /// NOLINT
  };
};

// In case when we can encode empty cells with zero mapped values.
template <typename Key, typename TMapped, typename TState = HashTableNoState>
struct FixedHashMapImplicitZeroCell {
  using Mapped = TMapped;
  using State = TState;

  using value_type = PairNoInit<Key, Mapped>;
  using mapped_type = TMapped;

  Mapped mapped;

  FixedHashMapImplicitZeroCell() {}  /// NOLINT
  FixedHashMapImplicitZeroCell(const Key&, const State&) {}
  FixedHashMapImplicitZeroCell(const value_type& value_, const State&)
      : mapped(value_.second) {}

  const VoidKey getKey() const { return {}; }  /// NOLINT
  Mapped& getMapped() { return mapped; }
  const Mapped& getMapped() const { return mapped; }

  bool isZero(const State&) const { return !mapped; }
  void setZero() { mapped = {}; }

  // Similar to FixedHashSetCell except that we need to contain a pointer to the Mapped
  // field.
  // Note that we have to assemble a continuous layout for the value_type on each call
  // of getValue().
  struct CellExt {
    CellExt() {}  //-V730 /// NOLINT
    CellExt(Key&& key_, const FixedHashMapImplicitZeroCell* ptr_)
        : key(key_), ptr(const_cast<FixedHashMapImplicitZeroCell*>(ptr_)) {}
    void update(Key&& key_, const FixedHashMapImplicitZeroCell* ptr_) {
      key = key_;
      ptr = const_cast<FixedHashMapImplicitZeroCell*>(ptr_);
    }
    Key key;
    FixedHashMapImplicitZeroCell* ptr;

    const Key& getKey() const { return key; }
    Mapped& getMapped() { return ptr->mapped; }
    const Mapped& getMapped() const { return ptr->mapped; }
    const value_type getValue() const { return {key, ptr->mapped}; }  /// NOLINT
  };
};

template <typename Key,
          typename Mapped,
          typename Cell = FixedHashMapCell<Key, Mapped>,
          typename Size = FixedHashTableStoredSize<Cell>,
          typename Allocator = HashTableAllocator>
class FixedHashMap : public FixedHashTable<Key, Cell, Size, Allocator> {
 public:
  using Base = FixedHashTable<Key, Cell, Size, Allocator>;
  using Self = FixedHashMap;
  using LookupResult = typename Base::LookupResult;

  using Base::Base;

  template <typename Func, bool>
  void ALWAYS_INLINE mergeToViaEmplace(Self& that, Func&& func) {
    for (auto it = this->begin(), end = this->end(); it != end; ++it) {
      typename Self::LookupResult res_it;
      bool inserted;
      that.emplace(it->getKey(), res_it, inserted, it.getHash());
      func(res_it->getMapped(), it->getMapped(), inserted);
    }
  }

  template <typename Func>
  void ALWAYS_INLINE mergeToViaFind(Self& that, Func&& func) {
    for (auto it = this->begin(), end = this->end(); it != end; ++it) {
      auto res_it = that.find(it->getKey(), it.getHash());
      if (!res_it)
        func(it->getMapped(), it->getMapped(), false);
      else
        func(res_it->getMapped(), it->getMapped(), true);
    }
  }

  template <typename Func>
  void forEachValue(Func&& func) {
    for (auto& v : *this)
      func(v.getKey(), v.getMapped());
  }

  template <typename Func>
  void forEachMapped(Func&& func) {
    for (auto& v : *this)
      func(v.getMapped());
  }

  Mapped& ALWAYS_INLINE operator[](const Key& x) {
    LookupResult it;
    bool inserted;
    this->emplace(x, it, inserted);
    if (inserted)
      new (&it->getMapped()) Mapped();

    return it->getMapped();
  }
};

// It is mainly used for 1-bytes key, like int8 etc.
template <typename Key, typename Mapped, typename Allocator = HashTableAllocator>
using FixedImplicitZeroHashMap =
    FixedHashMap<Key,
                 Mapped,
                 FixedHashMapImplicitZeroCell<Key, Mapped>,
                 FixedHashTableStoredSize<FixedHashMapImplicitZeroCell<Key, Mapped>>,
                 Allocator>;

// It is mainly used for 2-bytes key, like int16 etc.
template <typename Key, typename Mapped, typename Allocator = HashTableAllocator>
using FixedImplicitZeroHashMapWithCalculatedSize =
    FixedHashMap<Key,
                 Mapped,
                 FixedHashMapImplicitZeroCell<Key, Mapped>,
                 FixedHashTableCalculatedSize<FixedHashMapImplicitZeroCell<Key, Mapped>>,
                 Allocator>;
