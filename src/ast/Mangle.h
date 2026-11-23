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
std::string mangleFunction(const std::string& name, const std::vector<Type*>& paramTypes);

// Mark a function name as having C linkage (extern "C" / extern declarations):
// it will be emitted with its plain, unmangled symbol name.
void markCName(const std::string& name);
void markNamespaceCName(const std::string& name);
bool isCName(const std::string& name);
bool isNamespaceCName(const std::string& name);