#pragma once

#include "Lexer.h"
#include "ast.h"

class Parser
{
    PoolAllocator *pool;
    char errorStringBuffer[4096];
    char *errorString = nullptr;
    ast_namespace *current_scope = nullptr;
    ast_global *top_level_ast = nullptr;
    void Error(const char *msg, ...);
    void ErrorWithLoc(SrcLocation& loc, const char *msg, ...);
    bool MustMatchToken(TOKEN_TYPE type, const char *msg);
    ast_element * parseElementDeclaration();

public:
    Lexer *lex;
    bool success;

    ast_global * Parse(const char *filename, PoolAllocator *pool);
    ast_struct* parseStruct();
    ast_channel* parseChannel();
    ast_namespace* parseNamespace();
    const char *getErrorString() { return errorStringBuffer; } 
};