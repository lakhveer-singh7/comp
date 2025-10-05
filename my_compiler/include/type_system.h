#pragma once
#include <string>

enum class TypeKind { Int, Char, Void, Pointer };

struct Type {
    TypeKind kind;
    const Type* element = nullptr; // for pointers

    static Type Int() { return {TypeKind::Int, nullptr}; }
    static Type Char() { return {TypeKind::Char, nullptr}; }
    static Type Void() { return {TypeKind::Void, nullptr}; }
    static Type PointerTo(const Type* elem) { return {TypeKind::Pointer, elem}; }
};
