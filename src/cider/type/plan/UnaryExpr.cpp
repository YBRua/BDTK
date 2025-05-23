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
#include "UnaryExpr.h"
#include "exec/template/Execute.h"

namespace Analyzer {
using namespace cider::jitlib;

JITValuePointer codegenCastBetweenDateAndTime(CodegenContext& context,
                                              JITValue& operand_val,
                                              const SQLTypeInfo& operand_ti,
                                              const SQLTypeInfo& ti) {
  JITFunction& func = *context.getJITFunction();
  const int64_t operand_width = getTypeBytes(operand_ti.get_type());
  const int64_t target_width = getTypeBytes(ti.get_type());
  int64_t dim_scaled = DateTimeUtils::get_timestamp_precision_scale(
      abs(operand_ti.get_dimension() - ti.get_dimension()));
  JITValuePointer cast_scaled =
      func.createLiteral(JITTypeTag::INT64, dim_scaled * kSecondsInOneDay);
  if (target_width == operand_width) {
    return JITValuePointer(&operand_val);
  } else if (target_width > operand_width) {
    JITValuePointer cast_val =
        operand_val.castJITValuePrimitiveType(getJITTypeTag(ti.get_type()));
    return JITValuePointer(cast_val * cast_scaled);
  } else {
    JITValuePointer trunc_val = func.emitRuntimeFunctionCall(
        "floor_div_lhs",
        JITFunctionEmitDescriptor{.ret_type = JITTypeTag::INT64,
                                  .params_vector = {&operand_val, cast_scaled.get()}});
    return trunc_val->castJITValuePrimitiveType(getJITTypeTag(ti.get_type()));
  }
}

JITValuePointer codegenCastBetweenTime(CodegenContext& context,
                                       JITValue& operand_val,
                                       const SQLTypeInfo& operand_ti,
                                       const SQLTypeInfo& target_ti) {
  JITFunction& func = *context.getJITFunction();
  const auto operand_dimen = operand_ti.get_dimension();
  const auto target_dimen = target_ti.get_dimension();
  JITValuePointer cast_scaled = func.createLiteral(
      JITTypeTag::INT64,
      DateTimeUtils::get_timestamp_precision_scale(abs(operand_dimen - target_dimen)));
  if (operand_dimen == target_dimen) {
    return JITValuePointer(&operand_val);
  } else if (operand_dimen < target_dimen) {
    return JITValuePointer(operand_val * cast_scaled);
  } else {
    return func.emitRuntimeFunctionCall(
        "floor_div_lhs",
        JITFunctionEmitDescriptor{.ret_type = JITTypeTag::INT64,
                                  .params_vector = {&operand_val, cast_scaled.get()}});
  }
}

JITExprValue& UOper::codegen(CodegenContext& context) {
  if (auto& expr_var = get_expr_value()) {
    return expr_var;
  }
  auto operand = const_cast<Analyzer::Expr*>(get_operand());
  if (is_unnest(operand) || is_unnest(operand)) {
    CIDER_THROW(CiderCompileException, "Unnest not supported in UOper");
  }
  const auto& operand_ti = operand->get_type_info();
  if (operand_ti.is_decimal()) {
    CIDER_THROW(CiderCompileException, "Decimal not supported in Uoper codegen now.");
  }

  switch (get_optype()) {
    case kISNULL:
    case kISNOTNULL: {
      return codegenIsNull(context, operand, get_optype());
    }
    case kNOT: {
      return codegenNot(context, operand);
    }
    case kCAST: {
      return codegenCast(context, operand);
    }
    case kUMINUS: {
      return codegenUminus(context, operand);
    }
    default:
      UNIMPLEMENTED();
  }
}

JITExprValue& UOper::codegenIsNull(CodegenContext& context,
                                   Analyzer::Expr* operand,
                                   SQLOps optype) {
  JITFunction& func = *context.getJITFunction();
  // For ISNULL, the null will always be false
  const auto& ti = get_type_info();
  const auto type = ti.is_decimal() ? decimal_to_int_type(ti) : ti.get_type();

  // for IS NULL / IS NOT NULL, we only need the null info (getNull())
  JITExprValueAdaptor operand_val(operand->codegen(context));

  if (optype == kISNOTNULL) {
    // is not null
    auto null_value = operand_val.getNull()->notOp();
    return set_expr_value(func.createLiteral(getJITTag(type), false), null_value);
  } else {
    // is null
    auto null_value = operand_val.getNull();
    return set_expr_value(func.createLiteral(getJITTag(type), false), null_value);
  }
}

JITExprValue& UOper::codegenNot(CodegenContext& context, Analyzer::Expr* operand) {
  const auto& ti = get_type_info();

  // should be bool, or otherwise will throw in notOp()
  FixSizeJITExprValue operand_val(operand->codegen(context));

  CHECK(operand_val.getNull().get());
  return set_expr_value(operand_val.getNull(), operand_val.getValue()->notOp());
}

JITExprValue& UOper::codegenUminus(CodegenContext& context, Analyzer::Expr* operand) {
  // should be fixedSize type
  FixSizeJITExprValue operand_val(operand->codegen(context));
  CHECK(operand_val.getNull().get());
  return set_expr_value(operand_val.getNull(), -operand_val.getValue());
}

JITExprValue& UOper::codegenCast(CodegenContext& context, Analyzer::Expr* operand) {
  const auto& ret_ti = get_type_info();
  const auto& operand_ti = operand->get_type_info();
  if (operand_ti.is_string()) {
    // input is varchar
    VarSizeJITExprValue operand_val(operand->codegen(context));
    if (ret_ti.is_string()) {
      // only supports casting from varchar to varchar
      return set_expr_value(
          operand_val.getNull(), operand_val.getLength(), operand_val.getValue());
    } else {
      CIDER_THROW(CiderUnsupportedException,
                  fmt::format("casting from type:{} to type:{} is not supported",
                              operand_ti.get_type_name(),
                              ret_ti.get_type_name()));
    }
  } else {
    // input is primitive type
    FixSizeJITExprValue operand_val(operand->codegen(context));
    return set_expr_value(operand_val.getNull(),
                          codegenCastFunc(context, operand_val.getValue()));
  }
}

JITValuePointer UOper::codegenCastFunc(CodegenContext& context, JITValue& operand_val) {
  JITFunction& func = *context.getJITFunction();
  const SQLTypeInfo& ti = get_type_info();
  const SQLTypeInfo& operand_ti = get_operand()->get_type_info();
  JITTypeTag ti_jit_tag = getJITTypeTag(ti.get_type());
  if (operand_ti.is_string() || ti.is_string()) {
    UNIMPLEMENTED();
  } else if (operand_ti.get_type() == kDATE || ti.get_type() == kDATE) {
    return codegenCastBetweenDateAndTime(context, operand_val, operand_ti, ti);
  } else if (operand_ti.get_type() == kTIMESTAMP && ti.get_type() == kTIMESTAMP) {
    return codegenCastBetweenTime(context, operand_val, operand_ti, ti);
  } else if (operand_ti.is_integer()) {
    if (ti.is_fp() || ti.is_integer()) {
      return operand_val.castJITValuePrimitiveType(ti_jit_tag);
    }
  } else if (operand_ti.is_fp()) {
    // Round by adding/subtracting 0.5 before fptosi.
    if (ti.is_integer()) {
      func.createIfBuilder()
          ->condition([&]() { return operand_val < 0; })
          ->ifTrue([&]() { operand_val = operand_val - 0.5; })
          ->ifFalse([&]() { operand_val = operand_val + 0.5; })
          ->build();
    }
    return operand_val.castJITValuePrimitiveType(ti_jit_tag);
  }
  CIDER_THROW(CiderCompileException,
              fmt::format("cast type:{} into type:{} not support yet",
                          operand_ti.get_type_name(),
                          ti.get_type_name()));
}

std::shared_ptr<Analyzer::Expr> UOper::deep_copy() const {
  return makeExpr<UOper>(type_info, contains_agg, optype, operand->deep_copy());
}

std::shared_ptr<Analyzer::Expr> UOper::add_cast(const SQLTypeInfo& new_type_info) {
  if (optype != kCAST) {
    return Expr::add_cast(new_type_info);
  }
  if (type_info.is_string() && new_type_info.is_string() &&
      new_type_info.get_compression() == kENCODING_DICT &&
      type_info.get_compression() == kENCODING_NONE) {
    const SQLTypeInfo oti = operand->get_type_info();
    if (oti.is_string() && oti.get_compression() == kENCODING_DICT &&
        (oti.get_comp_param() == new_type_info.get_comp_param() ||
         oti.get_comp_param() == TRANSIENT_DICT(new_type_info.get_comp_param()))) {
      auto result = operand;
      operand = nullptr;
      return result;
    }
  }
  return Expr::add_cast(new_type_info);
}

void UOper::check_group_by(
    const std::list<std::shared_ptr<Analyzer::Expr>>& groupby) const {
  operand->check_group_by(groupby);
}

void UOper::group_predicates(std::list<const Expr*>& scan_predicates,
                             std::list<const Expr*>& join_predicates,
                             std::list<const Expr*>& const_predicates) const {
  std::set<int> rte_idx_set;
  operand->collect_rte_idx(rte_idx_set);
  if (rte_idx_set.size() > 1) {
    join_predicates.push_back(this);
  } else if (rte_idx_set.size() == 1) {
    scan_predicates.push_back(this);
  } else {
    const_predicates.push_back(this);
  }
}

bool UOper::operator==(const Expr& rhs) const {
  if (typeid(rhs) != typeid(UOper)) {
    return false;
  }
  const UOper& rhs_uo = dynamic_cast<const UOper&>(rhs);
  return optype == rhs_uo.get_optype() && *operand == *rhs_uo.get_operand();
}

std::string UOper::toString() const {
  std::string op;
  switch (optype) {
    case kNOT:
      op = "NOT ";
      break;
    case kUMINUS:
      op = "- ";
      break;
    case kISNULL:
      op = "IS NULL ";
      break;
    case kEXISTS:
      op = "EXISTS ";
      break;
    case kCAST:
      op = "CAST " + type_info.get_type_name() + "(" +
           std::to_string(type_info.get_precision()) + "," +
           std::to_string(type_info.get_scale()) + ") " +
           type_info.get_compression_name() + "(" +
           std::to_string(type_info.get_comp_param()) + ") ";
      break;
    case kUNNEST:
      op = "UNNEST ";
      break;
    default:
      break;
  }
  return "(" + op + operand->toString() + ") ";
}

void UOper::find_expr(bool (*f)(const Expr*), std::list<const Expr*>& expr_list) const {
  if (f(this)) {
    add_unique(expr_list);
    return;
  }
  operand->find_expr(f, expr_list);
}

}  // namespace Analyzer
