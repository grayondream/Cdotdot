#include "Mangle.h"
#include "ast/Type.h"
#include "ast/TypeUtils.h"
#include <unordered_set>

static std::unordered_set<std::string>& cNames() {
    static std::unordered_set<std::string> names;
    return names;
}

// C-linkage externs declared *inside a namespace body* (e.g. `std::sqrt`).
// Only these participate in the `ns::f` → bare-`f` resolution fallback, so
// a global-scope extern (prelude `malloc`, `strlen`, ...) is NOT reachable
// as `std::malloc` / `std::strlen` (F2 — namespace isolation preserved).
static std::unordered_set<std::string>& namespaceCNames() {
    static std::unordered_set<std::string> names;
    return names;
}

void markCName(const std::string& name) {
    cNames().insert(name);
}

void markNamespaceCName(const std::string& name) {
    cNames().insert(name);
    namespaceCNames().insert(name);
}

bool isCName(const std::string& name) {
    return cNames().count(name) != 0;
}

bool isNamespaceCName(const std::string& name) {
    return namespaceCNames().count(name) != 0;
}

std::string mangleQualifiedName(const std::string& name) {
    // `.` is not a source-identifier character, so it separates qualification
    // components without colliding with names that contain `_`.
    std::string out;
    out.reserve(name.size());
    for (size_t i = 0; i < name.size(); ++i) {
        if (name[i] == ':' && i + 1 < name.size() && name[i + 1] == ':') {
            out.push_back('.');
            ++i;
        } else if (name[i] == '.') {
            out.push_back('.');
        } else {
            out.push_back(name[i]);
        }
    }
    return out;
}

namespace { std::string signatureTypeToMangled(const Type* type); }

std::string typeToMangled(Type* type) {
    // Template arguments and conversion-operator names must preserve cv at
    // every level, just like signature mangling (but without the by-value
    // top-level adjustment performed by typeToMangledSignature).
    return signatureTypeToMangled(type);
}


