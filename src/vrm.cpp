#include <stdio.h>
#include <stdlib.h>
#include "Parser.h"
#include "SymbolTable.h"
#include "AstPrinter.h"

void print_ast(ast_global *top);

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

bool compute_hash(ast_struct *st, SymbolTable *symtable) 
{
    return false;
}

int main(int argc, char **argv)
{
    Parser parser;
    PoolAllocator pool;
    if (argc < 2) {
        printf("Please provide a vrm file\n");
        exit(0);
    }

    // checkParsing(argv[1]);

    auto top_ast = parser.Parse(argv[1], &pool);
    if (!parser.success) {
        printf("Error during parsing:\n%s\n", parser.getErrorString());
        return -1;
    }

    AstPrinter printer;
    auto *str = printer.print_ast(top_ast);
    printf("%s", str);
//     print_ast(top_ast);

    SymbolTable symtable;
    bool bret = symtable.initialize(top_ast);
    if (!bret) return -1;


    return 0;
}


static const char * ElementTypeToStr[] = {
    "uint8_t",
    "uint16_t",
    "uint32_t",
    "uint64_t",
    "int8_t",
    "int16_t",
    "int32_t",
    "int64_t",
    "float",
    "double",
    "std::string",
    "bool"
};

void print_elem(u32 ident, ast_element *elem)
{
    printf("%*s", ident, " "); // indentation
    ast_array_definition *ar = elem->array_suffix;
    bool close_array = false;

    if (ar && ar->size == 0 && elem->is_dynamic_array) {
        printf("std::vector< ");
        close_array = true;
    }
    if (elem->custom_name) {
        printf("%s ", elem->custom_name);
    } else {
        printf("%s ", ElementTypeToStr[elem->type]);
    }
    if (close_array) {
        printf("> ");
    }
    printf("%s", elem->name);
    while(ar != nullptr) {
        if (ar->size != 0) printf("[%ld]", ar->size);
        ar = ar->next;
    }
    printf(";\n");
}

void print_struct(u32 ident, ast_struct *st)
{
    printf("%*sstruct %s {\n", ident, " ", st->name);
    for(auto *elem: st->elements) {
        print_elem(ident + 4, elem);
    }
    printf("%*s}\n\n", ident, " ");
}

void print_namespace(u32 ident, ast_namespace *sp)
{
    printf("%.*snamespace %s {\n",ident, " ", sp->name);
    for(auto *st: sp->structs) {
        print_struct(ident+4,st);
    }
    printf("%*s}\n\n", ident, " ");
}

void print_ast(ast_global *top)
{
    for(auto *sp: top->spaces) {
        print_namespace(4, sp);
    }
    printf("\nGlobal namespace:\n");
    for(auto *st: top->global_space.structs) {
        print_struct(4, st);
    }
}