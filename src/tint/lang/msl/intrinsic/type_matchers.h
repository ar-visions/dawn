// Copyright 2024 The Dawn & Tint Authors
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its
//    contributors may be used to endorse or promote products derived from
//    this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef SRC_TINT_LANG_MSL_INTRINSIC_TYPE_MATCHERS_H_
#define SRC_TINT_LANG_MSL_INTRINSIC_TYPE_MATCHERS_H_

#include "src/tint/lang/core/intrinsic/table.h"
#include "src/tint/lang/core/type/manager.h"
#include "src/tint/lang/msl/type/level.h"

namespace tint::msl::intrinsic {

inline bool MatchLevel(core::intrinsic::MatchState&, const core::type::Type* ty) {
    if (ty->Is<msl::type::Level>()) {
        return true;
    }
    return false;
}

inline const core::type::Type* BuildLevel(core::intrinsic::MatchState& state,
                                          const core::type::Type*) {
    return state.types.Get<type::Level>();
}

}  // namespace tint::msl::intrinsic

#endif  // SRC_TINT_LANG_MSL_INTRINSIC_TYPE_MATCHERS_H_
