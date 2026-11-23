#include "ast/TypeUtils.h"

#include <sstream>

namespace {

void copyTypeMetadata(const Type* source, Type* target) {
    target->isVolatile = source->isVolatile;
    target->isConst = source->isConst;
    target->isAtomic = source->isAtomic;
    target->isRestrict = source->isRestrict;
    target->isReference = source->isReference;
    target->ctDeadBranch = source->ctDeadBranch;
    target->moduleName = source->moduleName;
    target->exported = source->exported;
    target->deprecated = source->deprecated;
    target->deprecatedMsg = source->deprecatedMsg;
    target->annotations = source->annotations;
    target->Type::base = source->Type::base;
}

std::string unqualifiedTypeName(const Type* type);

std::string joinTypes(const std::vector<Type*>& types) {
    std::ostringstream out;
    for (size_t i = 0; i < types.size(); ++i) {
        if (i) out << ", ";
        out << qualifiedTypeToString(types[i]);
    }
    return out.str();
}

std::string unqualifiedTypeName(const Type* type) {
    if (!type) return "<unknown>";
    switch (type->kind) {
        case TypeKind::Void: return "void";
        case TypeKind::Char: return "char";
        case TypeKind::Bool: return "bool";
        case TypeKind::Int8: return "int8";
        case TypeKind::Int16: return "int16";
        case TypeKind::Int32: return "int32";
        case TypeKind::Int64: return "int64";
        case TypeKind::Int128: return "int128";
        case TypeKind::UInt8: return "uint8";
        case TypeKind::UInt16: return "uint16";
        case TypeKind::UInt32: return "uint32";
        case TypeKind::UInt64: return "uint64";
        case TypeKind::UInt128: return "uint128";
        case TypeKind::ISize: return "isize";
        case TypeKind::USize: return "usize";
        case TypeKind::Float16: return "float16";
        case TypeKind::Float32: return "float32";
        case TypeKind::Float64: return "float64";
        case TypeKind::Float128: return "float128";
        case TypeKind::Str: return "string_view";
        case TypeKind::String: return "string";
        case TypeKind::Auto: return "auto";
        case TypeKind::Decltype: return "decltype(...)";
        case TypeKind::Struct:
            return "struct " + static_cast<const StructType*>(type)->name;
        case TypeKind::Class:
            return "class " + static_cast<const ClassType*>(type)->name;
        case TypeKind::Union:
            return "union " + static_cast<const UnionType*>(type)->name;
        case TypeKind::Enum:
            return "enum " + static_cast<const EnumType*>(type)->name;
        case TypeKind::Typedef:
            return static_cast<const TypedefType*>(type)->name;
        case TypeKind::TypeVar:
            return static_cast<const TypeVarType*>(type)->name;
        case TypeKind::Array: {
            const auto* array = static_cast<const ArrayType*>(type);
            std::string dim = array->sizeParam.empty()
                ? std::to_string(array->size)
                : array->sizeParam;
            return qualifiedTypeToString(array->elementType) + "[" + dim + "]";
        }
        case TypeKind::Slice:
            return qualifiedTypeToString(
                       static_cast<const SliceType*>(type)->elementType) + "[]";
        case TypeKind::Optional:
            return qualifiedTypeToString(
                       static_cast<const OptionalType*>(type)->elementType) + "?";
        case TypeKind::Result: {
            const auto* result = static_cast<const ResultType*>(type);
            return "result<" + qualifiedTypeToString(result->successType) + ", " +
                   qualifiedTypeToString(result->errorType) + ">";
        }
        case TypeKind::Function: {
            const auto* fn = static_cast<const FunctionType*>(type);
            std::string params = joinTypes(fn->paramTypes);
            if (fn->isVarArg) {
                if (!params.empty()) params += ", ";
                params += "...";
            }
            return qualifiedTypeToString(fn->returnType) + "(" + params + ")";
        }
        case TypeKind::TypeInstance: {
            const auto* inst = static_cast<const TypeInstanceType*>(type);
            std::string args = joinTypes(inst->typeArgs);
            for (long long value : inst->valueArgs) {
                if (!args.empty()) args += ", ";
                args += std::to_string(value);
            }
            return inst->templateName + "<" + args + ">";
        }
        case TypeKind::Pointer:
            // Handled by qualifiedTypeToString.
            break;
    }
    return "<unknown>";
}

} // namespace

