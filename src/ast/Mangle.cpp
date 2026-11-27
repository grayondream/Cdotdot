#include "Mangle.h"
#include "ast/Type.h"
#include "ast/TypeUtils.h"
#include <unordered_set>

namespace {
std::string mangleFunctionCpp(const std::string& name,
                              const std::vector<Type*>& paramTypes,
                              bool isVarArg, const std::string& moduleName = {});
}

static std::unordered_set<std::string>& cNames() {
    static std::unordered_set<std::string> names;
    return names;
}

// C-linkage signatures. C linkage applies to a declaration/signature, not to
// every CDD overload that happens to reuse the same source name.
static std::unordered_set<std::string>& cSignatures() {
    static std::unordered_set<std::string> signatures;
    return signatures;
}

// C-linkage externs declared *inside a namespace body* (e.g. `std::sqrt`).
// Only these participate in the `ns::f` → bare-`f` resolution fallback, so
// a global-scope extern (prelude `malloc`, `strlen`, ...) is NOT reachable
// as `std::malloc` / `std::strlen` (F2 — namespace isolation preserved).
static std::unordered_set<std::string>& namespaceCNames() {
    static std::unordered_set<std::string> names;
    return names;
}

void markCName(const std::string& name, const std::vector<Type*>& paramTypes,
               bool isVarArg) {
    cNames().insert(name);
    cSignatures().insert(mangleFunctionCpp(name, paramTypes, isVarArg));
}

void markNamespaceCName(const std::string& name,
                        const std::vector<Type*>& paramTypes, bool isVarArg) {
    markCName(name, paramTypes, isVarArg);
    namespaceCNames().insert(name);
}

bool isCName(const std::string& name) {
    return cNames().count(name) != 0;
}

bool isNamespaceCName(const std::string& name) {
    return namespaceCNames().count(name) != 0;
}

