#include <stdio.h>
#include <stdlib.h>
#include "Parser.h"

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
    print_ast(top_ast);
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

void print_elem(ast_element *elem)
{
    printf("      "); // indentation
    if (elem->custom_name) {
        printf("%s ", elem->custom_name);
    } else {
        printf("%s ", ElementTypeToStr[elem->type]);
    }
    printf("%s", elem->name);
    ast_array_definition *ar = elem->array_suffix;
    while(ar != nullptr) {
        if (ar->size == 0) printf("[]");
        else printf("[%ld]", ar->size);
        ar = ar->next;
    }
    printf(";\n");
}

void print_struct(ast_struct *st)
{
    printf("    struct %s {\n", st->name);
    for(auto *elem: st->elements) {
        print_elem(elem);
    }
    printf("}\n\n");
}

void print_namespace(ast_namespace *sp)
{
    printf("  namespace %s {\n", sp->name);
    for(auto *st: sp->structs) {
        print_struct(st);
    }
    printf("}\n\n");
}

void print_ast(ast_global *top)
{
    for(auto *sp: top->spaces) {
        print_namespace(sp);
    }
    printf("\nGlobal namespace:\n");
    for(auto *st: top->global_space.structs) {
        print_struct(st);
    }
}