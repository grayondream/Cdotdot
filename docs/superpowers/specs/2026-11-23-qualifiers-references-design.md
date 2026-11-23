# Qualifiers and lvalue references design

Date: 2026-11-23

## Intent

Fix CDD's type-qualifier and reference semantics as a coherent type-system change:

1. `const` must propagate exactly where C says it propagates, and nowhere else.
2. `volatile` must produce real volatile accesses, not just a parsed flag.
3. `restrict` must be validated as a pointer qualifier and must lower to the
   corresponding LLVM no-alias information.
4. `T&` / `const T&` must be real C++-style lvalue references: non-null aliases
   that bind once, not a hidden pointer with a blanket "only operator[]" rule.

This is a correctness project, not a performance experiment. No new CLI flags
are planned; the new rules become the language semantics.

## Observed failures

All cases below were reproduced with the current compiler (`048a689`), using
`build/bin/cdd -S -O 2`. No product code was changed for the investigation.

| Case | Current behavior | Required behavior |
|---|---|---|
| `const int32* p; int32* q = p;` | accepted | error: discards `const` |
| `float64* p; int32* q = p;` | accepted | error: incompatible pointer targets |
| `const S s; s.x = 1;` | accepted | error: member of const object |
| `const S* p; p->x = 1;` | accepted | error: member of const object |
| `const S s; S* p = &s;` | accepted | error: discards `const` |
| `const S s; S* f(); f(&s);` | accepted | error: discards `const` |
| `using CI = const int32; CI x; x = 2;` | accepted | error: alias preserves const |
| `int32* const p` diagnostic | printed as `const int32*` | print `int32* const` |
| `const int32* p` diagnostic | printed as `int32*` | print `const int32*` |
| `volatile int32 x; x = 1; x = 2;` | first store removed at `-O2` | both accesses volatile |
| `volatile` pointer/field/parameter | normal loads/stores | volatile loads/stores |
| `restrict int32 g;` | silently accepted | error: restrict requires pointer |
| `int32* restrict p` parameter | no `noalias` | `noalias` contract |
| `int32& r = x;` | rejected | real reference |
| `void f(int32& r)` | rejected | real reference parameter |
| `int32& f()` | rejected | real reference return |
| `struct S { int32& r; };` | rejected | real reference member |
| `int32& operator[](int32)` | special case that works | ordinary reference return |

## Goals

- Implement C qualifier rules for `const`, `volatile`, and `restrict`:
  qualifier-aware type identity, assignment, initialization, argument passing,
  return, member access, casts where applicable, and code generation.
- Make `const` objects and `const` pointers actually const.
- Add `const` member functions so `const` objects can call methods without
  losing constness.
- Generate `load volatile` / `store volatile` for every access through a
  volatile lvalue, including pointers, fields, parameters, and aggregates.
- Generate LLVM no-alias information for valid `restrict` declarations and
  reject invalid placements.
- Implement C++-style **lvalue references**: `T&`, `const T&`, and `volatile T&`.
- Keep the existing `operator[]` reference-return behavior working through the
  generic reference implementation, not through a special case.
- Add regression tests for every failure above, plus positive tests for the
  cases that must continue to work.

## Non-goals

- Rvalue references (`T&&`) and move semantics.
- Reference collapsing, reference arrays, pointers to references,
  references to functions, or references to arrays.
- Reference rebinding after initialization.
- Reference members in unions. C++ forbids them and CDD will diagnose them.
- `mutable` members.
- C++ constructors/destructors, copy/move special members, or virtual dispatch
  changes.
- Runtime `restrict` violation checks. Restrict remains an optimization
  contract; violating it is undefined behavior.
- Overloading a function purely on `T` versus `T&`, or purely on a `const`
  member function versus a non-const member function. These are diagnosed as
  duplicate/ambiguous signatures in this version.
- Changing the default ABI width of references: they remain pointer-sized in
  LLVM IR.