static bool isCNameSignature(const std::string& name,
                             const std::vector<Type*>& paramTypes,
                             bool isVarArg) {
    return cSignatures().count(mangleFunctionCpp(name, paramTypes, isVarArg)) != 0;
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

namespace {
struct MangleQualifiers {
    bool isConst = false;
    bool isVolatile = false;
    bool isAtomic = false;
};
std::string signatureTypeToMangled(const Type* type,
                                   const MangleQualifiers& extra = {});
} // namespace

std::string typeToMangled(Type* type) {
    // Template arguments and conversion-operator names must preserve cv at
    // every level, just like signature mangling (but without the by-value
    // top-level adjustment performed by typeToMangledSignature).
    return signatureTypeToMangled(type);
}


namespace {

MangleQualifiers effectiveQualifiers(const Type* type,
                                     const MangleQualifiers& extra) {
    return MangleQualifiers{
        type->isConst || extra.isConst,
        type->isVolatile || extra.isVolatile,
        type->isAtomic || extra.isAtomic,
    };
}

std::string qualifierPrefix(const MangleQualifiers& q) {
    std::string result;
    if (q.isConst) result += "C";
    if (q.isVolatile) result += "V";
    if (q.isAtomic) result += "atomic";
    return result;
}

// Composite constructors put their own qualifiers *after* the element payload.
// A prefix would be indistinguishable from qualifiers already present on the
// element: `slice<const int32>` and `const slice<int32>` both start with `C`.
std::string qualifierSuffix(const MangleQualifiers& q) {
    return qualifierPrefix(q);
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
std::string signatureTypeToMangled(const Type* type,
                                   const MangleQualifiers& extra) {
    if (!type) return "unknown";
    const MangleQualifiers q = effectiveQualifiers(type, extra);
    switch (type->kind) {
        case TypeKind::Pointer: {
            std::string result = signatureTypeToMangled(type->base);
            result += type->isReference ? "ref" : "ptr";
            result += qualifierSuffix(q);
            return result;
        }
        case TypeKind::Array: {
            const auto* arr = static_cast<const ArrayType*>(type);
            std::string result =
                signatureTypeToMangled(arr->elementType) + "arr";
            if (!arr->sizeParam.empty())
                result += "p" + lengthPrefixedName(arr->sizeParam);
            result += "s" + std::to_string(arr->size);
            result += qualifierSuffix(q);
            return result;
        }
        case TypeKind::Struct:
            return qualifierPrefix(q) + "N" +
                   lengthPrefixedName(static_cast<const StructType*>(type)->name);
        case TypeKind::Class:
            return qualifierPrefix(q) + "N" +
                   lengthPrefixedName(static_cast<const ClassType*>(type)->name);
        case TypeKind::Union:
            return qualifierPrefix(q) + "N" +
                   lengthPrefixedName(static_cast<const UnionType*>(type)->name);
        case TypeKind::Enum: {
            const auto* e = static_cast<const EnumType*>(type);
            return qualifierPrefix(q) +
                   (e->name.empty() ? "int32"
                                    : "N" + lengthPrefixedName(e->name));
        }
        case TypeKind::Void: return qualifierPrefix(q) + "void";
        case TypeKind::Nullptr: return qualifierPrefix(q) + "nullptr";
        case TypeKind::Char: return qualifierPrefix(q) + "char";
        case TypeKind::Bool: return qualifierPrefix(q) + "bool";
        case TypeKind::Int8: return qualifierPrefix(q) + "int8";
        case TypeKind::Int16: return qualifierPrefix(q) + "int16";
        case TypeKind::Int32: return qualifierPrefix(q) + "int32";
        case TypeKind::Int64: return qualifierPrefix(q) + "int64";
        case TypeKind::Int128: return qualifierPrefix(q) + "int128";
        case TypeKind::UInt8: return qualifierPrefix(q) + "uint8";
        case TypeKind::UInt16: return qualifierPrefix(q) + "uint16";
        case TypeKind::UInt32: return qualifierPrefix(q) + "uint32";
        case TypeKind::UInt64: return qualifierPrefix(q) + "uint64";
        case TypeKind::UInt128: return qualifierPrefix(q) + "uint128";
        case TypeKind::ISize: return qualifierPrefix(q) + "isize";
        case TypeKind::USize: return qualifierPrefix(q) + "usize";
        case TypeKind::Float16: return qualifierPrefix(q) + "float16";
        case TypeKind::Float32: return qualifierPrefix(q) + "float32";
        case TypeKind::Float64: return qualifierPrefix(q) + "float64";
        case TypeKind::Float128: return qualifierPrefix(q) + "float128";
        case TypeKind::Str: return qualifierPrefix(q) + "str";
        case TypeKind::String: return qualifierPrefix(q) + "string";
        case TypeKind::Slice:
            return signatureTypeToMangled(
                       static_cast<const SliceType*>(type)->elementType) +
                   "slice" + qualifierSuffix(q);
        case TypeKind::Optional:
            return signatureTypeToMangled(
                       static_cast<const OptionalType*>(type)->elementType) +
                   "opt" + qualifierSuffix(q);
        case TypeKind::Result: {
            const auto* r = static_cast<const ResultType*>(type);
            // The error payload is length-prefixed so an outer constructor
            // (e.g. `result<A,B>*`) cannot look like `result<A,B*>`.
            return signatureTypeToMangled(r->successType) + "res" +
                   lengthPrefixedName(signatureTypeToMangled(r->errorType)) +
                   qualifierSuffix(q);
        }
        case TypeKind::Typedef:
            // Aliases are transparent: merge this level's cv into the alias's
            // top level instead of prefixing, so `const S` and
            // `const slice<int32>` (where `using S = int32[]`) encode alike.
            return signatureTypeToMangled(
                static_cast<const TypedefType*>(type)->aliasedType, q);
        case TypeKind::TypeVar:
            return qualifierPrefix(q) + "X" +
                   lengthPrefixedName(static_cast<const TypeVarType*>(type)->name);
        case TypeKind::TypeInstance: {
            const auto* ti = static_cast<const TypeInstanceType*>(type);
            std::string result =
                qualifierPrefix(q) + "I" + lengthPrefixedName(ti->templateName);
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
            std::string result =
                signatureTypeToMangled(
                    adjustParameterTypeForSignature(fn->returnType)) +
                "fn" + std::to_string(fn->paramTypes.size()) + ":";
            for (const auto* param : fn->paramTypes)
                result += signatureTypeToMangled(
                    adjustParameterTypeForSignature(param));
            result += fn->isVarArg ? "v" : "n";
            result += qualifierSuffix(q);
            return result;
        }
        default:
            return qualifierPrefix(q) + "unknown";
    }
}

} // namespace

std::string typeToMangledSignature(Type* type) {
    return signatureTypeToMangled(adjustParameterTypeForSignature(type));
}

namespace {
const std::string kFunctionPrefix = "cdd$f$";
const std::string kModuleFunctionPrefix = "cdd$fm";
const std::string kVariablePrefix = "cdd$v";

std::string mangleFunctionCpp(const std::string& name,
                              const std::vector<Type*>& paramTypes,
                              bool isVarArg, const std::string& moduleName) {
    // `$` cannot appear in a CDD source identifier. Reserving it for CDD
    // function symbols keeps them disjoint from global variables, C symbols,
    // and identifier-like spellings such as `f_int32`. Participating modules
    // add their module name so module-private same-name functions do not
    // collide in the shared object-file symbol space.
    std::string result = kFunctionPrefix;
    if (!moduleName.empty())
        result = kModuleFunctionPrefix + lengthPrefixedName(moduleName) + "$";
    result += escapeMangledName(name);
    for (auto* type : paramTypes) {
        result += "_" + typeToMangledSignature(type);
    }
    // `f(int32)` and `f(int32, ...)` are distinct function types. The marker
    // starts with `_`, which no type encoding starts with, so it cannot be
    // confused with the final parameter.
    if (isVarArg) result += "_vararg";
    return result;
}
} // namespace

namespace {
bool hasModuleQualifier(const std::string& symbol) {
    if (symbol.rfind(kModuleFunctionPrefix, 0) != 0) return false;
    size_t pos = kModuleFunctionPrefix.size();
    const size_t digits = pos;
    while (pos < symbol.size() && symbol[pos] >= '0' && symbol[pos] <= '9') ++pos;
    return pos > digits && pos < symbol.size() && symbol[pos] == ':';
}
} // namespace

std::string qualifyFunctionSymbolForModule(const std::string& symbol,
                                           const std::string& moduleName) {
    if (moduleName.empty() || symbol.rfind(kFunctionPrefix, 0) != 0 ||
        hasModuleQualifier(symbol)) {
        return symbol;
    }
    return kModuleFunctionPrefix + lengthPrefixedName(moduleName) + "$" +
           symbol.substr(kFunctionPrefix.size());
}

std::string resolveFunctionSymbol(const std::string& preferredSymbol,
                                  const std::string& name,
                                  const std::vector<Type*>& paramTypes,
                                  bool isVarArg,
                                  const std::string& moduleName) {
    if (!preferredSymbol.empty())
        return qualifyFunctionSymbolForModule(preferredSymbol, moduleName);
    return mangleFunction(name, paramTypes, isVarArg, moduleName);
}

std::string mangleGlobalVariable(const std::string& name,
                                 const std::string& moduleName) {
    const std::string moduleMarker =
        moduleName.empty() ? "$" : "m" + lengthPrefixedName(moduleName) + "$";
    return kVariablePrefix + moduleMarker + name;
}

std::string mangleFunction(const std::string& name,
                           const std::vector<Type*>& paramTypes,
                           bool isVarArg, const std::string& moduleName) {
    // The program entry point is the C `main` symbol even when it declares
    // argc/argv.  Without this, `main(int32, char**)` was mangled to
    // `main_int32_charptrptr` and the linker could not find an entry point.
    if (name == "main") return "main";
    if (isCNameSignature(name, paramTypes, isVarArg)) return name;
    return mangleFunctionCpp(name, paramTypes, isVarArg, moduleName);
}

std::string mangleCddFunction(const std::string& name,
                              const std::vector<Type*>& paramTypes,
                              bool isVarArg, const std::string& moduleName) {
    if (name == "main") return "main";
    return mangleFunctionCpp(name, paramTypes, isVarArg, moduleName);
}
