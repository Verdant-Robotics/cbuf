#include "Parser.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static bool isBuiltInType(TOKEN_TYPE t)
{
    return (t == TK_BOOL)
        || (t == TK_INT)
        || (t == TK_U8)
        || (t == TK_U16)
        || (t == TK_U32)
        || (t == TK_U64)
        || (t == TK_S8)
        || (t == TK_S16)
        || (t == TK_S32)
        || (t == TK_S64)
        || (t == TK_FLOAT)
        || (t == TK_F32)
        || (t == TK_F64)
        || (t == TK_STRING_KEYWORD)
        || (t == TK_SHORT_STRING_KEYWORD)
        || (t == TK_VOID);
}

void Parser::ErrorWithLoc(SrcLocation &loc, const char *msg, ...)
{
    va_list args;
    s32 off = sprintf(errorString, "%s:%d:%d: error : ", lex->getFilename(),
        loc.line, loc.col);

    va_start(args, msg);
    off += vsprintf(errorString + off, msg, args);
    va_end(args);
    success = false;
    errorString += off;
    errorString = lex->getFileData()->printLocation(loc, errorString);
}

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

bool Parser::MustMatchToken(TOKEN_TYPE type, const char *msg)
{
    if (!lex->checkToken(type)) {
        if (type == TK_SEMICOLON) {
            Token tok;
            lex->lookbehindToken(tok);
            ErrorWithLoc(tok.loc, "%s - Expected a semicolon after this token\n", msg);
            return false;
        }
        Error("%s - Token %s was expected, but we found: %s\n", msg,
            TokenTypeToStr(type), TokenTypeToStr(lex->getTokenType()) );
        return false;
    }
    lex->consumeToken();
    return true;
}

ast_element* Parser::parseElementDeclaration()
{
    Token t;
    lex->getNextToken(t);

    if (!isBuiltInType(t.type) && (t.type != TK_IDENTIFIER)) {
        Error("To define an element of a struct please use a built in type or defined struct");
        return nullptr;
    }
    ast_element *elem = new ast_element();
    if (t.type == TK_IDENTIFIER) {
        elem->custom_name = t.string;
        elem->type = TYPE_CUSTOM;
    } else {
        switch(t.type) {
        case TK_U8:
            elem->type = TYPE_U8; 
            break;
        case TK_U16:
            elem->type = TYPE_U16; 
            break;
        case TK_U32:
            elem->type = TYPE_U32; 
            break;
        case TK_U64:
            elem->type = TYPE_U64; 
            break;
        case TK_S8:
            elem->type = TYPE_S8; 
            break;
        case TK_S16:
            elem->type = TYPE_S16; 
            break;
        case TK_S32:
            elem->type = TYPE_S32; 
            break;
        case TK_S64:
            elem->type = TYPE_S64; 
            break;
        case TK_F32:
            elem->type = TYPE_F32;
            break;
        case TK_F64:
            elem->type = TYPE_F64;
            break;
        case TK_STRING_KEYWORD:
            elem->type = TYPE_STRING;
            break;            
        case TK_BOOL:
            elem->type = TYPE_BOOL;
            break;
        case TK_SHORT_STRING_KEYWORD:
            elem->type = TYPE_SHORT_STRING;
            // TODO: Parse now the size of the string, optionally
            break;
        default:
            Error("Something unforeseen happened here");
            return nullptr;
        }
    }

    // Now we parse the name, has to be an identifier.
    lex->getNextToken(t);
    if (t.type != TK_IDENTIFIER) {
        Error("An element of a struct needs to have a name");
        return nullptr;
    }
    elem->name = t.string;
    ast_array_definition *last_array = nullptr;

    // And now we check for possible array suffixes, and the semicolon
    while(!lex->checkToken(TK_SEMICOLON)) {
        if (lex->checkToken(TK_OPEN_SQBRACKET)) {
            lex->consumeToken();
            auto *ar = new ast_array_definition();
            if (lex->checkToken(TK_NUMBER)) {
                lex->getNextToken(t);
                ar->size = t._u64;
                while (!lex->checkToken(TK_CLOSE_SQBRACKET)) {
                    if (lex->checkToken(TK_STAR)) {
                        lex->consumeToken();
                        lex->getNextToken(t);
                        if (t.type != TK_NUMBER) {
                            Error("Number is expected after a '*' operator\n");
                            return nullptr;
                        }
                        ar->size *= t._u64;
                    } else {
                        Error("Only a close bracket or a multiplication operation are supported for now, found %s\n",
                              TokenTypeToStr(lex->getTokenType()));
                        return nullptr;
                    }
                }
            } 
            
            if (lex->checkToken(TK_CLOSE_SQBRACKET)) {
                lex->consumeToken();
                if (last_array == nullptr) {
                    elem->array_suffix = ar;
                    last_array = ar;
                } else {
                    // TODO: Enable Multidimensional arrays when needed
                    Error("Multidimensional arrays are not supported yet\n");
                    return nullptr;
                    last_array->next = ar;
                    last_array = ar;
                }
                if (ar->size == 0) elem->is_dynamic_array = true;
            } else {
                Error("Array close bracket could not be found");
                return nullptr;
            }
        } else {
          MustMatchToken(TK_SEMICOLON, "Expected semicolon\n");
          return nullptr;
        }
    }
    lex->consumeToken(); // eat the semicolon
    return elem;
}

