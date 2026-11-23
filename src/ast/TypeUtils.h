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

// Strip typedef aliases while accumulating qualifiers written on the alias
// (`const CI` where `using CI = int32` must become `const int32`).  Returns the
// aliased type when no alias-level qualifiers were present.
Type* stripTypedefPreservingQualifiers(const Type* type);

// C signature adjustment: top-level const/volatile/restrict on a by-value
// parameter or return is ignored.  Pointee qualifiers are preserved.
Type* adjustParameterTypeForSignature(const Type* type);

// Reference helpers.  A reference is a Pointer with isReference; the
// referent is its base type.  These helpers strip typedefs while preserving
// alias-level qualifiers.
bool isReferenceType(const Type* type);
Type* referenceReferent(const Type* type);
bool typeContainsReference(const Type* type);
bool referencePlacementValid(const Type* type, std::string* reason = nullptr);
bool referenceBindingAllowed(const Type* referenceType, const Type* sourceType,
                             bool sourceIsLValue, bool sourceIsBitField,
                             bool& needsTemporary, std::string* reason = nullptr);

// Restrict placement: true only when the top-level type (after typedef
// stripping) is a restrict-qualified pointer to an object type.
bool isRestrictQualifiedPointer(const Type* type);

// Validate every restrict qualifier in a type tree.  restrict is valid only on
// pointers to object types; it is invalid on non-pointers, references, and
// function pointers.  Typedefs are stripped before each decision.
bool restrictPlacementValid(const Type* type, std::string* reason = nullptr);

// Resolve a (possibly qualified/cloned) nominal record type to the canonical
// registered StructType/ClassType/UnionType used for layout and member lookup.
// Qualifiers are intentionally dropped: callers keep the qualified type for
// access volatility/constness and use this type only for field/bit-field
// indices and LLVM layout. Non-record types are returned after typedef and
// top-level qualifier stripping.
Type* canonicalRecordType(const Type* type);

// Structural type identity.  const/volatile at every level, reference-ness,
// pointer targets, aggregate names, array lengths, and function signatures are
// compared.  restrict is deliberately excluded: it is optimization-only.
bool typesIdentical(const Type* a, const Type* b);

// Lvalue qualification: a member of a const/volatile object is a const/
// volatile lvalue.  Arrays qualify their elements; pointer/reference members
// gain the qualifier on the member itself, not on the pointee/referent.
Type* qualifyMemberLValue(const Type* memberType, bool objectIsConst,
                          bool objectIsVolatile);

// C modifiable-lvalue test: false when the object itself is const or an
// aggregate/array contains a const subobject.  volatile does not prevent
// modification.  When non-null, `reason` receives a short explanation.
bool isModifiableLValueType(const Type* type, std::string* reason = nullptr);

// C pointer assignment compatibility.  The target may add const/volatile at
// the pointer's immediate pointee level, but may not drop qualifiers.  Deeper
// qualifiers may not be added unsafely (`int32** -> const int32**` is false;
// `int32** -> int32* const*` is true).  Void object pointers convert with the
// same qualifier rule; function pointers do not convert to void*.  restrict is
// ignored.
bool pointerConversionAllowed(const Type* target, const Type* source);

// Render qualifiers at the level where they belong, e.g. `const int32*`,
// `int32* const`, and `const int32* const restrict`.
std::string qualifiedTypeToString(const Type* type);