Type* cloneType(const Type* type) {
    if (!type) return nullptr;

    Type* clone = nullptr;
    switch (type->kind) {
        case TypeKind::Pointer:
            clone = new Type(TypeKind::Pointer, type->base);
            break;
        case TypeKind::Array: {
            const auto* array = static_cast<const ArrayType*>(type);
            auto* copy = new ArrayType(array->elementType, array->size);
            copy->sizeParam = array->sizeParam;
            clone = copy;
            break;
        }
        case TypeKind::Struct: {
            const auto* source = static_cast<const StructType*>(type);
            auto* copy = new StructType(source->name);
            copy->baseClass = source->baseClass;
            copy->base = source->base;
            copy->isComplete = source->isComplete;
            copy->parsedDefinition = source->parsedDefinition;
            copy->isTemplatePattern = source->isTemplatePattern;
            copy->isPacked = source->isPacked;
            copy->reprC = source->reprC;
            copy->forcedAlign = source->forcedAlign;
            copy->fieldAligns = source->fieldAligns;
            copy->fieldDeprecated = source->fieldDeprecated;
            copy->fieldDeprecatedMsg = source->fieldDeprecatedMsg;
            copy->layoutMaterialized = source->layoutMaterialized;
            copy->fields = source->fields;
            copy->semanticFields = source->semanticFields;
            copy->bitFields = source->bitFields;
            copy->isClosure = source->isClosure;
            copy->closureCallName = source->closureCallName;
            copy->closureParamTypes = source->closureParamTypes;
            copy->closureReturnType = source->closureReturnType;
            copy->closureHasByRefCapture = source->closureHasByRefCapture;
            copy->closureCapturesThis = source->closureCapturesThis;
            clone = copy;
            break;
        }
        case TypeKind::Class: {
            const auto* source = static_cast<const ClassType*>(type);
            auto* copy = new ClassType(source->name);
            copy->fields = source->fields;
            copy->methods = source->methods;
            copy->baseClass = source->baseClass;
            copy->memberAccess = source->memberAccess;
            copy->isComplete = source->isComplete;
            copy->parsedDefinition = source->parsedDefinition;
            copy->isTemplatePattern = source->isTemplatePattern;
            copy->isPacked = source->isPacked;
            copy->reprC = source->reprC;
            copy->forcedAlign = source->forcedAlign;
            copy->fieldAligns = source->fieldAligns;
            copy->fieldDeprecated = source->fieldDeprecated;
            copy->fieldDeprecatedMsg = source->fieldDeprecatedMsg;
            copy->layoutMaterialized = source->layoutMaterialized;
            copy->semanticFields = source->semanticFields;
            copy->bitFields = source->bitFields;
            clone = copy;
            break;
        }
        case TypeKind::Union: {
            const auto* source = static_cast<const UnionType*>(type);
            auto* copy = new UnionType(source->name);
            copy->isPacked = source->isPacked;
            copy->reprC = source->reprC;
            copy->forcedAlign = source->forcedAlign;
            copy->members = source->members;
            copy->bitFields = source->bitFields;
            copy->isComplete = source->isComplete;
            copy->parsedDefinition = source->parsedDefinition;
            clone = copy;
            break;
        }
        case TypeKind::Enum: {
            const auto* source = static_cast<const EnumType*>(type);
            auto* copy = new EnumType(source->name);
            copy->values = source->values;
            copy->underlyingType = source->underlyingType;
            copy->isComplete = source->isComplete;
            clone = copy;
            break;
        }
        case TypeKind::Function: {
            const auto* source = static_cast<const FunctionType*>(type);
            clone = new FunctionType(source->returnType, source->paramTypes,
                                     source->isVarArg);
            break;
        }
        case TypeKind::Typedef: {
            const auto* source = static_cast<const TypedefType*>(type);
            clone = new TypedefType(source->name, source->aliasedType);
            break;
        }
        case TypeKind::Slice:
            clone = new SliceType(static_cast<const SliceType*>(type)->elementType);
            break;
        case TypeKind::Optional:
            clone = new OptionalType(
                static_cast<const OptionalType*>(type)->elementType);
            break;
        case TypeKind::Result: {
            const auto* source = static_cast<const ResultType*>(type);
            clone = new ResultType(source->successType, source->errorType);
            break;
        }
        case TypeKind::Str:
            clone = new StrType();
            break;
        case TypeKind::String:
            clone = new StringType();
            break;
        case TypeKind::TypeVar:
            clone = new TypeVarType(static_cast<const TypeVarType*>(type)->name);
            break;
        case TypeKind::Decltype:
            clone = new DecltypeType(
                static_cast<const DecltypeType*>(type)->expr);
            break;
        case TypeKind::TypeInstance: {
            const auto* source = static_cast<const TypeInstanceType*>(type);
            auto* copy = new TypeInstanceType(source->templateName);
            copy->typeArgs = source->typeArgs;
            copy->valueArgs = source->valueArgs;
            copy->valueArgNames = source->valueArgNames;
            copy->valueArgExprs = source->valueArgExprs;
            clone = copy;
            break;
        }
        default:
            clone = new Type(type->kind, type->base);
            break;
    }

    copyTypeMetadata(type, clone);
    return clone;
}