## Decisions

### 1. Type-qualifier model

`Type` keeps the existing per-level flags:

- `isConst`
- `isVolatile`
- `isRestrict`
- `isAtomic`
- `isReference`

A pointer/reference level stores qualifiers that apply to **that level**. Its
`base` stores the pointee qualifiers. Examples:

- `const int32*` = pointer level unqualified, base `const int32`.
- `int32* const` = pointer level `isConst`, base `int32`.
- `int32* restrict` = pointer level `isRestrict`.
- `const int32* const restrict` = base const, pointer const + restrict.
- `const int32&` = reference level, base `const int32`.

Rules:

- Never mutate a shared `Type` object. All qualifier application goes through a
  `qualifyType(Type*, Qualifiers)` helper that clones the complete type,
  including StructType/ClassType/UnionType/EnumType/TypedefType subclasses.
- The current scalar-only clone in `parseType` is replaced. The direct
  `baseType->isConst = ...` mutation is removed.
- Type identity (`typesIdentical`) compares `const`/`volatile` at every level,
  reference-ness, pointer targets, aggregate names, array lengths, etc.
  `restrict` is tracked for validation/codegen but is not part of identity,
  overload matching, or mangling.
- Assignment compatibility is a separate function and follows C rules; it does
  not reuse type identity blindly.
- `typeToString` prints qualifiers at the level where they belong. In
  particular:
  - `const int32*` prints `const int32*`
  - `int32* const` prints `int32* const`
  - `int32* restrict` prints `int32* restrict`
  - `const int32&` prints `const int32&`

#### Signature identity and mangling

Function type identity follows C/C++ signature adjustment rather than raw
spelling:

- Top-level qualifiers on a by-value parameter or return are ignored:
  `void f(const int32)` and `void f(int32)` are the same signature, and
  `const S f()` and `S f()` are the same signature.
- Top-level pointer qualifiers on a parameter are ignored:
  `void f(int32* const)` and `void f(int32*)` are the same signature.
- Pointee qualifiers are part of the type identity and are included in the
  mangled name: `void f(int32*)` and `void f(const int32*)` are distinct
  overloads.
- Reference-ness is part of type identity and mangling: `f(T&)` and `f(T*)`
  are distinct; `f(T&)` and `f(const T&)` are distinct.
- `restrict` is an optimization qualifier only. It does not participate in
  overload identity, signature matching, or mangling. Top-level restrict on a
  parameter is dropped for signature purposes, just like top-level const.
- A const member function is recorded for out-of-class definition matching and
  for `this` typing, but because const-only overloads are rejected, it does not
  add a separate lookup/mangling suffix. A const and non-const method with the
  same parameter list are diagnosed as duplicates.

### 2. C assignment and conversion rules

The qualifier conversion helper is used by variable initialization, assignment,
function arguments, returns, aggregate initialization, and ternary/common-type
selection.

For pointer targets:

- Base types must be compatible: `int32*` is not `float64*`, and `S*` is not
  `T*` unless one is a base class of the other where CDD already permits it.
- Adding `const`/`volatile` to the pointed-to type is allowed:
  `int32* -> const int32*`, `int32* -> volatile int32*`.
- Dropping `const`/`volatile` is an error:
  `const int32* -> int32*`, `volatile int32* -> int32*`.
- Multi-level qualifiers follow C's safe rules:
  - `int32** -> int32* const*` is allowed.
  - `int32** -> const int32**` is an error.
  - `const int32** -> int32**` is an error.
- `void*` conversions are allowed between object pointers, with the same
  qualifier rules. Function pointers do not convert to `void*`.
- Array-to-pointer decay produces a pointer to the array element, preserving
  element qualifiers.
- Integer-to-pointer conversion is limited to a null pointer constant (`0`), not
  any arbitrary integer.
- Top-level qualifiers on the destination are checked by the modifiable-lvalue
  rule, not by type compatibility. Assigning to a `const` object is an error;
  assigning a `const` value to a mutable object is allowed.

