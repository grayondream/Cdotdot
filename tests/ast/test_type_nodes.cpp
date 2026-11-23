#include <gtest/gtest.h>
#include "ast/Mangle.h"
#include "ast/Type.h"

class TypeNodeTest : public ::testing::Test {
protected:
    void SetUp() override {
        typeCtx = &TypeContext::instance();
    }

    TypeContext* typeCtx;
};

TEST_F(TypeNodeTest, ArrayType) {
    auto arrType = std::make_unique<ArrayType>(typeCtx->getInt32(), 10);

    EXPECT_EQ(arrType->kind, TypeKind::Array);
    EXPECT_EQ(arrType->elementType->kind, TypeKind::Int32);
    EXPECT_EQ(arrType->size, 10);
}

TEST_F(TypeNodeTest, StructType) {
    auto structType = std::make_unique<StructType>("Point");
    structType->addField("x", typeCtx->getInt32());
    structType->addField("y", typeCtx->getInt32());

    EXPECT_EQ(structType->kind, TypeKind::Struct);
    EXPECT_EQ(structType->name, "Point");
    EXPECT_EQ(structType->fields.size(), 2);
    EXPECT_EQ(structType->fields[0].name, "x");
    EXPECT_EQ(structType->fields[1].name, "y");
}

TEST_F(TypeNodeTest, UnionType) {
    auto unionType = std::make_unique<UnionType>("Data");
    unionType->addMember("i", typeCtx->getInt32());
    unionType->addMember("f", typeCtx->getFloat32());

    EXPECT_EQ(unionType->kind, TypeKind::Union);
    EXPECT_EQ(unionType->name, "Data");
    EXPECT_EQ(unionType->members.size(), 2);
}

TEST_F(TypeNodeTest, EnumType) {
    auto enumType = std::make_unique<EnumType>("Color");
    enumType->addValue("RED", 0);
    enumType->addValue("GREEN", 1);
    enumType->addValue("BLUE", 2);

    EXPECT_EQ(enumType->kind, TypeKind::Enum);
    EXPECT_EQ(enumType->name, "Color");
    EXPECT_EQ(enumType->values.size(), 3);
    EXPECT_EQ(enumType->values[0].second, 0);
    EXPECT_EQ(enumType->values[1].second, 1);
    EXPECT_EQ(enumType->values[2].second, 2);
}

TEST_F(TypeNodeTest, FunctionType) {
    std::vector<Type*> paramTypes = {typeCtx->getInt32(), typeCtx->getFloat32()};
    auto funcType = std::make_unique<FunctionType>(typeCtx->getVoid(), std::move(paramTypes));

    EXPECT_EQ(funcType->kind, TypeKind::Function);
    EXPECT_EQ(funcType->returnType->kind, TypeKind::Void);
    EXPECT_EQ(funcType->paramTypes.size(), 2);
    EXPECT_FALSE(funcType->isVarArg);
}

TEST_F(TypeNodeTest, FunctionTypeVarArg) {
    std::vector<Type*> paramTypes = {typeCtx->getInt32()};
    auto funcType = std::make_unique<FunctionType>(
        typeCtx->getVoid(), std::move(paramTypes), true);

    EXPECT_TRUE(funcType->isVarArg);
}

TEST_F(TypeNodeTest, TypedefType) {
    auto typedefType = std::make_unique<TypedefType>("MyInt", typeCtx->getInt32());

    EXPECT_EQ(typedefType->kind, TypeKind::Typedef);
    EXPECT_EQ(typedefType->name, "MyInt");
    EXPECT_EQ(typedefType->aliasedType->kind, TypeKind::Int32);
}

TEST_F(TypeNodeTest, TypeConst) {
    auto type = new Type(TypeKind::Int32);
    type->isConst = true;

    EXPECT_TRUE(type->isConst);
    EXPECT_FALSE(type->isVolatile);
    delete type;
}

TEST_F(TypeNodeTest, TypeVolatile) {
    auto type = new Type(TypeKind::Int32);
    type->isVolatile = true;

    EXPECT_FALSE(type->isConst);
    EXPECT_TRUE(type->isVolatile);
    delete type;
}

TEST_F(TypeNodeTest, ManglesStringViewAndOwningString) {
    EXPECT_EQ(typeToMangled(typeCtx->getStrType()), "str");
    EXPECT_EQ(typeToMangled(typeCtx->getStringType()), "string");
    Type* ptr = new Type(TypeKind::Pointer, typeCtx->getStrType());
    EXPECT_EQ(typeToMangled(ptr), "strptr");
}