Type* qualifyType(const Type* type, TypeQualifiers qualifiers) {
    Type* clone = cloneType(type);
    if (!clone) return nullptr;
    if (qualifiers.isConst) clone->isConst = true;
    if (qualifiers.isVolatile) clone->isVolatile = true;
    if (qualifiers.isRestrict) clone->isRestrict = true;
    return clone;
}

Type* qualifyTopLevel(const Type* type, bool isConst, bool isVolatile) {
    if (!type) return nullptr;
    if (!isConst && !isVolatile) return const_cast<Type*>(type);
    return qualifyType(type, TypeQualifiers{isConst, isVolatile, false});
}

Type* adjustParameterTypeForSignature(const Type* type) {
    if (!type) return nullptr;
    if (!type->isConst && !type->isVolatile && !type->isRestrict)
        return const_cast<Type*>(type);
    Type* clone = cloneType(type);
    clone->isConst = false;
    clone->isVolatile = false;
    clone->isRestrict = false;
    return clone;
}

namespace {

const Type* stripTypedefsForQualifiers(const Type* type) {
    while (type && type->kind == TypeKind::Typedef)
        type = static_cast<const TypedefType*>(type)->aliasedType;
    return type;
}

bool qualifiersMayBeAdded(const Type* target, const Type* source) {
    if (source->isConst && !target->isConst) return false;
    if (source->isVolatile && !target->isVolatile) return false;
    return true;
}

const Type* stripTopLevelQualifiers(const Type* type) {
    if (!type->isConst && !type->isVolatile && !type->isRestrict)
        return type;
    Type* clone = cloneType(type);
    clone->isConst = false;
    clone->isVolatile = false;
    clone->isRestrict = false;
    return clone;
}

bool isRecordType(const Type* type) {
    return type && (type->kind == TypeKind::Struct ||
                    type->kind == TypeKind::Class);
}

const Type* recordParent(const Type* type) {
    if (!type) return nullptr;
    if (type->kind == TypeKind::Struct) {
        const auto* record = static_cast<const StructType*>(type);
        if (record->base) return record->base;
        if (record->baseClass.empty()) return nullptr;
        if (auto* parent = TypeContext::instance().getStruct(record->baseClass))
            return parent;
        if (auto* parent = TypeContext::instance().getClass(record->baseClass))
            return parent;
        return nullptr;
    }
    if (type->kind == TypeKind::Class) {
        if (type->base) return type->base;
        const auto* record = static_cast<const ClassType*>(type);
        if (record->baseClass.empty()) return nullptr;
        if (auto* parent = TypeContext::instance().getClass(record->baseClass))
            return parent;
        if (auto* parent = TypeContext::instance().getStruct(record->baseClass))
            return parent;
        return nullptr;
    }
    return nullptr;
}

bool isDerivedRecord(const Type* derived, const Type* base) {
    derived = stripTypedefsForQualifiers(derived);
    base = stripTypedefsForQualifiers(base);
    if (!isRecordType(derived) || !isRecordType(base)) return false;
    for (int depth = 0; derived && depth < 64; ++depth) {
        const std::string* derivedName = nullptr;
        const std::string* baseName = nullptr;
        if (derived->kind == TypeKind::Struct)
            derivedName = &static_cast<const StructType*>(derived)->name;
        else if (derived->kind == TypeKind::Class)
            derivedName = &static_cast<const ClassType*>(derived)->name;
        if (base->kind == TypeKind::Struct)
            baseName = &static_cast<const StructType*>(base)->name;
        else if (base->kind == TypeKind::Class)
            baseName = &static_cast<const ClassType*>(base)->name;
        if (derivedName && baseName && *derivedName == *baseName) return true;
        derived = recordParent(derived);
    }
    return false;
}

} // namespace

