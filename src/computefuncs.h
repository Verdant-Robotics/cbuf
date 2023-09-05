#pragma once

#include "Interp.h"
#include "SymbolTable.h"

bool compute_simple(ast_struct* st, SymbolTable* symtable, Interp* interp);
bool compute_compact(ast_struct* st, SymbolTable* symtable, Interp* interp);
bool compute_hash(ast_struct* st, SymbolTable* symtable, Interp* interp);
