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

#include "exec/nextgen/context/CodegenContext.h"
#include "exec/nextgen/context/RuntimeContext.h"

namespace cider::exec::nextgen::context {
using namespace cider::jitlib;

JITValuePointer CodegenContext::registerBatch(const SQLTypeInfo& type,
                                              const std::string& name,
                                              bool arrow_array_output) {
  int64_t id = acquireContextID();
  JITValuePointer ret = jit_func_->createLocalJITValue([this, id, arrow_array_output]() {
    auto index = this->jit_func_->createLiteral(JITTypeTag::INT64, id);
    auto pointer = this->jit_func_->emitRuntimeFunctionCall(
        "get_query_context_item_ptr",
        JITFunctionEmitDescriptor{
            .ret_type = JITTypeTag::POINTER,
            .ret_sub_type = JITTypeTag::INT8,
            .params_vector = {this->jit_func_->getArgument(0).get(), index.get()}});
    if (arrow_array_output) {
      return this->jit_func_->emitRuntimeFunctionCall(
          "get_arrow_array_ptr",
          JITFunctionEmitDescriptor{.ret_type = JITTypeTag::POINTER,
                                    .ret_sub_type = JITTypeTag::INT8,
                                    .params_vector = {pointer.get()}});
    }
    return pointer;
  });
  ret->setName(name);

  batch_descriptors_.emplace_back(std::make_shared<BatchDescriptor>(id, name, type), ret);

  return ret;
}

JITValuePointer CodegenContext::getBufferContentPtr(
    int64_t id,
    bool output_raw_buffer,
    const std::string& raw_buffer_func_name) {
  JITValuePointer ret = jit_func_->createLocalJITValue(
      [this, id, output_raw_buffer, raw_buffer_func_name]() {
        auto index = this->jit_func_->createLiteral(JITTypeTag::INT64, id);
        auto pointer = this->jit_func_->emitRuntimeFunctionCall(
            "get_query_context_item_ptr",
            JITFunctionEmitDescriptor{
                .ret_type = JITTypeTag::POINTER,
                .ret_sub_type = JITTypeTag::INT8,
                .params_vector = {this->jit_func_->getArgument(0).get(), index.get()}});
        if (output_raw_buffer) {
          return this->jit_func_->emitRuntimeFunctionCall(
              raw_buffer_func_name,
              JITFunctionEmitDescriptor{.ret_type = JITTypeTag::POINTER,
                                        .ret_sub_type = JITTypeTag::INT8,
                                        .params_vector = {pointer.get()}});
        }
        return pointer;
      });
  return ret;
}

JITValuePointer CodegenContext::registerBuffer(const int32_t capacity,
                                               const std::string& name,
                                               const BufferInitializer& initializer,
                                               bool output_raw_buffer) {
  int64_t id = acquireContextID();
  JITValuePointer ret =
      getBufferContentPtr(id, output_raw_buffer, "get_under_level_buffer_ptr");
  ret->setName(name);

  buffer_descriptors_.emplace_back(
      std::make_shared<BufferDescriptor>(id, name, capacity, initializer), ret);

  return ret;
}

JITValuePointer CodegenContext::registerBuffer(const int32_t capacity,
                                               const std::vector<AggExprsInfo>& info,
                                               const std::string& name,
                                               const BufferInitializer& initializer,
                                               bool output_raw_buffer) {
  int64_t id = acquireContextID();
  JITValuePointer ret =
      getBufferContentPtr(id, output_raw_buffer, "get_under_level_buffer_ptr");
  ret->setName(name);

  buffer_descriptors_.emplace_back(
      std::make_shared<AggBufferDescriptor>(id, name, capacity, initializer, info), ret);
  return ret;
}

jitlib::JITValuePointer CodegenContext::registerCiderSet(const std::string& name,
                                                         const SQLTypeInfo& type,
                                                         CiderSetPtr c_set) {
  int64_t id = acquireContextID();
  auto index = this->jit_func_->createLiteral(JITTypeTag::INT64, id);
  JITValuePointer ret = jit_func_->createLocalJITValue([this, id]() {
    auto index = this->jit_func_->createLiteral(JITTypeTag::INT64, id);
    auto pointer = this->jit_func_->emitRuntimeFunctionCall(
        "get_query_context_item_ptr",
        JITFunctionEmitDescriptor{
            .ret_type = JITTypeTag::POINTER,
            .ret_sub_type = JITTypeTag::INT8,
            .params_vector = {this->jit_func_->getArgument(0).get(), index.get()}});

    return pointer;
  });
  ret->setName(name);

  cider_set_descriptors_.emplace_back(
      std::make_shared<CiderSetDescriptor>(id, name, type, std::move(c_set)), ret);
  return ret;
}

JITValuePointer CodegenContext::registerHashTable(const std::string& name) {
  int64_t id = acquireContextID();
  auto index = this->jit_func_->createLiteral(JITTypeTag::INT64, id);
  JITValuePointer ret = jit_func_->createLocalJITValue([this, id]() {
    auto index = this->jit_func_->createLiteral(JITTypeTag::INT64, id);
    auto pointer = this->jit_func_->emitRuntimeFunctionCall(
        "get_query_context_item_ptr",
        JITFunctionEmitDescriptor{
            .ret_type = JITTypeTag::POINTER,
            .ret_sub_type = JITTypeTag::INT8,
            .params_vector = {this->jit_func_->getArgument(0).get(), index.get()}});

    return pointer;
  });
  ret->setName(name);

  hashtable_descriptor_.first = std::make_shared<HashTableDescriptor>(id, name);
  hashtable_descriptor_.second.replace(ret);
  return ret;
}

RuntimeCtxPtr CodegenContext::generateRuntimeCTX(
    const CiderAllocatorPtr& allocator) const {
  auto runtime_ctx = std::make_unique<RuntimeContext>(getNextContextID());

  for (auto& batch_desc : batch_descriptors_) {
    runtime_ctx->addBatch(batch_desc.first);
  }

  for (auto& buffer_desc : buffer_descriptors_) {
    runtime_ctx->addBuffer(buffer_desc.first);
  }

  runtime_ctx->addHashTable(hashtable_descriptor_.first);
  for (auto& cider_set_desc : cider_set_descriptors_) {
    runtime_ctx->addCiderSet(cider_set_desc.first);
  }

  runtime_ctx->instantiate(allocator);
  return runtime_ctx;
}

std::string AggExprsInfo::getAggName(SQLAgg agg_type, SQLTypes sql_type) {
  std::string agg_name = "nextgen_cider_agg";
  switch (agg_type) {
    case SQLAgg::kSUM: {
      agg_name = agg_name + "_sum_" + utils::getSQLTypeName(sql_type);
      break;
    }
    default:
      LOG(FATAL) << "unsupport agg function type: " << toString(agg_type);
      break;
  }
  return agg_name;
}

namespace codegen_utils {
jitlib::JITValuePointer getArrowArrayLength(jitlib::JITValuePointer& arrow_array) {
  CHECK(arrow_array->getValueTypeTag() == JITTypeTag::POINTER);
  CHECK(arrow_array->getValueSubTypeTag() == JITTypeTag::INT8);

  auto& func = arrow_array->getParentJITFunction();
  auto ret = func.emitRuntimeFunctionCall(
      "extract_arrow_array_len",
      JITFunctionEmitDescriptor{.ret_type = JITTypeTag::INT64,
                                .params_vector = {arrow_array.get()}});
  ret->setName("array_len");
  return ret;
}

jitlib::JITValuePointer getArrowArrayBuffer(jitlib::JITValuePointer& arrow_array,
                                            int64_t index) {
  CHECK(arrow_array->getValueTypeTag() == JITTypeTag::POINTER);
  CHECK(arrow_array->getValueSubTypeTag() == JITTypeTag::INT8);

  auto& func = arrow_array->getParentJITFunction();
  auto jit_index = func.createLiteral(JITTypeTag::INT64, index);
  auto ret = func.emitRuntimeFunctionCall(
      "extract_arrow_array_buffer",
      JITFunctionEmitDescriptor{.ret_type = JITTypeTag::POINTER,
                                .ret_sub_type = JITTypeTag::INT8,
                                .params_vector = {arrow_array.get(), jit_index.get()}});
  ret->setName("array_buffer");
  return ret;
}

jitlib::JITValuePointer getArrowArrayChild(jitlib::JITValuePointer& arrow_array,
                                           int64_t index) {
  CHECK(arrow_array->getValueTypeTag() == JITTypeTag::POINTER);
  CHECK(arrow_array->getValueSubTypeTag() == JITTypeTag::INT8);

  auto& func = arrow_array->getParentJITFunction();
  auto jit_index = func.createLiteral(JITTypeTag::INT64, index);
  auto ret = func.emitRuntimeFunctionCall(
      "extract_arrow_array_child",
      JITFunctionEmitDescriptor{.ret_type = JITTypeTag::POINTER,
                                .ret_sub_type = JITTypeTag::INT8,
                                .params_vector = {arrow_array.get(), jit_index.get()}});

  ret->setName("child_array");
  return ret;
}

jitlib::JITValuePointer allocateArrowArrayBuffer(jitlib::JITValuePointer& arrow_array,
                                                 int64_t index,
                                                 jitlib::JITValuePointer& bytes) {
  CHECK(arrow_array->getValueTypeTag() == JITTypeTag::POINTER);
  CHECK(arrow_array->getValueSubTypeTag() == JITTypeTag::INT8);

  auto& func = arrow_array->getParentJITFunction();
  auto jit_index = func.createLiteral(JITTypeTag::INT64, index);
  func.emitRuntimeFunctionCall(
      "allocate_arrow_array_buffer",
      JITFunctionEmitDescriptor{
          .ret_type = JITTypeTag::VOID,
          .params_vector = {arrow_array.get(), jit_index.get(), bytes.get()}});

  return getArrowArrayBuffer(arrow_array, index);
}

void setArrowArrayLength(jitlib::JITValuePointer& arrow_array,
                         jitlib::JITValuePointer& len) {
  CHECK(arrow_array->getValueTypeTag() == JITTypeTag::POINTER);
  CHECK(arrow_array->getValueSubTypeTag() == JITTypeTag::INT8);

  auto& func = arrow_array->getParentJITFunction();
  func.emitRuntimeFunctionCall(
      "set_arrow_array_len",
      JITFunctionEmitDescriptor{.ret_type = JITTypeTag::VOID,
                                .params_vector = {arrow_array.get(), len.get()}});
}
}  // namespace codegen_utils
}  // namespace cider::exec::nextgen::context
