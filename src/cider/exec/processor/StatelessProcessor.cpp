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

#include "exec/processor/StatelessProcessor.h"

namespace cider::exec::processor {

void StatelessProcessor::getResult(struct ArrowArray& array, struct ArrowSchema& schema) {
  if (!input_arrow_array_) {
    if (no_more_batch_) {
      // set state as finish if last batch has been processed and no more batch
      state_ = BatchProcessorState::kFinished;
    }
    array.length = 0;
    return;
  }

  if (input_arrow_array_->release) {
    input_arrow_array_->release(const_cast<struct ArrowArray*>(input_arrow_array_));
  }
  input_arrow_array_ = nullptr;

  if (input_arrow_schema_ && input_arrow_schema_->release) {
    input_arrow_schema_->release(const_cast<struct ArrowSchema*>(input_arrow_schema_));
  }
  input_arrow_schema_ = nullptr;

  auto output_batch = runtime_context_->getOutputBatch();
  output_batch->move(schema, array);
  return;
}

}  // namespace cider::exec::processor