bool pointerConversionAllowed(const Type* target, const Type* source) {
    target = stripTypedefsForQualifiers(target);
    source = stripTypedefsForQualifiers(source);
    if (!target || !source || target->kind != TypeKind::Pointer) return false;

    const Type* targetBase = stripTypedefsForQualifiers(target->base);
    const Type* sourceBase = nullptr;
    if (source->kind == TypeKind::Pointer) {
        sourceBase = stripTypedefsForQualifiers(source->base);
    } else if (source->kind == TypeKind::Array) {
        // Array-to-pointer decay preserves the element qualifiers.
        sourceBase = stripTypedefsForQualifiers(
            static_cast<const ArrayType*>(source)->elementType);
    } else {
        return false;
    }
    if (!targetBase || !sourceBase) return false;

    const bool targetFunction = targetBase->kind == TypeKind::Function;
    const bool sourceFunction = sourceBase->kind == TypeKind::Function;
    if (targetFunction || sourceFunction) {
        // Function pointers do not convert to/from object pointers.
        return targetFunction && sourceFunction &&
               typesIdentical(targetBase, sourceBase);
    }

    if (targetBase->kind == TypeKind::Void ||
        sourceBase->kind == TypeKind::Void) {
        return qualifiersMayBeAdded(targetBase, sourceBase);
    }

    if (targetBase->kind != sourceBase->kind) {
        return qualifiersMayBeAdded(targetBase, sourceBase) &&
               isDerivedRecord(sourceBase, targetBase);
    }
    if (!qualifiersMayBeAdded(targetBase, sourceBase)) return false;

    // Only the immediate pointee level may gain qualifiers.  Anything deeper
    // must already be identical (`int32** -> const int32**` is unsafe).
    if (typesIdentical(stripTopLevelQualifiers(targetBase),
                       stripTopLevelQualifiers(sourceBase))) {
        return true;
    }
    // Single-inheritance derived-to-base pointer conversion is safe only at
    // this immediate level; pointer-to-pointer covariance remains rejected.
    return isDerivedRecord(sourceBase, targetBase);
}