For non-pointers:

- Arithmetic conversions ignore top-level qualifiers.
- Aggregate copy/initialization copies values; it does not propagate the source
  object's `const` to the destination.
- A typedef is stripped before qualifier decisions; `using CI = const int32`
  therefore behaves exactly like `const int32`.

### 3. Const propagation through lvalues

Lvalue qualification is computed from the object expression, not only from the
declared field type.

- A member of a `const` object is a const lvalue, even if the field was declared
  without `const`.
- A member reached through `const S*` is a const lvalue.
- A pointer member of a `const` object is a **const pointer**; its pointee is
  not automatically const.
- An array member of a `const` object has const elements.
- A reference member does not propagate the containing object's constness to its
  referent; references are not objects.
- `const` arrays have const elements through the existing array-element type.
- `const` globals remain read-only; local/parameter/field access uses the same
  lvalue rules.

The helper is `qualifyMemberLValue(Type* memberType, bool objectIsConst,
bool objectIsVolatile)`. It clones the member type and adds top-level
qualifiers, recursing into array element types but not into pointer pointees or
reference targets.

`AssignmentExprAST` uses the lvalue-qualified type for its modifiable-lvalue
check, after stripping typedefs. It no longer looks only at
`lhsType->isConst`.

Aggregate assignment is rejected if the destination contains a const
subobject: a const field, a const array element, or a base subobject with a
const member. This mirrors C's "modifiable lvalue" rule and C++'s deleted
implicit assignment. Copy-initialization from such an aggregate remains legal;
it reads the source and does not write through the const subobject. Codegen
never emits a whole-aggregate store into an object containing a const
subobject.

The same recursive rule applies to volatile and reference members where a
whole-object write would be undefined by the language rules below.

### 4. Const member functions

Grammar: an optional trailing `const` after the parameter list:

```cdd
class C {
public:
    int32 get() const;
};
```

- `FunctionType`/method metadata records `isConstMethod`.
- `this` inside a const method has type `const C*`; otherwise `C*`.
- A const object may call only const methods. A non-const object may call both.
- A const method may not:
  - assign to non-reference fields through `this`;
  - call a non-const method on `this`;
  - pass `this` to a `C*` parameter without an explicit cast.
- A const method may read fields and call other const methods.
- `static` methods cannot be `const`.
- This version does not overload solely on constness; a duplicate method name
  with the same parameter types is diagnosed.

#### Built-in types and const

The built-in view/container types get an explicit const policy rather than
silently ignoring the object qualifier:

- `const string`: `len`, `data`/`c_str`-style reads and other non-mutating
  methods are allowed; mutating built-ins (`append`, `push`, `destroy`,
  `clear`, and equivalents) are rejected. `data()` on a const `string` yields
  `const char*`, never a mutable `char*`.
- `const optional<T>` / `const result<T,E>`: reading `has_value`, tags, and
  value/error accessors is allowed; mutating/assigning accessors are rejected.
- `const str` / `const string_view`: the view object is read-only; its byte
  data is already read-only (`const char*`).
- `const slice<T>` means the view object is const, not that its elements are
  const. `s[i]` still denotes a `T` lvalue unless the element type itself is
  const. A view of const elements is written `const T[]` / a slice whose
  element type is const.
- Built-in method dispatch is const-aware: the same object-constness checks
  used for user methods are applied to the built-in method table.

### 5. Volatile access semantics

Every access through a volatile-qualified lvalue emits a volatile LLVM access:

- scalar load -> `load volatile`
- scalar store -> `store volatile`
- pointer load/store -> volatile pointer access
- field load/store -> volatile access at the field address
- parameter/local/global -> same rule
- aggregate copy where the source or destination is volatile -> volatile
  memcpy or field-wise volatile loads/stores
