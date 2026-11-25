#pragma once
#include <string>
#include <vector>

class Type;

// Namespace/class/module qualification uses `.` as its separator.  `.` is not
// part of a source identifier, so `ns::f` / `ns.f` never collide with the flat
// source name `ns_f`.
std::string mangleQualifiedName(const std::string& name);

std::string typeToMangled(Type* type);
// Signature mangling: applies C parameter adjustment (top-level cv/restrict
// ignored) and preserves pointee cv and reference-ness.
std::string typeToMangledSignature(Type* type);
std::string mangleFunction(const std::string& name,
                           const std::vector<Type*>& paramTypes,
                           bool isVarArg = false,
                           const std::string& moduleName = {});

// Add the module qualifier to an already-mangled CDD function symbol when it
// carries none. Raw C names and `main` are returned unchanged.
std::string qualifyFunctionSymbolForModule(const std::string& symbol,
                                           const std::string& moduleName);

// Resolve the symbol used at a call/declaration site. Sema normally supplies
// `preferredSymbol`; when it is absent (structural function type, legacy
// path), fall back to mangling `name` plus the declared parameter types.
// This keeps the "prefer sema's symbol, otherwise re-mangle" rule in one place.
std::string resolveFunctionSymbol(const std::string& preferredSymbol,
                                  const std::string& name,
                                  const std::vector<Type*>& paramTypes,
                                  bool isVarArg = false,
                                  const std::string& moduleName = {});

// File-scope variable/array symbol. `$` keeps it disjoint from source-level
// identifiers, C symbols, and the CDD function namespace; participating
// modules add their module name so private same-name variables do not collide.
std::string mangleGlobalVariable(const std::string& name,
                                 const std::string& moduleName = {});

// Mark a function signature as having C linkage (extern "C" / extern
// declarations): only that signature is emitted with its plain, unmangled
// symbol name. A CDD overload sharing the name keeps its mangled symbol.
void markCName(const std::string& name, const std::vector<Type*>& paramTypes,
               bool isVarArg);
void markNamespaceCName(const std::string& name,
                        const std::vector<Type*>& paramTypes, bool isVarArg);
bool isCName(const std::string& name);
bool isNamespaceCName(const std::string& name);