ast_struct* Parser::parseStruct()
{
    Token t;
    lex->getNextToken(t);

    if (t.type != TK_STRUCT) {
        Error("Keyword 'struct' expected, found %s\n", TokenTypeToStr(t.type));
        return nullptr;
    }

    lex->getNextToken(t);
    if (t.type != TK_IDENTIFIER) {
        Error("After struct there has to be an identifier (name)\n");
        return nullptr;
    }
    ast_struct *nst = new ast_struct();
    nst->name = t.string;
     
    if (!MustMatchToken(TK_OPEN_BRACKET, "Please use brackets around a struct\n")) {
        return nullptr;
    }

    while(!lex->checkToken(TK_CLOSE_BRACKET)) {
        auto *elem = parseElementDeclaration();
        if (!success) {
            return nullptr;
        }
        nst->elements.push_back(elem);
    }
    lex->consumeToken();
    return nst;
}

ast_channel* Parser::parseChannel()
{
    Token t;
    lex->getNextToken(t);

    if (t.type != TK_CHANNEL) {
        Error("Keyword 'channel' expected, found %s\n", TokenTypeToStr(t.type));
        return nullptr;
    }

    lex->getNextToken(t);
    if (t.type != TK_IDENTIFIER) {
        Error("After channel there has to be an identifier (name)\n");
        return nullptr;
    }
    ast_channel *cn = new ast_channel();
    cn->name = t.string;

    if (!MustMatchToken(TK_COLON, "Please use colon after a channel to describe the struct the channel is composed of\n")) {
        return nullptr;
    }

    lex->getNextToken(t);
    if (t.type != TK_IDENTIFIER) {
        Error("After channel there has to be an identifier (name)\n");
        return nullptr;
    }
    cn->inner_struct = t.string;

    return cn;
}

ast_enum* Parser::parseEnum()
{
    Token t;
    lex->getNextToken(t);

    if (t.type != TK_ENUM) {
        Error("Keyword 'enum' expected, found: %s\n", TokenTypeToStr(t.type));
        return nullptr;
    }

    lex->getNextToken(t);
    if (t.type != TK_IDENTIFIER) {
        Error("After enum keyword there has to be an identifier (name), found: %s\n", TokenTypeToStr(t.type));
        return nullptr;
    }
    ast_enum *en = new ast_enum();
    en->name = t.string;

    if (!MustMatchToken(TK_OPEN_BRACKET, "Please use brackets around a namespace\n")) {
        return nullptr;
    }

    while(!lex->checkToken(TK_CLOSE_BRACKET)) {
        Token t;
        lex->lookaheadToken(t);

        if (t.type != TK_IDENTIFIER) {
            Error("Inside an enum only identifiers are allowed, found: %s\n", TokenTypeToStr(t.type));
            return nullptr;
        }
        // check that it is not a duplicate
        bool found = false;
        for(auto el:en->elements) {
            if (!strcmp(el, t.string)) {
                found = true;
                break;
            }
        }
        if (found) {
            Error("Found duplicate identifier inside enum: %s\n", t.string);
        }
        en->elements.push_back(t.string);

        lex->consumeToken();
        lex->lookaheadToken(t);
        if (t.type == TK_COMMA) {
            // consume the comma
            lex->consumeToken();
        } else if (t.type == TK_CLOSE_BRACKET) {
            // nothing to do, be ok to skip
        } else {
            Error("Found unexpected token: %s\n", TokenTypeToStr(t.type));
            return nullptr;
        }
    }
    lex->consumeToken();
    return en;
}