Type* qualifyMemberLValue(const Type* memberType, bool objectIsConst,
                          bool objectIsVolatile) {
    if (!memberType || (!objectIsConst && !objectIsVolatile))
        return const_cast<Type*>(memberType);
    const Type* stripped = stripTypedefsForQualifiers(memberType);
    if (!stripped) return const_cast<Type*>(memberType);
    // References are aliases; the containing object's qualifiers do not
    // propagate to the referent.  The reference itself has no qualifiers.
    if (stripped->kind == TypeKind::Pointer && stripped->isReference)
        return const_cast<Type*>(memberType);

    TypeQualifiers qualifiers{objectIsConst, objectIsVolatile, false};
    if (stripped->kind == TypeKind::Array) {
        // A const array has const elements; recurse through nested arrays.
        const auto* array = static_cast<const ArrayType*>(stripped);
        Type* clone = cloneType(array);
        auto* arrayClone = static_cast<ArrayType*>(clone);
        Type* element = const_cast<Type*>(array->elementType);
        if (objectIsConst || objectIsVolatile) {
            const Type* elemStripped = stripTypedefsForQualifiers(element);
            if (elemStripped && elemStripped->kind == TypeKind::Array) {
                element = qualifyMemberLValue(elemStripped, objectIsConst,
                                              objectIsVolatile);
            } else {
                element = qualifyType(elemStripped, qualifiers);
            }
        }
        arrayClone->elementType = element;
        return arrayClone;
    }
    return qualifyType(stripped, qualifiers);
}

namespace {

bool hasConstSubobject(const Type* type, int depth) {
    if (!type || depth > 64) return false;
    type = stripTypedefsForQualifiers(type);
    if (!type) return false;
    if (type->isConst) return true;
    switch (type->kind) {
        case TypeKind::Array:
            return hasConstSubobject(
                static_cast<const ArrayType*>(type)->elementType, depth + 1);
        case TypeKind::Struct: {
            const auto* record = static_cast<const StructType*>(type);
            const auto& fields = !record->semanticFields.empty()
                                     ? record->semanticFields : record->fields;
            for (const auto& field : fields) {
                if (hasConstSubobject(field.type, depth + 1)) return true;
            }
            return record->base && hasConstSubobject(record->base, depth + 1);
        }
        case TypeKind::Class: {
            const auto* record = static_cast<const ClassType*>(type);
            const auto& fields = !record->semanticFields.empty()
                                     ? record->semanticFields : record->fields;
            for (const auto& field : fields) {
                if (hasConstSubobject(field.type, depth + 1)) return true;
            }
            return type->base && hasConstSubobject(type->base, depth + 1);
        }
        case TypeKind::Union: {
            for (const auto& member : static_cast<const UnionType*>(type)->members) {
                if (hasConstSubobject(member.type, depth + 1)) return true;
            }
            return false;
        }
        case TypeKind::Optional:
            return hasConstSubobject(
                static_cast<const OptionalType*>(type)->elementType, depth + 1);
        case TypeKind::Result: {
            const auto* result = static_cast<const ResultType*>(type);
            return hasConstSubobject(result->successType, depth + 1) ||
                   hasConstSubobject(result->errorType, depth + 1);
        }
        default:
            return false;
    }
}

} // namespace

bool isModifiableLValueType(const Type* type, std::string* reason) {
    if (!type) {
        if (reason) *reason = "unknown type";
        return false;
    }
    type = stripTypedefsForQualifiers(type);
    if (!type) {
        if (reason) *reason = "unknown type";
        return false;
    }
    if (type->kind == TypeKind::Function) {
        if (reason) *reason = "function type";
        return false;
    }
    if (hasConstSubobject(type, 0)) {
        if (reason) *reason = "const";
        return false;
    }
    return true;
}

