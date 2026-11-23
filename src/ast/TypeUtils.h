#pragma once

#include <string>

#include "ast/Type.h"

// Qualifier-aware type utilities.
//
// A qualifier is stored on the type level it applies to: `const int32*` has an
// unqualified pointer whose base is const, while `int32* const` has a const
// pointer.  These helpers clone types instead of mutating shared Type objects.
struct TypeQualifiers {
    bool isConst = false;
    bool isVolatile = false;
    bool isRestrict = false;
};

// Clone a type shell/payload without deep-cloning nominal aggregates.  Aggregate
// clones keep the same name, fields, and referenced types, so they retain their
// nominal identity; only the qualifier flags on the cloned top level can differ.
Type* cloneType(const Type* type);

// Clone `type` and OR the supplied qualifiers onto its top level.
Type* qualifyType(const Type* type, TypeQualifiers qualifiers);

// Parser-facing helper for `const`/`volatile` written before or after a base
// type.  Returns the original pointer when neither qualifier is present.
Type* qualifyTopLevel(const Type* type, bool isConst, bool isVolatile);

// Structural type identity.  const/volatile at every level, reference-ness,
// pointer targets, aggregate names, array lengths, and function signatures are
// compared.  restrict is deliberately excluded: it is optimization-only.
bool typesIdentical(const Type* a, const Type* b);

// Render qualifiers at the level where they belong, e.g. `const int32*`,
// `int32* const`, and `const int32* const restrict`.
std::string qualifiedTypeToString(const Type* type);
