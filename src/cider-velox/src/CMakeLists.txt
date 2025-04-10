# Copyright (c) 2022 Intel Corporation.
#
# Licensed to the Apache Software Foundation (ASF) under one
# or more contributor license agreements.  See the NOTICE file
# distributed with this work for additional information
# regarding copyright ownership.  The ASF licenses this file
# to you under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in compliance
# with the License.  You may obtain a copy of the License at
#
#   http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing,
# software distributed under the License is distributed on an
# "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
# KIND, either express or implied.  See the License for the
# specific language governing permissions and limitations
# under the License.

add_subdirectory(substrait)
add_subdirectory(planTransformer)
add_subdirectory(ciderTransformer)

set(VELOX_PLUGIN_SOURCES
    CiderPlanNode.cpp
    CiderOperator.cpp
    CiderJoinBuild.cpp
    DataConvertor.cpp
    RawDataConvertor.cpp
    ArrowDataConvertor.cpp
    ArrowConvertorUtils.cpp
    CiderVeloxPluginCtx.cpp
    CiderStatefulOperator.cpp
    CiderStatelessOperator.cpp
    CiderPipelineOperator.cpp
    CiderHashJoinBuild.cpp)

add_library(velox_plugin ${VELOX_PLUGIN_SOURCES})

target_precompile_headers(velox_plugin PUBLIC DataConvertor.h)

target_link_libraries(velox_plugin velox_substrait_convertor
                      velox_plan_transformer velox_exec ${GLOG})

# install headers
set(VELOX_PLUGIN_HEADERS
    CiderOperator.h CiderPlanNode.h CiderPlanNodeTranslator.h
    VeloxPlanToCiderExecutionUnit.h)
install(FILES ${VELOX_PLUGIN_HEADERS} DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")

# install so
install(TARGETS velox_plugin LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