namespace {

std::string qualifierPrefix(const Type* type) {
    std::string result;
    if (type->isConst) result += "C";
    if (type->isVolatile) result += "V";
    if (type->isAtomic) result += "atomic";
    return result;
}

// Variable-length names are length-prefixed so a name like `int32ptr` cannot
// be confused with the encoding of `int32*`.  The payload may contain any
// character (including `_`, `.`, `$`, and `:`); the decimal length tells the
// decoder exactly where it ends.
std::string lengthPrefixedName(const std::string& name) {
    return std::to_string(name.size()) + ":" + name;
}

// `mangleFunction` uses a single `_` between the function name and each
// parameter.  Doubling underscores inside the name makes that separator
// unambiguous: a zero-parameter function literally named `f_int32` and a
// one-parameter function `f(int32)` can no longer both become `f_int32`.
std::string escapeMangledName(const std::string& name) {
    std::string out;
    out.reserve(name.size() * 2);
    for (char c : name) {
        out.push_back(c);
        if (c == '_') out.push_back('_');
    }
    return out;
}

// Signature mangling preserves cv-qualifiers at every level and distinguishes
// references from pointers.  restrict is intentionally absent.
std::string signatureTypeToMangled(const Type* type) {
    if (!type) return "unknown";
    switch (type->kind) {
        case TypeKind::Pointer: {
            std::string result = signatureTypeToMangled(type->base);
            result += type->isReference ? "ref" : "ptr";
            if (type->isConst) result += "C";
            if (type->isVolatile) result += "V";
            return result;
        }
        case TypeKind::Array: {
            const auto* arr = static_cast<const ArrayType*>(type);
            std::string result =
                signatureTypeToMangled(arr->elementType) + "arr";
            if (!arr->sizeParam.empty())
                result += "p" + lengthPrefixedName(arr->sizeParam);
            result += "s" + std::to_string(arr->size);
            return result;
        }
        case TypeKind::Struct:
            return qualifierPrefix(type) + "N" +
                   lengthPrefixedName(static_cast<const StructType*>(type)->name);
        case TypeKind::Class:
            return qualifierPrefix(type) + "N" +
                   lengthPrefixedName(static_cast<const ClassType*>(type)->name);
        case TypeKind::Union:
            return qualifierPrefix(type) + "N" +
                   lengthPrefixedName(static_cast<const UnionType*>(type)->name);
        case TypeKind::Enum: {
            const auto* e = static_cast<const EnumType*>(type);
            return qualifierPrefix(type) +
                   (e->name.empty() ? "int32"
                                    : "N" + lengthPrefixedName(e->name));
        }
        case TypeKind::Void: return qualifierPrefix(type) + "void";
        case TypeKind::Nullptr: return qualifierPrefix(type) + "nullptr";
        case TypeKind::Char: return qualifierPrefix(type) + "char";
        case TypeKind::Bool: return qualifierPrefix(type) + "bool";
        case TypeKind::Int8: return qualifierPrefix(type) + "int8";
        case TypeKind::Int16: return qualifierPrefix(type) + "int16";
        case TypeKind::Int32: return qualifierPrefix(type) + "int32";
        case TypeKind::Int64: return qualifierPrefix(type) + "int64";
        case TypeKind::Int128: return qualifierPrefix(type) + "int128";
        case TypeKind::UInt8: return qualifierPrefix(type) + "uint8";
        case TypeKind::UInt16: return qualifierPrefix(type) + "uint16";
        case TypeKind::UInt32: return qualifierPrefix(type) + "uint32";
        case TypeKind::UInt64: return qualifierPrefix(type) + "uint64";
        case TypeKind::UInt128: return qualifierPrefix(type) + "uint128";
        case TypeKind::ISize: return qualifierPrefix(type) + "isize";
        case TypeKind::USize: return qualifierPrefix(type) + "usize";
        case TypeKind::Float16: return qualifierPrefix(type) + "float16";
        case TypeKind::Float32: return qualifierPrefix(type) + "float32";
        case TypeKind::Float64: return qualifierPrefix(type) + "float64";
        case TypeKind::Float128: return qualifierPrefix(type) + "float128";
        case TypeKind::Str: return qualifierPrefix(type) + "str";
        case TypeKind::String: return qualifierPrefix(type) + "string";
        case TypeKind::Slice:
            return qualifierPrefix(type) +
                   signatureTypeToMangled(
                       static_cast<const SliceType*>(type)->elementType) +
                   "slice";
        case TypeKind::Optional:
            return qualifierPrefix(type) +
                   signatureTypeToMangled(
                       static_cast<const OptionalType*>(type)->elementType) +
                   "opt";
        case TypeKind::Result: {
            const auto* r = static_cast<const ResultType*>(type);
            // The error payload is length-prefixed so an outer constructor
            // (e.g. `result<A,B>*`) cannot look like `result<A,B*>`.
            return qualifierPrefix(type) + signatureTypeToMangled(r->successType) +
                   "res" +
                   lengthPrefixedName(signatureTypeToMangled(r->errorType));
        }
        case TypeKind::Typedef:
            return qualifierPrefix(type) +
                   signatureTypeToMangled(
                       static_cast<const TypedefType*>(type)->aliasedType);
        case TypeKind::TypeVar:
            return qualifierPrefix(type) + "X" +
                   lengthPrefixedName(static_cast<const TypeVarType*>(type)->name);
        case TypeKind::TypeInstance: {
            const auto* ti = static_cast<const TypeInstanceType*>(type);
            std::string result =
                qualifierPrefix(type) + "I" + lengthPrefixedName(ti->templateName);
            result += std::to_string(ti->typeArgs.size()) + ":";
            for (const auto* arg : ti->typeArgs)
                result += signatureTypeToMangled(arg);
            result += std::to_string(ti->valueArgs.size()) + ":";
            for (long long value : ti->valueArgs)
                result += "v" + lengthPrefixedName(std::to_string(value));
            result += std::to_string(ti->valueArgNames.size()) + ":";
            for (const auto& argName : ti->valueArgNames)
                result += lengthPrefixedName(argName);
            return result;
        }
        case TypeKind::Function: {
            const auto* fn = static_cast<const FunctionType*>(type);
            std::string result = qualifierPrefix(type) +
                                 signatureTypeToMangled(
                                     adjustParameterTypeForSignature(fn->returnType)) +
                                 "fn" + std::to_string(fn->paramTypes.size()) + ":";
            for (const auto* param : fn->paramTypes)
                result += signatureTypeToMangled(
                    adjustParameterTypeForSignature(param));
            result += fn->isVarArg ? "v" : "n";
            return result;
        }
        default:
            return qualifierPrefix(type) + "unknown";
    }
}

} // namespace

std::string typeToMangledSignature(Type* type) {
    return signatureTypeToMangled(adjustParameterTypeForSignature(type));
}

std::string mangleFunction(const std::string& name, const std::vector<Type*>& paramTypes) {
    // The program entry point is the C `main` symbol even when it declares
    // argc/argv.  Without this, `main(int32, char**)` was mangled to
    // `main_int32_charptrptr` and the linker could not find an entry point.
    if (name == "main") return "main";
    if (isCName(name)) return name;
    std::string result = escapeMangledName(name);
    for (auto* type : paramTypes) {
        result += "_" + typeToMangledSignature(type);
    }
    return result;
}