- `volatile` pointer target and `volatile` pointer itself are distinct:
  - `volatile int32*` affects the pointee access.
  - `int32* volatile` affects the pointer variable access.

`volatile` is not a hint: it cannot be dropped by optimization, and two
successive stores to the same volatile object must both survive. Volatile does
not imply atomicity; `atomic volatile` keeps both properties.

The current `readLValue`/store paths are centralized so volatile bits are
applied in one place. Special paths (bit-fields, string assignment, aggregate
copy, atomics) receive explicit volatile handling.

### 6. Restrict

Placement and validation:

- `restrict` is valid only on pointer types.
- It is invalid on non-pointer objects, references, function types, and
  non-object pointer targets (function pointers).
- `restrict int32 g;` is a parse/sema error; `int32* restrict p;` is valid.
- Typedefs are stripped before validation, so `using RP = int32* restrict;` is
  valid, while `using RI = int32 restrict;` is not.
- `restrict` does not participate in assignment compatibility; it is an
  optimization qualifier on a pointer level.
- A file-scope pointer or a pointer member may be restrict-qualified, as C
  permits. They are validated like any other pointer, but no per-access noalias
  metadata is emitted for those storage locations; the qualifier is accepted
  and has no additional optimization effect. This is documented rather than
  silently claiming a guarantee.

Lowering:

- A restrict-qualified pointer parameter gets LLVM `noalias` on that argument.
- A restrict-qualified pointer return gets LLVM `noalias` on the return value.
- A block-scope restrict pointer is lowered with
  `llvm.experimental.noalias.scope.decl` and alias-scope metadata on accesses
  through that pointer. If LLVM's metadata cannot be attached safely for a
  particular access shape, the compiler conservatively omits the metadata for
  that access rather than claiming a false alias guarantee.
- No runtime checks are emitted.
- `restrict` copies and derived pointers follow C's based-on model at the
  source level; the implementation may only attach metadata where it can prove
  the access is through the restrict pointer. This is an optimization
  completeness limit, not a semantic relaxation.

### 7. Lvalue references

#### Representation

A reference is represented as `Pointer` with `isReference = true`. The LLVM
representation is a pointer to the referent. This keeps the ABI stable and
reuses the existing pointer lowering, but references are a distinct semantic
type:

- `isReferenceType` is true.
- `typesIdentical` distinguishes `T&` from `T*`.
- Mangling includes a reference marker, so `f(T&)` and `f(T*)` are distinct
  symbols.
- `sizeof(T&)` and `alignof(T&)` report the referent's size/alignment.
- A reference variable/field occupies pointer size in layout.

#### Binding rules

- A reference must be initialized when declared.
- `T&` binds only to a non-const, non-volatile lvalue of compatible type.
- `const T&` binds to a non-const lvalue, a const lvalue, or an rvalue. It may
  also bind through a standard conversion when the source is an rvalue (for
  example `const float64& r = int32Expr;`), with the converted temporary
  materialized.
- `volatile T&` may bind to a non-volatile or volatile lvalue; adding volatile
  is allowed, dropping volatile is not. `int32 x; volatile int32& r = x;` is
  valid.
- `const volatile T&` binds to all of the above, lvalue or rvalue.
- `T&` does not bind to a const object, a volatile object, an rvalue, or a
  bit-field.
- Binding never inserts a null value; there is no null reference.
- A reference cannot be rebound. Assignment to a reference expression assigns
  to the referent.
- A reference cannot bind to an incompatible type.
- A reference to a reference is not supported.
- `T&&` is rejected as an unsupported rvalue reference.
- Single-inheritance derived-to-base reference binding is allowed; codegen
  forms the base-subobject address using the existing class layout. Binding
  that would require a non-zero, unsupported base-subobject adjustment is
  diagnosed rather than silently pointing at the wrong subobject.

#### Expression behavior

For a reference expression:

