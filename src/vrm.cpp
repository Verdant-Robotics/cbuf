#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "Parser.h"
#include "SymbolTable.h"
#include "AstPrinter.h"
#include "CPrinter.h"

void checkParsing(const char *filename)
{
    Lexer lex;
    Token tok;
    PoolAllocator pool;

    lex.setPoolAllocator(&pool);
    lex.openFile(filename);
    lex.parseFile();

    while(tok.type != TK_LAST_TOKEN) {
        lex.getNextToken(tok);
        tok.print();
//        printf("%s\n", TokenTypeToStr(tok.type));
    }
}

template< typename T >
void loop_all_structs(ast_global *ast, SymbolTable *symtable, T func)
{
    for(auto *sp: ast->spaces) {
        for(auto *st: sp->structs) {
            func(st, symtable);
        }
    }

    for(auto *st: ast->global_space.structs) {
        func(st, symtable);
    }
}

bool compute_simple(ast_struct *st, SymbolTable *symtable)
{
    if (st->simple_computed) return st->simple;
    st->simple = true;
    for (auto *elem: st->elements) {
        if (elem->type == TYPE_STRING) {
            st->simple = false;
            st->simple_computed = true;
            return false;
        }
        if (elem->type == TYPE_CUSTOM) {
            auto *inner_st = symtable->find_symbol(elem->custom_name);
            if (inner_st == nullptr) {
                fprintf(stderr, "Struct %s, element %s was referencing type %s and could not be found\n",
                    st->name, elem->name, elem->custom_name);
                exit(-1);
            }
            bool elem_simple = compute_simple(inner_st, symtable);
            if (!elem_simple) {
                st->simple = false;
                st->simple_computed = true;
                return false;
            }
        }
    }
    st->simple_computed = true;
    return true;
}

unsigned long hash(const unsigned char *str)
{
    unsigned long hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

bool compute_hash(ast_struct *st, SymbolTable *symtable) 
{
    StringBuffer buf;
    AstPrinter printer;
    if (st->hash_computed) return true;

    for(auto *elem: st->elements) {
        if (elem->type == TYPE_CUSTOM) {
            auto *inner_st = symtable->find_symbol(elem->custom_name);
            bool bret = compute_hash(inner_st, symtable);
            if (!bret) return false;
            buf.print("%lX ", inner_st->hash_value);
        } else {
            printer.print_ast(&buf, elem);
        }
    }

    st->hash_value = hash((const unsigned char*)buf.get_buffer());
    st->hash_computed = true;
    return true;
}

int main(int argc, char **argv)
{
    Parser parser;
    PoolAllocator pool;
    if (argc < 2) {
        printf("Please provide a vrm file\n");
        exit(0);
    }

    std::string nk;
    // checkParsing(argv[1]);

    auto top_ast = parser.Parse(argv[1], &pool);
    if (!parser.success) {
        printf("Error during parsing:\n%s\n", parser.getErrorString());
        return -1;
    }

    SymbolTable symtable;
    bool bret = symtable.initialize(top_ast);
    if (!bret) return -1;

    loop_all_structs(top_ast, &symtable, compute_simple);
    loop_all_structs(top_ast, &symtable, compute_hash);
/*
    AstPrinter printer;
    StringBuffer buf;
    printer.print_ast(&buf, top_ast);
    printf("%s", buf.get_buffer());
*/

    CPrinter printer;
    StringBuffer buf;
    printer.print(&buf, top_ast);
    printf("%s", buf.get_buffer());
    return 0;
}
