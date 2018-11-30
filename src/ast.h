#pragma once
#include "Array.h"
#include "mytypes.h"
#include "TextType.h"

struct ast_namespace;

enum ElementType
{
    TYPE_U8,
    TYPE_U16,
    TYPE_U32,
    TYPE_U64,
    TYPE_S8,
    TYPE_S16,
    TYPE_S32,
    TYPE_S64,
    TYPE_F32,
    TYPE_F64,
    TYPE_STRING,
    CUSTOM
};

struct ast_element
{
    TextType name = nullptr ;
    ElementType type;
    TextType custom_name = nullptr;  
    bool is_dynamic_array = false;
};

struct ast_struct
{
    Array<ast_element *> elements;    
    struct ast_namespace* space = nullptr;
    bool simple = false;
};

struct ast_namespace
{
    Array<ast_struct *> structs;
};

struct ast_global
{
    Array<ast_namespace *> spaces;
    ast_namespace global_space;
}