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

// Simple global type singletons
inline Type* Type::Int() { static Type t{TypeKind::Int}; return &t; }
inline Type* Type::Char() { static Type t{TypeKind::Char}; return &t; }
inline Type* Type::Void() { static Type t{TypeKind::Void}; return &t; }
inline Type* Type::PointerTo(Type* elem) { static std::vector<Type> pool; pool.push_back(Type{TypeKind::Pointer}); pool.back().element = elem; return &pool.back(); }
inline Type* Type::ArrayOf(Type* elem, size_t len) { static std::vector<Type> pool; pool.push_back(Type{TypeKind::Array}); pool.back().element = elem; pool.back().arrayLength = len; return &pool.back(); }
inline Type* Type::FunctionOf(Type* ret, const std::vector<Type*>& params) { static std::vector<Type> pool; pool.push_back(Type{TypeKind::Function}); pool.back().element = ret; pool.back().params = params; return &pool.back(); }
inline Type* Type::StructNamed(const std::string& name, const std::vector<StructField>& fields) { static std::vector<Type> pool; pool.push_back(Type{TypeKind::Struct}); pool.back().structName = name; pool.back().fields = fields; return &pool.back(); }