- The expression type is the referent type, not the reference type.
- The expression is an lvalue.
- Reading it reads the referent.
- Assigning it writes the referent.
- `&ref` yields the referent address.
- `ref.member`, `ref[i]`, and method calls use the referent.
- `sizeof(ref)` is the referent size.

#### Declarations

Supported:

- local reference variables
- function parameters
- function returns
- struct/class fields (not union fields)
- template parameters and returns
- `using` aliases
- const and volatile referents
- references to pointers (`T*&`)

Template support uses C++-style lvalue-reference deduction:

- For a `T&` parameter, the argument must be an lvalue; `T` is deduced from the
  referent, preserving source cv-qualifiers.
- For a `const T&` / `volatile T&` parameter, cv-qualifiers on the argument are
  absorbed as in C++ (`f<int>(const int)` still deduces `T = int` for
  `const T&`), and rvalues may bind through a materialized temporary.
- No rvalue-reference deduction is added.

Not supported:

- reference arrays (`T& a[3]`)
- pointers to references (`T&*`)
- references to arrays or functions
- union reference members
- rvalue references
- reference rebinding
- `auto&` deduction in this version

#### Initialization and lifetime

- Local `const T&` binding to an rvalue materializes the temporary in the
  enclosing function and extends its lifetime to the end of the reference's
  scope.
- A `const T&` function parameter may bind to a temporary for the duration of
  the call; the temporary is materialized at the call site.
- A reference return must return an lvalue. Returning a reference to a temporary
  is rejected.
- A reference member initialized from a temporary is rejected; the referent
  must outlive the aggregate.
- Dangling references are the programmer's responsibility only after these
  compile-time rules; no runtime lifetime checking is added.

#### Aggregates with reference members

- Aggregate initialization must bind every reference member.
- Copy-initialization of an aggregate copies the reference binding (the copy
  refers to the same object), matching C++ copy construction.
- Assignment of a whole aggregate containing a reference member is rejected
  (C++ deletes the implicit assignment). Individual referents remain assignable
  through the reference member.
- A reference member has no default/zero initialization.

#### References and operator[]

The existing writable `operator[]` return is implemented through the generic
reference return path. The special case in sema is removed.

### 8. Diagnostics

New or tightened diagnostics:

- discarding `const`/`volatile` in pointer conversion
- incompatible pointer target types
- assignment to a const object/member
- calling a non-const method on a const object
- invalid `const` on a static method
- `volatile` access on an unsupported aggregate shape
- `restrict` on a non-pointer type
- `restrict` on a function pointer/reference
- uninitialized reference
- binding a non-const reference to a const lvalue or rvalue
- binding a reference to a bit-field
- returning a reference to a temporary
- reference member without an initializer
- assignment of an aggregate containing a reference member
- unsupported rvalue reference/reference array/pointer to reference
- duplicate overload that differs only by reference or const-method qualifier

Diagnostics should use the existing diagnostic codes where they fit and add
specific message text with the correct `typeToString` spelling.

## Implementation structure

- `src/ast/Type.h` / `Type.cpp`: qualifier-aware cloning, type identity helpers,
  reference helpers, size/alignment rules.
- `src/frontend/Parser.cpp`: stop mutating shared types; parse trailing
  `const` methods; record reference/qualifier placement; reject invalid
  `restrict`.
- `src/sema/SemanticAnalyzer.cpp`: qualified lvalue propagation; assignment
  compatibility; const/non-const method calls; reference binding, parameters,
  returns, fields; aggregate reference rules; `sizeof`/`alignof`.
- `src/sema/TemplateInstantiator.cpp`: preserve/clone all qualifiers without
  shared-type mutation.
- `src/sema/CompileTimeEvaluator.cpp`: qualifier-aware type predicates and
  `const_of`.
- `src/ast/Symbol.cpp` / `Mangle.cpp`: qualified identity, conversion rank,
  reference/const-method mangling.
