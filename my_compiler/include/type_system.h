#pragma once
#include <string>
#include <vector>

enum class TypeKind {
    Int,
    Char,
    Void,
    Pointer,
    Array,
    Struct,
    Function
};

struct StructField { std::string name; size_t index = 0; struct Type* type = nullptr; };

struct Type {
    TypeKind kind;
    Type* element = nullptr;        // for Pointer/Array: element type; for Function: return type
    std::vector<Type*> params;       // for Function: parameter types
    size_t arrayLength = 0;          // for Array
    std::string structName;          // for Struct
    std::vector<StructField> fields; // for Struct

    static Type* Int();
    static Type* Char();
    static Type* Void();
    static Type* PointerTo(Type* elem);
    static Type* ArrayOf(Type* elem, size_t len);
    static Type* FunctionOf(Type* ret, const std::vector<Type*>& params);
    static Type* StructNamed(const std::string& name, const std::vector<StructField>& fields);
};
