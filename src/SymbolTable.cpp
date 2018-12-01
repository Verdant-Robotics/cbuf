#include "SymbolTable.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

bool SymbolTable::add_struct(ast_struct *st)
{
    if (find_symbol(st->name) == nullptr) {
        ar.push_back(st);
        return true;
    }
    return false;
}

ast_struct *SymbolTable::find_symbol(TextType name)
{
    for(auto *st: ar) {
        if (!strcmp(name, st->name)) {
            return st;
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
    for (auto *sp: top_ast->spaces) {
        for (auto *st: sp->structs) {
            bool bret = add_struct(st);
            if (!bret) {
                fprintf(stderr, "Found repeated struct name: %s\n", st->name);
                return false;
            }
        }
    }
    return true;
}
