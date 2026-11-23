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
        case TypeKind::Array:
            return signatureTypeToMangled(
                       static_cast<const ArrayType*>(type)->elementType) + "arr";
        case TypeKind::Struct:
            return qualifierPrefix(type) +
                   static_cast<const StructType*>(type)->name;
        case TypeKind::Class:
            return qualifierPrefix(type) +
                   static_cast<const ClassType*>(type)->name;
        case TypeKind::Union:
            return qualifierPrefix(type) +
                   static_cast<const UnionType*>(type)->name;
        case TypeKind::Enum: {
            const auto* e = static_cast<const EnumType*>(type);
            return qualifierPrefix(type) + (e->name.empty() ? "int32" : e->name);
        }
        case TypeKind::Void: return qualifierPrefix(type) + "void";
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
            return qualifierPrefix(type) + signatureTypeToMangled(r->successType) +
                   "res" + signatureTypeToMangled(r->errorType);
        }
        case TypeKind::Typedef:
            return qualifierPrefix(type) +
                   signatureTypeToMangled(
                       static_cast<const TypedefType*>(type)->aliasedType);
        case TypeKind::TypeVar:
            return qualifierPrefix(type) +
                   static_cast<const TypeVarType*>(type)->name;
        case TypeKind::TypeInstance: {
            const auto* ti = static_cast<const TypeInstanceType*>(type);
            std::string result = qualifierPrefix(type) + ti->templateName;
            for (const auto* arg : ti->typeArgs)
                result += "$" + signatureTypeToMangled(arg);
            for (long long value : ti->valueArgs)
                result += "$" + std::to_string(value);
            return result;
        }
        case TypeKind::Function: {
            const auto* fn = static_cast<const FunctionType*>(type);
            std::string result = qualifierPrefix(type) +
                                 signatureTypeToMangled(
                                     adjustParameterTypeForSignature(fn->returnType)) +
                                 "fn";
            for (const auto* param : fn->paramTypes)
                result += "_" + signatureTypeToMangled(
                                     adjustParameterTypeForSignature(param));
            if (fn->isVarArg) result += "_vararg";
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
    if (paramTypes.empty()) return name;
    std::string result = name;
    for (auto* type : paramTypes) {
        result += "_" + typeToMangledSignature(type);
    }
    return result;
}