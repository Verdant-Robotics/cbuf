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

public:
    Lexer *lex;
    bool success;

    ast_global * Parse(const char *filename, PoolAllocator *pool);
    void parseStruct();
    void parseNamespace(); 
};