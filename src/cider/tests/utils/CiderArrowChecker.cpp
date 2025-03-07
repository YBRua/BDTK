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

#include "tests/utils/CiderArrowChecker.h"

#include <cstring>

#include "util/Logger.h"

namespace cider::test::util {

namespace {

template <typename T>
bool checkArrowBuffer(const struct ArrowArray* expect_array,
                      const struct ArrowArray* actual_array) {
  auto expect_value_buffer = reinterpret_cast<const T*>(expect_array->buffers[1]);
  auto actual_value_buffer = reinterpret_cast<const T*>(actual_array->buffers[1]);
  if (expect_value_buffer == nullptr && actual_value_buffer == nullptr) {
    return true;
  }
  if (expect_value_buffer == nullptr || actual_value_buffer == nullptr) {
    return false;
  }

  auto expect_null_buffer = reinterpret_cast<const uint8_t*>(expect_array->buffers[0]);
  auto actual_null_buffer = reinterpret_cast<const uint8_t*>(actual_array->buffers[0]);
  if (expect_null_buffer == nullptr && actual_null_buffer == nullptr) {
    return !memcmp(
        expect_value_buffer, actual_value_buffer, sizeof(T) * expect_array->length);
  } else if (expect_null_buffer && actual_null_buffer) {
    for (int64_t i = 0; i < expect_array->length; i++) {
      int expect_valid = expect_null_buffer[i / 8] & (1 << (i % 8));
      int actual_valid = actual_null_buffer[i / 8] & (1 << (i % 8));
      if (expect_valid != actual_valid) {
        LOG(INFO) << "ArrowArray null bit not equal: "
                  << "Expected: " << expect_valid << ". Actual: " << actual_valid;
        return false;
      }
      if (expect_valid) {
        if (expect_value_buffer[i] != actual_value_buffer[i]) {
          return false;
        }
      }
    }
  } else {
    LOG(INFO) << "One ArrowArray null buffer is null in checkArrowBuffer.";
    return false;
  }

  return true;
}

}  // namespace

bool checkOneScalarArrowEqual(const struct ArrowArray* expect_array,
                              const struct ArrowArray* actual_array,
                              const struct ArrowSchema* expect_schema,
                              const struct ArrowSchema* actual_schema) {
  if (!expect_schema || !actual_schema) {
    LOG(INFO) << "One or more Arrowschema are null in checkOneScalarArrowEqual.";
    return false;
  }

  if (expect_schema->n_children != 0 || actual_schema->n_children != 0) {
    LOG(INFO) << "checkOneScalarArrowEqual only support ArrowSchema without children";
    return false;
  }

  if (expect_schema->format != actual_schema->format) {
    LOG(INFO) << "ArrowSchema format not equal: "
              << "Expected: " << expect_schema->format
              << ". Actual: " << actual_schema->format;
    return false;
  }

  if (!expect_array || !actual_array) {
    LOG(INFO) << "One or more Arrowarray are null in checkOneScalarArrowEqual.";
    return false;
  }

  if (expect_array->n_children != 0 || actual_array->n_children != 0) {
    LOG(INFO) << "checkOneScalarArrowEqual only support ArrowArray without children";
    return false;
  }

  if (expect_array->null_count != actual_array->null_count) {
    LOG(INFO) << "ArrowArray null_count not equal: "
              << "Expected: " << expect_array->null_count
              << ". Actual: " << actual_array->null_count;
    return false;
  }

  if (expect_array->n_buffers != actual_array->n_buffers) {
    LOG(INFO) << "ArrowArray n_buffers not equal: "
              << "Expected: " << expect_array->n_buffers
              << ". Actual: " << actual_array->n_buffers;
    return false;
  }

  if (expect_array->length != actual_array->length) {
    LOG(INFO) << "ArrowArray length not equal: "
              << "Expected: " << expect_array->length
              << ". Actual: " << actual_array->length;
    return false;
  }

  switch (expect_schema->format[0]) {
    case 'b':
      return checkArrowBuffer<bool>(expect_array, actual_array);
    case 'c':
    case 'C':
      return checkArrowBuffer<int8_t>(expect_array, actual_array);
    case 's':
    case 'S':
      return checkArrowBuffer<int16_t>(expect_array, actual_array);
    case 'i':
    case 'I':
      return checkArrowBuffer<int32_t>(expect_array, actual_array);
    case 'l':
    case 'L':
      return checkArrowBuffer<int64_t>(expect_array, actual_array);
    case 'e':
    case 'f':
    case 'g':
    case 'z':
    case 'Z':
    case 'u':
    case 'U':
    case 'd':
    case 'w':
    case 't':
    default:
      LOG(ERROR) << "ArrowArray value buffer check not support for type: "
                 << expect_schema->format;
  }
  return false;
}

bool CiderArrowChecker::checkArrowEq(const struct ArrowArray* expect_array,
                                     const struct ArrowArray* actual_array,
                                     const struct ArrowSchema* expect_schema,
                                     const struct ArrowSchema* actual_schema) {
  if (!expect_array || !actual_array) {
    LOG(INFO) << "One or more Arrowarray are null_ptr in checkArrowEq. ";
    return false;
  }
  if (expect_array->n_children != actual_array->n_children) {
    LOG(INFO) << "ArrowArray n_children not equal: "
              << "Expected: " << expect_array->n_children
              << ". Actual: " << actual_array->n_children;
    return false;
  }
  if (expect_array->length != actual_array->length) {
    LOG(INFO) << "ArrowArray length not equal: "
              << "Expected: " << expect_array->length
              << ". Actual: " << actual_array->length;
    return false;
  }

  if (!expect_schema || !actual_schema) {
    LOG(INFO) << "One or more Arrowschema are null_ptr in checkArrowEq. ";
    return false;
  }
  if (expect_schema->n_children != actual_schema->n_children) {
    LOG(INFO) << "ArrowSchema n_children not equal: "
              << "Expected: " << expect_schema->n_children
              << ". Actual: " << actual_schema->n_children;
    return false;
  }
  if (strcmp(expect_schema->format, actual_schema->format)) {
    LOG(INFO) << "ArrowSchema format not equal: "
              << "Expected: " << expect_schema->format
              << ". Actual: " << actual_schema->format;
    return false;
  }

  if (expect_array->n_children == 0) {
    return checkOneScalarArrowEqual(
        expect_array, actual_array, expect_schema, expect_schema);
  }

  for (int64_t i = 0; i < expect_array->n_children; i++) {
    bool child_arrow_eq = checkOneScalarArrowEqual(expect_array->children[i],
                                                   actual_array->children[i],
                                                   expect_schema->children[i],
                                                   expect_schema->children[i]);
    if (!child_arrow_eq) {
      return false;
    }
  }

  return true;
}

}  // namespace cider::test::util