- `src/ast/Decl.cpp`, `src/ast/Expr.cpp`, `src/codegen/CodegenContext.cpp`:
  volatile load/store, reference variable/parameter/return/member lowering,
  noalias attributes and scope metadata.
- `src/sema/SemanticAnalyzer.h`: helper declarations.
- Tests:
  - `tests/frontend/test_parser.cpp`
  - new `tests/sema/test_qualifiers.cpp`
  - new `tests/sema/test_references.cpp`
  - new `tests/e2e/test_qualifiers.cpp`
  - new `tests/e2e/test_references.cpp`
  - IR-level assertions for volatile/noalias/reference ABI in a focused e2e or
    driver test.
  - update existing const/pointer/reference tests, including replacing the
    "general references are rejected" tests.

## Test strategy

Every negative case in the failure table becomes a failing test first, then a
diagnostic assertion. Every positive case gets a runtime or IR assertion.

Qualifier tests:

- pointer target qualifier add/drop, multi-level pointers, void*, typedefs
- const object/member/array/global behavior
- const method call matrix
- const built-in method matrix (`string`, `slice`, `optional`, `result`, views)
- aggregate assignment rejection for const subobjects
- signature/mangling: top-level cv ignored, pointee cv distinct, reference
  distinct, restrict not part of the signature
- volatile load/store and dead-store preservation
- restrict placement, global/member acceptance, and `noalias` attributes

Reference tests:

- local bind/read/write, `&ref`, `sizeof`
- const/non-const/volatile binding matrix
- parameter and return behavior
- reference fields and aggregate initialization
- aggregate copy versus assignment rejection
- temporary lifetime for local/parameter const references
- template deduction for `T&` / `const T&` parameters and returns
- derived-to-base reference binding where the layout supports it
- operator[] migration
- negative cases: uninitialized, rebind attempts (assign writes referent), bit-
  field, rvalue to non-const ref, return temporary, union reference member,
  unsupported shapes

Acceptance:

- full `compiler_tests` suite passes
- `examples/all_features.cdd` prints `ALL FEATURES OK`
- the four failure classes above are covered by tests
- no existing language feature regresses
- no benchmark checksum changes

## Risks and rollout

This touches type identity, overload resolution, layout, and codegen. The work
is split into independently verifiable phases:

1. Qualifier type layer, cloning, printing, identity, pointer conversion.
2. const lvalue propagation and const member functions.
3. volatile loads/stores and aggregate access.
4. restrict validation, parameter/return noalias, block-scope scopes.
5. lvalue reference type, binding, parameters/returns/fields, lifetime.
6. Migration: update `libs/std`, examples, and existing tests; full suite and
   benchmark checksum gate.

Each phase must keep the full suite green and must not silently weaken existing
const-string or compile_time behavior. Stricter pointer compatibility may expose
latent bugs in `libs/std` or tests; those are fixed as part of the migration,
not by weakening the checker.

## Builder calls

These are the calls made in this document that a builder could settle without
changing the overall direction:

- Keep `Pointer + isReference` rather than adding `TypeKind::Reference`; it
  minimizes ABI/switch churn while still making references distinct in identity
  and diagnostics.
- Reject const-only and reference-only overload pairs instead of implementing
  C++ overload ranking on qualifiers in this version.
- Use C signature adjustment: top-level cv/restrict on by-value parameters and
  returns is ignored; pointee cv and reference-ness are part of identity and
  mangling.
- Extend temporary lifetime only for local/parameter `const T&`; reject
  temporary-bound reference members and returned temporaries.
- Treat non-provable block-scope restrict accesses conservatively: omit
  metadata rather than overstate aliasing. File-scope and member restrict are
  accepted but do not receive per-access metadata.
- Keep reference ABI pointer-sized in LLVM IR.
- Support references in template parameters/returns with lvalue-reference
  deduction; do not add rvalue-reference deduction.
- Reject reference members in unions.
- Apply constness to built-in methods through an explicit table, including the
  `const string` -> `const char*` data rule.
