// Copyright 2021 The Tint Authors.
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

#ifndef SRC_TINT_RESOLVER_DEPENDENCY_GRAPH_H_
#define SRC_TINT_RESOLVER_DEPENDENCY_GRAPH_H_

#include <vector>

#include "src/tint/ast/module.h"
#include "src/tint/diagnostic/diagnostic.h"
#include "src/tint/sem/builtin_type.h"
#include "src/tint/type/builtin.h"
#include "src/tint/utils/hashmap.h"

namespace tint::resolver {

/// ResolvedIdentifier holds the resolution of an ast::Identifier.
/// Can hold one of:
/// - const ast::TypeDecl*  (as const ast::Node*)
/// - const ast::Variable*  (as const ast::Node*)
/// - const ast::Function*  (as const ast::Node*)
/// - sem::BuiltinType
/// - type::Builtin
class ResolvedIdentifier {
  public:
    ResolvedIdentifier() = default;

    /// Constructor
    /// @param value the resolved identifier value
    template <typename T>
    ResolvedIdentifier(T value) : value_(value) {}  // NOLINT(runtime/explicit)

    /// @return true if the ResolvedIdentifier holds a value (successfully resolved)
    operator bool() const { return !std::holds_alternative<std::monostate>(value_); }

    /// @return the node pointer if the ResolvedIdentifier holds an AST node, otherwise nullptr
    const ast::Node* Node() const {
        if (auto n = std::get_if<const ast::Node*>(&value_)) {
            return *n;
        }
        return nullptr;
    }

    /// @return the builtin function if the ResolvedIdentifier holds sem::BuiltinType, otherwise
    /// sem::BuiltinType::kNone
    sem::BuiltinType BuiltinFunction() const {
        if (auto n = std::get_if<sem::BuiltinType>(&value_)) {
            return *n;
        }
        return sem::BuiltinType::kNone;
    }

    /// @return the builtin type if the ResolvedIdentifier holds type::Builtin, otherwise
    /// type::Builtin::kUndefined
    type::Builtin BuiltinType() const {
        if (auto n = std::get_if<type::Builtin>(&value_)) {
            return *n;
        }
        return type::Builtin::kUndefined;
    }

    /// @param value the value to compare the ResolvedIdentifier to
    /// @return true if the ResolvedIdentifier is equal to @p value
    template <typename T>
    bool operator==(const T& value) const {
        if (auto n = std::get_if<T>(&value_)) {
            return *n == value;
        }
        return false;
    }

    /// @param other the other value to compare to this
    /// @return true if this ResolvedIdentifier and @p other are not equal
    template <typename T>
    bool operator!=(const T& other) const {
        return !(*this == other);
    }

  private:
    std::variant<std::monostate, const ast::Node*, sem::BuiltinType, type::Builtin> value_;
};

/// @param out the std::ostream to write to
/// @param ri the ResolvedIdentifier
/// @returns `out` so calls can be chained
std::ostream& operator<<(std::ostream& out, const ResolvedIdentifier& ri);

/// DependencyGraph holds information about module-scope declaration dependency
/// analysis and symbol resolutions.
struct DependencyGraph {
    /// Constructor
    DependencyGraph();
    /// Move-constructor
    DependencyGraph(DependencyGraph&&);
    /// Destructor
    ~DependencyGraph();

    /// Build() performs symbol resolution and dependency analysis on `module`,
    /// populating `output` with the resulting dependency graph.
    /// @param module the AST module to analyse
    /// @param symbols the symbol table
    /// @param diagnostics the diagnostic list to populate with errors / warnings
    /// @param output the resulting DependencyGraph
    /// @returns true on success, false on error
    static bool Build(const ast::Module& module,
                      const SymbolTable& symbols,
                      diag::List& diagnostics,
                      DependencyGraph& output);

    /// All globals in dependency-sorted order.
    utils::Vector<const ast::Node*, 32> ordered_globals;

    /// Map of ast::Identifier to a ResolvedIdentifier
    utils::Hashmap<const ast::Identifier*, ResolvedIdentifier, 64> resolved_identifiers;

    /// Map of ast::Variable to a type, function, or variable that is shadowed by
    /// the variable key. A declaration (X) shadows another (Y) if X and Y use
    /// the same symbol, and X is declared in a sub-scope of the scope that
    /// declares Y.
    utils::Hashmap<const ast::Variable*, const ast::Node*, 16> shadows;
};

}  // namespace tint::resolver

#endif  // SRC_TINT_RESOLVER_DEPENDENCY_GRAPH_H_
