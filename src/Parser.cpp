#include "Parser.h"
#include <stdlib.h>
#include <stdio.h>

void Parser::Error(const char *msg, ...)
{
    va_list args;
    SrcLocation loc;
    lex->getLocation(loc);
    s32 off = sprintf(errorString, "%s:%d:%d: error : ", lex->getFilename(), 
        loc.line, loc.col);

    va_start(args, msg);
    off += vsprintf(errorString + off, msg, args);
    va_end(args);
    success = false;
    errorString += off;
    errorString = lex->getFileData()->printLocation(loc, errorString);
}

void Parser::parseStruct()
{

}

void Parser::parseNamespace()
{
    
}

ast_global * Parser::Parse(const char *filename, PoolAllocator *pool)
{
    Lexer local_lex;
    this->lex = &local_lex;
    this->pool = pool;

    lex->setPoolAllocator(pool);

    if (errorString == nullptr) {
        errorString = errorStringBuffer;
        errorString[0] = 0;
    }

    if (!lex->openFile(filename)) {
        errorString += sprintf(errorString, "Error: File [%s] could not be opened to be processed\n", filename);
        return nullptr;
    }

    ast_global *top_ast = new (pool) ast_global;
    success = true;
    top_level_ast = top_ast;

    lex->parseFile();
    while (!lex->checkToken(TK_LAST_TOKEN)) {
        Token t;
        lex->lookaheadToken(t);
        if (t.type == TK_NAMESPACE) {
            parseNamespace();
        } else if (t.type == TK_STRUCT) {
            parseStruct();
        } else {
            Error("Unknown token, at the top level only structs and namespaces are allowed\n");
        }
    }

    this->lex = nullptr;
    top_level_ast = nullptr;
    return top_ast;
}