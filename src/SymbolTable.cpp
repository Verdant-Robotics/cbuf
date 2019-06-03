#include "SymbolTable.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

bool SymbolTable::add_struct(ast_struct *st)
{
    if (!find_symbol(st->name)) {
        ar.push_back(st);
        return true;
    }
    return false;
}

bool SymbolTable::add_enum(ast_enum *en)
{
    if (!find_symbol(en->name)) {
        enar.push_back(en);
        return true;
    }
    return false;
}

bool SymbolTable::find_symbol(const TextType name) const
{
    for(auto *st: ar) {
        if (!strcmp(name, st->name)) {
            return true;
        }
    }
    for(auto *en: enar) {
        if (!strcmp(name, en->name)) {
            return true;
        }
    }
    return false;
}

ast_struct* SymbolTable::find_struct(const TextType name) const
{
    for(auto *st: ar) {
        if (!strcmp(name, st->name)) {
            return st;
        }
    }
    return nullptr;
}

ast_enum* SymbolTable::find_enum(const TextType name) const
{
    for(auto *en: enar) {
        if (!strcmp(name, en->name)) {
            return en;
        }
    }
    return nullptr;
}

bool SymbolTable::initialize(ast_global *top_ast)
{
    for(auto *st: top_ast->global_space.structs) {
        bool bret = add_struct(st);
        if (!bret) {
            fprintf(stderr, "Found repeated struct name: %s\n", st->name);
            return false;
        }
    }
    for(auto *en: top_ast->global_space.enums) {
        bool bret = add_enum(en);
        if (!bret) {
            fprintf(stderr, "Found repeated enum name: %s\n", en->name);
            return false;
        }
    }
    for (auto *sp: top_ast->spaces) {
        for (auto *st: sp->structs) {
            bool bret = add_struct(st);
            if (!bret) {
                fprintf(stderr, "Found repeated struct name: %s\n", st->name);
                return false;
            }
        }
        for(auto *en: sp->enums) {
            bool bret = add_enum(en);
            if (!bret) {
                fprintf(stderr, "Found repeated enum name: %s\n", en->name);
                return false;
            }
        }
    }
    return true;
}