TEST_F(TypeNodeTest, QualifiedNameSeparatorIsNotAnIdentifierCharacter) {
    EXPECT_EQ(mangleQualifiedName("std::Vec"), "std.Vec");
    EXPECT_EQ(mangleQualifiedName("std.Vec"), "std.Vec");
    EXPECT_NE(mangleQualifiedName("ns::f"), mangleQualifiedName("ns_f"));
}

TEST_F(TypeNodeTest, MangledTypeEncodingIsInjectiveForKnownCollisions) {
    Type* int32Ptr = new Type(TypeKind::Pointer, typeCtx->getInt32());
    StructType int32ptrRecord("int32ptr");
    EXPECT_NE(typeToMangled(int32Ptr), typeToMangled(&int32ptrRecord));

    ArrayType two(typeCtx->getInt32(), 2);
    ArrayType three(typeCtx->getInt32(), 3);
    EXPECT_NE(typeToMangled(&two), typeToMangled(&three));

    delete int32Ptr;
}

TEST_F(TypeNodeTest, MangledTypeEncodingSeparatesCompositeBoundaries) {
    StructType A("A");
    Type* ptrA = new Type(TypeKind::Pointer, &A);
    ArrayType arrA(&A, 2);

    TypeInstanceType box("Box");
    box.typeArgs.push_back(&A);
    TypeInstanceType boxPtr("Box");
    boxPtr.typeArgs.push_back(ptrA);
    Type* ptrBox = new Type(TypeKind::Pointer, &box);
    EXPECT_NE(typeToMangled(&boxPtr), typeToMangled(ptrBox));

    TypeInstanceType boxArr("Box");
    boxArr.typeArgs.push_back(&arrA);
    ArrayType arrBox(&box, 2);
    EXPECT_NE(typeToMangled(&boxArr), typeToMangled(&arrBox));

    StructType B("B");
    Type* ptrB = new Type(TypeKind::Pointer, &B);
    ResultType resultAB(&A, &B);
    Type* ptrResult = new Type(TypeKind::Pointer, &resultAB);
    ResultType resultABPtr(&A, ptrB);
    EXPECT_NE(typeToMangled(ptrResult), typeToMangled(&resultABPtr));

    delete ptrA;
    delete ptrB;
    delete ptrBox;
    delete ptrResult;
}

TEST_F(TypeNodeTest, MangledFunctionNamesAreInjectiveForKnownCollisions) {
    Type* int32Ptr = new Type(TypeKind::Pointer, typeCtx->getInt32());
    StructType int32ptrRecord("int32ptr");
    StructType A("A");
    StructType B("B");

    // A one-parameter overload must not collide with a flat function name
    // that already looks like a mangled spelling.
    EXPECT_NE(mangleFunction("f", {int32Ptr}),
              mangleFunction("f_int32ptr", {}));
    EXPECT_NE(mangleFunction("f", {typeCtx->getInt32()}),
              mangleFunction("f_int32", {}));
    // `A_B` in a parameter list is two parameters, not one type name.
    EXPECT_NE(mangleFunction("f", {&A, &B}),
              mangleFunction("f_A", {&B}));

    delete int32Ptr;
}

TEST_F(TypeNodeTest, PointerType) {
    auto ptrType = new Type(TypeKind::Pointer, typeCtx->getInt32());

    EXPECT_EQ(ptrType->kind, TypeKind::Pointer);
    EXPECT_EQ(ptrType->base->kind, TypeKind::Int32);

    delete ptrType;
}

TEST_F(TypeNodeTest, ClassTypeCreation) {
    auto* classType = typeCtx->getOrCreateClass("Foo");
    EXPECT_NE(classType, nullptr);
    EXPECT_EQ(classType->kind, TypeKind::Class);
    EXPECT_EQ(classType->name, "Foo");
    EXPECT_EQ(classType->fields.size(), 0);
}

TEST_F(TypeNodeTest, ClassTypeWithFields) {
    auto classType = std::make_unique<ClassType>("Person");
    classType->addField("name", typeCtx->getInt32());
    classType->addField("age", typeCtx->getInt32());

    EXPECT_EQ(classType->kind, TypeKind::Class);
    EXPECT_EQ(classType->name, "Person");
    EXPECT_EQ(classType->fields.size(), 2);
    EXPECT_EQ(classType->fields[0].name, "name");
    EXPECT_EQ(classType->fields[1].name, "age");
}

TEST_F(TypeNodeTest, ClassTypeGetMethod) {
    auto* classType = typeCtx->getOrCreateClass("Bar");
    EXPECT_EQ(typeCtx->getClass("Bar"), classType);
    EXPECT_EQ(typeCtx->getClass("NonExistent"), nullptr);
}
