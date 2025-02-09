/*
 * Copyright (c) 2022 Intel Corporation.
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

#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <algorithm>
#include <iostream>
#include <random>
#include <unordered_map>
#include <vector>
#include "exec/operator/join/CiderF14HashTable.h"
#include "exec/operator/join/CiderLinearProbingHashTable.h"
#include "exec/operator/join/CiderStdUnorderedHashTable.h"
#include "util/Logger.h"

// hash function for test collision
struct Hash {
  size_t operator()(int v) { return v % 7; }
};

// classic simple MurmurHash
struct MurmurHash {
  size_t operator()(int64_t rawHash) {
    rawHash ^= unsigned(rawHash) >> 33;
    rawHash *= 0xff51afd7ed558ccdL;
    rawHash ^= unsigned(rawHash) >> 33;
    rawHash *= 0xc4ceb9fe1a85ec53L;
    rawHash ^= unsigned(rawHash) >> 33;
    return rawHash;
  }
};

struct Equal {
  bool operator()(int lhs, int rhs) { return lhs == rhs; }
};

std::random_device rd;
std::mt19937 gen(rd());

int random(int low, int high) {
  std::uniform_int_distribution<> dist(low, high);
  return dist(gen);
}

TEST(CiderHashTableTest, mergeTest) {
  // Create a LinearProbeHashTable  with 16 buckets and 0 as the empty key
  cider_hashtable::LinearProbeHashTable<int, int, MurmurHash, Equal> hm1(16, NULL);
  cider_hashtable::LinearProbeHashTable<int, int, MurmurHash, Equal> hm2(16, NULL);
  cider_hashtable::LinearProbeHashTable<int, int, MurmurHash, Equal> hm3(16, NULL);
  for (int i = 0; i < 100; i++) {
    int value = random(-1000, 1000);
    hm1.insert(random(-10, 10), value);
    hm2.insert(random(-10, 10), value);
    hm3.insert(random(-10, 10), value);
  }
  std::vector<
      std::unique_ptr<cider_hashtable::LinearProbeHashTable<int, int, MurmurHash, Equal>>>
      other_tables;
  other_tables.push_back(
      std::make_unique<
          cider_hashtable::LinearProbeHashTable<int, int, MurmurHash, Equal>>(hm1));
  other_tables.push_back(
      std::make_unique<
          cider_hashtable::LinearProbeHashTable<int, int, MurmurHash, Equal>>(hm2));
  other_tables.push_back(
      std::make_unique<
          cider_hashtable::LinearProbeHashTable<int, int, MurmurHash, Equal>>(hm3));

  cider_hashtable::LinearProbeHashTable<int, int, MurmurHash, Equal> hm_final(16, NULL);
  hm_final.merge_other_hashtables(std::move(other_tables));
  EXPECT_EQ(hm_final.size(), 300);
}

// test value type for probe
TEST(CiderHashTableTest, pairAsValueTest) {
  // Create a LinearProbeHashTable  with 16 buckets and 0 as the empty key
  cider_hashtable::LinearProbeHashTable<int, std::pair<int*, int>, MurmurHash, Equal> hm(
      16, NULL);
  StdMapDuplicateKeyWrapper<int, std::pair<int*, int>> dup_map;
  for (int i = 0; i < 10000; i++) {
    int key = random(-1000, 1000);
    int value = random(-1000, 1000);
    // fake ptr
    int* fake_ptr = &key;
    hm.insert(key, std::make_pair(fake_ptr, value));
    dup_map.insert(std::move(key), std::move(std::make_pair(fake_ptr, value)));
  }
  for (auto key_iter : dup_map.getMap()) {
    auto dup_res_vec = dup_map.find(key_iter.first);
    auto hm_res_vec = hm.find(key_iter.first);
    std::sort(dup_res_vec.begin(), dup_res_vec.end());
    std::sort(hm_res_vec.begin(), hm_res_vec.end());
    EXPECT_TRUE(dup_res_vec == hm_res_vec);
  }
}

TEST(CiderHashTableTest, keyCollisionTest) {
  // Create a LinearProbeHashTable  with 16 buckets and 0 as the empty key
  cider_hashtable::LinearProbeHashTable<int, int, Hash, Equal> hm(16, NULL);
  StdMapDuplicateKeyWrapper<int, int> udup_map;
  hm.insert(1, 1);
  hm.insert(1, 2);
  hm.insert(15, 1515);
  hm.insert(8, 88);
  hm.insert(1, 5);
  hm.insert(1, 6);
  udup_map.insert(1, 1);
  udup_map.insert(1, 2);
  udup_map.insert(15, 1515);
  udup_map.insert(8, 88);
  udup_map.insert(1, 5);
  udup_map.insert(1, 6);

  for (auto key_iter : udup_map.getMap()) {
    auto dup_res_vec = udup_map.find(key_iter.first);
    auto hm_res_vec = hm.find(key_iter.first);
    std::sort(dup_res_vec.begin(), dup_res_vec.end());
    std::sort(hm_res_vec.begin(), hm_res_vec.end());
    EXPECT_TRUE(dup_res_vec == hm_res_vec);
  }
}

TEST(CiderHashTableTest, randomInsertAndfindTest) {
  // Create a LinearProbeHashTable  with 16 buckets and 0 as the empty key
  cider_hashtable::LinearProbeHashTable<int, int, MurmurHash, Equal> hm(16, NULL);
  StdMapDuplicateKeyWrapper<int, int> dup_map;
  for (int i = 0; i < 10000; i++) {
    int key = random(-1000, 1000);
    int value = random(-1000, 1000);
    hm.insert(key, value);
    dup_map.insert(std::move(key), std::move(value));
  }
  for (auto key_iter : dup_map.getMap()) {
    auto dup_res_vec = dup_map.find(key_iter.first);
    auto hm_res_vec = hm.find(key_iter.first);
    std::sort(dup_res_vec.begin(), dup_res_vec.end());
    std::sort(hm_res_vec.begin(), hm_res_vec.end());
    EXPECT_TRUE(dup_res_vec == hm_res_vec);
  }
}

TEST(CiderHashTableTest, LPHashMapTest) {
  // Create a LinearProbeHashTable  with 16 buckets and 0 as the empty key
  cider_hashtable::LinearProbeHashTable<int, int, MurmurHash, Equal> hm(1024, NULL);
  for (int i = 0; i < 10000; i++) {
    int key = random(-100000, 10000);
    int value = random(-10000, 10000);
    hm.insert(key, value);
  }
  for (int i = 0; i < 1000000; i++) {
    int key = random(-10000, 10000);
    auto hm_res_vec = hm.find(key);
  }
}

TEST(CiderHashTableTest, dupMapTest) {
  StdMapDuplicateKeyWrapper<int, int> dup_map;
  for (int i = 0; i < 100000; i++) {
    int key = random(-10000, 10000);
    int value = random(-10000, 10000);
    dup_map.insert(std::move(key), std::move(value));
  }
  for (int i = 0; i < 1000000; i++) {
    int key = random(-10000, 10000);
    auto dup_res_vec = dup_map.find(key);
  }
}

TEST(CiderHashTableTest, f14MapTest) {
  F14MapDuplicateKeyWrapper<int, int> f14_map;
  for (int i = 0; i < 100000; i++) {
    int key = random(-10000, 10000);
    int value = random(-10000, 10000);
    f14_map.insert(std::move(key), std::move(value));
  }
  for (int i = 0; i < 1000000; i++) {
    int key = random(-10000, 10000);
    auto f14_res_vec = f14_map.find(key);
  }
}

int main(int argc, char* argv[]) {
  testing::InitGoogleTest(&argc, argv);
  int err{0};
  logger::LogOptions log_options(argv[0]);
  log_options.severity_ = logger::Severity::INFO;
  log_options.set_options();  // update default values
  logger::init(log_options);
  gflags::ParseCommandLineFlags(&argc, &argv, true);

  try {
    err = RUN_ALL_TESTS();
  } catch (const std::exception& e) {
  }
  return err;
}
