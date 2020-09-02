// Copyright 2020 The Tint Authors.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <memory>

#include "src/ast/binary_expression.h"
#include "src/ast/identifier_expression.h"
#include "src/ast/module.h"
#include "src/writer/hlsl/test_helper.h"

namespace tint {
namespace writer {
namespace hlsl {
namespace {

struct BinaryData {
  const char* result;
  ast::BinaryOp op;
};
inline std::ostream& operator<<(std::ostream& out, BinaryData data) {
  out << data.op;
  return out;
}

using HlslBinaryTest = TestHelperBase<testing::TestWithParam<BinaryData>>;
TEST_P(HlslBinaryTest, Emit) {
  auto params = GetParam();

  auto left = std::make_unique<ast::IdentifierExpression>("left");
  auto right = std::make_unique<ast::IdentifierExpression>("right");

  ast::BinaryExpression expr(params.op, std::move(left), std::move(right));

  ASSERT_TRUE(gen().EmitExpression(pre(), out(), &expr)) << gen().error();
  EXPECT_EQ(result(), params.result);
}
INSTANTIATE_TEST_SUITE_P(
    HlslGeneratorImplTest,
    HlslBinaryTest,
    testing::Values(
        BinaryData{"(left & right)", ast::BinaryOp::kAnd},
        BinaryData{"(left | right)", ast::BinaryOp::kOr},
        BinaryData{"(left ^ right)", ast::BinaryOp::kXor},
        // BinaryData{"(left && right)", ast::BinaryOp::kLogicalAnd},
        // BinaryData{"(left || right)", ast::BinaryOp::kLogicalOr},
        BinaryData{"(left == right)", ast::BinaryOp::kEqual},
        BinaryData{"(left != right)", ast::BinaryOp::kNotEqual},
        BinaryData{"(left < right)", ast::BinaryOp::kLessThan},
        BinaryData{"(left > right)", ast::BinaryOp::kGreaterThan},
        BinaryData{"(left <= right)", ast::BinaryOp::kLessThanEqual},
        BinaryData{"(left >= right)", ast::BinaryOp::kGreaterThanEqual},
        BinaryData{"(left << right)", ast::BinaryOp::kShiftLeft},
        BinaryData{"(left >> right)", ast::BinaryOp::kShiftRight},
        BinaryData{"(left + right)", ast::BinaryOp::kAdd},
        BinaryData{"(left - right)", ast::BinaryOp::kSubtract},
        BinaryData{"(left * right)", ast::BinaryOp::kMultiply},
        BinaryData{"(left / right)", ast::BinaryOp::kDivide},
        BinaryData{"(left % right)", ast::BinaryOp::kModulo}));

}  // namespace
}  // namespace hlsl
}  // namespace writer
}  // namespace tint
