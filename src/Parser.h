#pragma once

#include "Lexer.h"
#include "ast.h"
#include "Interp.h"

class Parser
{
    Allocator *pool;
    ast_namespace *current_scope = nullptr;
    ast_global *top_level_ast = nullptr;
    void Error(const char *msg, ...);
    void ErrorWithLoc(SrcLocation& loc, const char *msg, ...);
    bool MustMatchToken(TOKEN_TYPE type, const char *msg);
    ast_element * parseElementDeclaration();
    ast_namespace *find_existing_namespace(const TextType name);

public:
    Lexer *lex = nullptr;
    Interp *interp = nullptr;
    bool success;

    ast_global * Parse(const char *filename, Allocator *pool);
    ast_global * ParseBuffer(const char *buffer, u64 buf_size, Allocator *pool);

    ast_enum* parseEnum();
    ast_struct* parseStruct();
    ast_namespace* parseNamespace();
};