ast_namespace* Parser::parseNamespace()
{
    Token t;
    lex->getNextToken(t);

    if (t.type != TK_NAMESPACE) {
        Error("Keyword 'namespace' expected, found: %s\n", TokenTypeToStr(t.type));
        return nullptr;
    }

    lex->getNextToken(t);
    if (t.type != TK_IDENTIFIER) {
        Error("After namespace there has to be an identifier (name), found: %s\n", TokenTypeToStr(t.type));
        return nullptr;
    }
    ast_namespace *sp = new ast_namespace();
    sp->name = t.string;

    if (!MustMatchToken(TK_OPEN_BRACKET, "Please use brackets around a namespace\n")) {
        return nullptr;
    }

    while(!lex->checkToken(TK_CLOSE_BRACKET)) {
        Token t;
        lex->lookaheadToken(t);

        if (t.type == TK_NAMESPACE) {
            Error("Nested namespaces are not allowed");
            return nullptr;
        }
        if (t.type == TK_CHANNEL) {
            auto *cn = parseChannel();
            if (!success) {
                return nullptr;
            }
            sp->channels.push_back(cn);
            cn->space = sp;
        } else if (t.type == TK_STRUCT) {
            auto *st = parseStruct();
            if (!success) {
                return nullptr;
            }
            sp->structs.push_back(st);
            st->space = sp;
        } else if (t.type == TK_ENUM) {
            auto *en = parseEnum();
            if (!success) {
                return nullptr;
            }
            sp->enums.push_back(en);
            en->space = sp;
        } else {
            Error("Unrecognized keyword inside a namespace: %s\n", TokenTypeToStr(t.type));
            return nullptr;
        }
    }
    lex->consumeToken();
    return sp;
}

ast_global * Parser::ParseBuffer(const char *buffer, u64 buf_size, Allocator *pool)
{
    Lexer local_lex;
    this->lex = &local_lex;
    this->pool = pool;

    lex->setPoolAllocator(pool);

    if (errorString == nullptr) {
        errorString = errorStringBuffer;
        errorString[0] = 0;
    }

    if (!lex->loadString(buffer, buf_size)) {
        errorString += sprintf(errorString, "Error: String Buffer could not be opened to be processed\n");
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
            auto *sp = parseNamespace();
            if (!success) {
                return nullptr;
            }
            top_ast->spaces.push_back(sp);
        } else if (t.type == TK_STRUCT) {
            auto st = parseStruct();
            if (!success) {
                return nullptr;
            }
            top_ast->global_space.structs.push_back(st);
            st->space = &top_ast->global_space;
        } else if (t.type == TK_CHANNEL) {
            auto *cn = parseChannel();
            if (!success) {
                return nullptr;
            }
            top_ast->channels.push_back(cn);
        } else if (t.type == TK_ENUM) {
            auto *en = parseEnum();
            if (!success) {
                return nullptr;
            }
            top_ast->enums.push_back(en);
            en->space = &top_ast->global_space;
        } else {
            Error("Unknown token [%s], at the top level only structs and namespaces are allowed\n", TokenTypeToStr(t.type));
            return nullptr;
        }
    }

    this->lex = nullptr;
    top_level_ast = nullptr;
    return top_ast;
}

ast_global * Parser::Parse(const char *filename, Allocator *pool)
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
            auto *sp = parseNamespace();
            if (!success) {
                return nullptr;
            }
            top_ast->spaces.push_back(sp);
        } else if (t.type == TK_STRUCT) {
            auto st = parseStruct();
            if (!success) {
                return nullptr;
            }
            top_ast->global_space.structs.push_back(st);
            st->space = &top_ast->global_space;
        } else if (t.type == TK_CHANNEL) {
            auto *cn = parseChannel();
            if (!success) {
                return nullptr;
            }
            top_ast->channels.push_back(cn);
        } else if (t.type == TK_ENUM) {
            auto *en = parseEnum();
            if (!success) {
                return nullptr;
            }
            top_ast->enums.push_back(en);
            en->space = &top_ast->global_space;
        } else {
            Error("Unknown token [%s], at the top level only structs and namespaces are allowed\n", TokenTypeToStr(t.type));
            return nullptr;
        }
    }

    this->lex = nullptr;
    top_level_ast = nullptr;
    return top_ast;
}