bool typesIdentical(const Type* a, const Type* b) {
    if (!a || !b) return a == b;
    if (a->kind != b->kind) return false;
    if (a->isConst != b->isConst || a->isVolatile != b->isVolatile ||
        a->isAtomic != b->isAtomic || a->isReference != b->isReference) {
        return false;
    }

    switch (a->kind) {
        case TypeKind::Pointer:
            return typesIdentical(a->base, b->base);
        case TypeKind::Array: {
            const auto* aa = static_cast<const ArrayType*>(a);
            const auto* ab = static_cast<const ArrayType*>(b);
            return aa->size == ab->size && aa->sizeParam == ab->sizeParam &&
                   typesIdentical(aa->elementType, ab->elementType);
        }
        case TypeKind::Struct:
            return static_cast<const StructType*>(a)->name ==
                   static_cast<const StructType*>(b)->name;
        case TypeKind::Class:
            return static_cast<const ClassType*>(a)->name ==
                   static_cast<const ClassType*>(b)->name;
        case TypeKind::Union:
            return static_cast<const UnionType*>(a)->name ==
                   static_cast<const UnionType*>(b)->name;
        case TypeKind::Enum:
            return static_cast<const EnumType*>(a)->name ==
                   static_cast<const EnumType*>(b)->name;
        case TypeKind::Typedef:
            return typesIdentical(static_cast<const TypedefType*>(a)->aliasedType,
                                  static_cast<const TypedefType*>(b)->aliasedType);
        case TypeKind::Slice:
            return typesIdentical(static_cast<const SliceType*>(a)->elementType,
                                  static_cast<const SliceType*>(b)->elementType);
        case TypeKind::Optional:
            return typesIdentical(static_cast<const OptionalType*>(a)->elementType,
                                  static_cast<const OptionalType*>(b)->elementType);
        case TypeKind::Result: {
            const auto* ra = static_cast<const ResultType*>(a);
            const auto* rb = static_cast<const ResultType*>(b);
            return typesIdentical(ra->successType, rb->successType) &&
                   typesIdentical(ra->errorType, rb->errorType);
        }
        case TypeKind::Function: {
            const auto* fa = static_cast<const FunctionType*>(a);
            const auto* fb = static_cast<const FunctionType*>(b);
            if (fa->isVarArg != fb->isVarArg) return false;
            if (!typesIdentical(fa->returnType, fb->returnType)) return false;
            if (fa->paramTypes.size() != fb->paramTypes.size()) return false;
            for (size_t i = 0; i < fa->paramTypes.size(); ++i) {
                if (!typesIdentical(fa->paramTypes[i], fb->paramTypes[i]))
                    return false;
            }
            return true;
        }
        case TypeKind::TypeVar:
            return static_cast<const TypeVarType*>(a)->name ==
                   static_cast<const TypeVarType*>(b)->name;
        case TypeKind::TypeInstance: {
            const auto* ia = static_cast<const TypeInstanceType*>(a);
            const auto* ib = static_cast<const TypeInstanceType*>(b);
            if (ia->templateName != ib->templateName) return false;
            if (ia->typeArgs.size() != ib->typeArgs.size()) return false;
            for (size_t i = 0; i < ia->typeArgs.size(); ++i) {
                if (!typesIdentical(ia->typeArgs[i], ib->typeArgs[i]))
                    return false;
            }
            return ia->valueArgs == ib->valueArgs &&
                   ia->valueArgNames == ib->valueArgNames;
        }
        case TypeKind::Decltype:
            return a == b;
        default:
            return true;
    }
}

std::string qualifiedTypeToString(const Type* type) {
    if (!type) return "<unknown>";

    if (type->kind == TypeKind::Pointer) {
        std::string result = qualifiedTypeToString(type->base);
        result += type->isReference ? "&" : "*";
        if (type->isConst) result += " const";
        if (type->isVolatile) result += " volatile";
        if (type->isRestrict) result += " restrict";
        return result;
    }

    std::string result = unqualifiedTypeName(type);
    if (type->isConst) result = "const " + result;
    if (type->isVolatile) result = "volatile " + result;
    if (type->isRestrict) result = "restrict " + result;
    return result;
}
