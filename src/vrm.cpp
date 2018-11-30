#include <stdio.h>
#include <stdlib.h>
#include "Lexer.h"

int main(int argc, char **argv)
{
    Lexer lex;
    Token tok;
    PoolAllocator pool;
    if (argc < 2) {
        printf("Please provide a vrm file\n");
        exit(0);
    }
    lex.setPoolAllocator(&pool);
    lex.openFile(argv[1]);
    lex.parseFile();

    while(tok.type != TK_LAST_TOKEN) {
        lex.getNextToken(tok);
        tok.print();
//        printf("%s\n", TokenTypeToStr(tok.type));
    }
    return 0;
}