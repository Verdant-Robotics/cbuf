#include "Parser.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

static bool file_exists(const char* file) {
  struct stat buffer;
  return (stat(file, &buffer) == 0);
}

static bool isBuiltInType(TOKEN_TYPE t) {
  // clang-format off
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
  // clang-format on
}

void Parser::ErrorWithLoc(SrcLocation& loc, const char* msg, ...) {
  va_list args;
  va_start(args, msg);
  interp->ErrorWithLoc(loc, lex->getFileData(), msg, args);
  va_end(args);
  success = false;
}

void Parser::Error(const char* msg, ...) {
  va_list args;
  SrcLocation loc;
  lex->getLocation(loc);
  va_start(args, msg);
  interp->ErrorWithLoc(loc, lex->getFileData(), msg, args);
  va_end(args);
  success = false;
}

bool Parser::MustMatchToken(TOKEN_TYPE type, const char* msg) {
  if (!lex->checkToken(type)) {
    if (type == TK_SEMICOLON) {
      Token tok;
      lex->lookbehindToken(tok);
      ErrorWithLoc(tok.loc, "%s - Expected a semicolon after this token\n", msg);
      return false;
    }
    Error("%s - Token %s was expected, but we found: %s\n", msg, TokenTypeToStr(type),
          TokenTypeToStr(lex->getTokenType()));
    return false;
  }
  lex->consumeToken();
  return true;
}

ast_element* Parser::parseElementDeclaration() {
  Token t;
  lex->getNextToken(t);

  if (!isBuiltInType(t.type) && (t.type != TK_IDENTIFIER)) {
    Error("To define an element of a struct please use a built in type or defined struct");
    return nullptr;
  }
  ast_element* elem = new ast_element();
  lex->getLocation(elem->loc);
  if (t.type == TK_IDENTIFIER) {
    elem->custom_name = t.string;
    elem->type = TYPE_CUSTOM;
    if (lex->checkToken(TK_DOUBLE_COLON)) {
      lex->consumeToken();
      lex->getNextToken(t);
      if (t.type != TK_IDENTIFIER) {
        Error("Please put a name after the namespace");
        return nullptr;
      }
      elem->namespace_name = elem->custom_name;
      elem->custom_name = t.string;
    }
  } else {
    switch (t.type) {
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
  ast_array_definition* last_array = nullptr;

  // And now we check for possible array suffixes, and the semicolon
  while (!lex->checkToken(TK_SEMICOLON)) {
    if (lex->checkToken(TK_OPEN_SQBRACKET)) {
      lex->consumeToken();
      auto* ar = new ast_array_definition();
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

      if (lex->checkToken(TK_COMPACT_ARRAY)) {
        lex->consumeToken();
        if (ar->size == 0) {
          Error("Maximim compact arrays are only supported on static, size defined arrays\n");
          return nullptr;
        }
        if (elem->type == TYPE_STRING) {
          Error("Compact arrays are not supported for strings\n");
          return nullptr;
        }
        elem->is_compact_array = true;
      }
    } else if (lex->checkToken(TK_ASSIGN)) {
      lex->consumeToken();
      if (lex->checkToken(TK_STRING) || lex->checkToken(TK_NUMBER) || lex->checkToken(TK_FNUMBER) ||
          lex->checkToken(TK_IDENTIFIER)) {
        lex->getNextToken(t);
        if (t.type == TK_STRING) {
          char sbuf[1024] = {};
          sbuf[0] = '"';
          strncpy(sbuf + 1, t.string, sizeof(sbuf) - 3);
          sbuf[strlen(sbuf)] = '"';
          elem->init_value = CreateTextType(pool, sbuf);
        } else if (t.type == TK_FNUMBER) {
          if (elem->type != TYPE_F32 && elem->type != TYPE_F64) {
            Error("Initialization value is a floating point value but the element is not of type float.\n");
            return nullptr;
          }
          elem->init_value = t.string;
        } else {
          elem->init_value = t.string;
        }
      } else {
        Error("Initialization value has to be a string, number or identifier\n");
        return nullptr;
      }
    } else {
      MustMatchToken(TK_SEMICOLON, "Expected semicolon\n");
      return nullptr;
    }
  }
  lex->consumeToken();  // eat the semicolon
  return elem;
}

ast_struct* Parser::parseStruct() {
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
  ast_struct* nst = new ast_struct();
  lex->getLocation(nst->loc);
  nst->name = t.string;
  nst->file = lex->getFileData();

  if (lex->checkToken(TK_NAKED)) {
    lex->consumeToken();
    nst->naked = true;
  }

  if (!MustMatchToken(TK_OPEN_BRACKET, "Please use brackets around a struct\n")) {
    return nullptr;
  }

  while (!lex->checkToken(TK_CLOSE_BRACKET)) {
    auto* elem = parseElementDeclaration();
    if (!success) {
      return nullptr;
    }
    elem->enclosing_struct = nst;
    nst->elements.push_back(elem);
  }
  lex->consumeToken();
  return nst;
}

ast_enum* Parser::parseEnum() {
  Token t;
  lex->getNextToken(t);
  bool is_class = false;

  if (t.type != TK_ENUM) {
    Error("Keyword 'enum' expected, found: %s\n", TokenTypeToStr(t.type));
    return nullptr;
  }

  lex->getNextToken(t);
  if (t.type == TK_CLASS) {
    is_class = true;
    lex->getNextToken(t);
  }

  if (t.type != TK_IDENTIFIER) {
    Error("After enum keyword there has to be an identifier (name), found: %s\n", TokenTypeToStr(t.type));
    return nullptr;
  }
  ast_enum* en = new ast_enum();
  en->name = t.string;
  lex->getLocation(en->loc);
  en->file = lex->getFileData();
  en->is_class = is_class;

  if (!MustMatchToken(TK_OPEN_BRACKET, "Please use brackets around a namespace\n")) {
    return nullptr;
  }

  while (!lex->checkToken(TK_CLOSE_BRACKET)) {
    Token t;
    lex->lookaheadToken(t);

    if (t.type != TK_IDENTIFIER) {
      Error("Inside an enum only identifiers are allowed, found: %s\n", TokenTypeToStr(t.type));
      return nullptr;
    }
    // check that it is not a duplicate
    bool found = false;
    for (auto el : en->elements) {
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

ast_const* Parser::parseConst() {
  Token t;
  lex->getNextToken(t);

  if (t.type != TK_CONST) {
    Error("Keyword 'const' expected, found: %s\n", TokenTypeToStr(t.type));
    return nullptr;
  }

  lex->getNextToken(t);
  if (!isBuiltInType(t.type)) {
    Error("After const keyword there has to be a basic built in type, found: %s\n", TokenTypeToStr(t.type));
    return nullptr;
  }

  ast_const* cst = new ast_const();
  switch (t.type) {
    case TK_U8:
      cst->type = TYPE_U8;
      break;
    case TK_U16:
      cst->type = TYPE_U16;
      break;
    case TK_U32:
      cst->type = TYPE_U32;
      break;
    case TK_U64:
      cst->type = TYPE_U64;
      break;
    case TK_S8:
      cst->type = TYPE_S8;
      break;
    case TK_S16:
      cst->type = TYPE_S16;
      break;
    case TK_S32:
      cst->type = TYPE_S32;
      break;
    case TK_S64:
      cst->type = TYPE_S64;
      break;
    case TK_F32:
      cst->type = TYPE_F32;
      break;
    case TK_F64:
      cst->type = TYPE_F64;
      break;
    case TK_STRING_KEYWORD:
    case TK_SHORT_STRING_KEYWORD:
      cst->type = TYPE_STRING;
      break;
    case TK_BOOL:
      cst->type = TYPE_BOOL;
      break;
    default:
      Error("Something unforeseen happened here");
      delete cst;
      return nullptr;
  }

  lex->getLocation(cst->loc);

  lex->getNextToken(t);
  if (t.type != TK_IDENTIFIER) {
    Error("After const keyword and type there has to be an identifier (name), found: %s\n",
          TokenTypeToStr(t.type));
    return nullptr;
  }
  cst->name = t.string;
  cst->file = lex->getFileData();

  if (!MustMatchToken(TK_ASSIGN, "Please use '=' when declaring a const value\n")) {
    return nullptr;
  }

  lex->getNextToken(t);
  cst->value.str_val = t.string;
  if (cst->type == TYPE_STRING) {
    if (t.type != TK_STRING) {
      Error("Expected a constant of type string but got a value of type %s\n", TokenTypeToStr(t.type));
      return nullptr;
    }
  } else if ((cst->type == TYPE_F32) || (cst->type == TYPE_F64)) {
    if (t.type != TK_FNUMBER) {
      Error("Expected a constant of type floating point but got a value of type %s\n",
            TokenTypeToStr(t.type));
      return nullptr;
    }
    cst->value.float_val = t._f64;
  } else {
    if (t.type != TK_NUMBER) {
      Error("Expected a constant of type integer but got a value of type %s\n", TokenTypeToStr(t.type));
      return nullptr;
    }
    cst->value.int_val = t._u64;
    cst->value.is_hex = t._is_hex;
  }

  if (!MustMatchToken(TK_SEMICOLON, "Please use a semicolon after a const declaration\n")) {
    return nullptr;
  }

  return cst;
}

ast_namespace* Parser::parseNamespace() {
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
  ast_namespace* sp = find_existing_namespace(t.string);
  if (sp == nullptr) {
    sp = new ast_namespace();
    sp->name = t.string;
    top_level_ast->spaces.push_back(sp);
  }

  if (!MustMatchToken(TK_OPEN_BRACKET, "Please use brackets around a namespace\n")) {
    return nullptr;
  }

  while (!lex->checkToken(TK_CLOSE_BRACKET)) {
    Token t;
    lex->lookaheadToken(t);

    if (t.type == TK_NAMESPACE) {
      Error("Nested namespaces are not allowed");
      return nullptr;
    }
    if (t.type == TK_STRUCT) {
      auto* st = parseStruct();
      if (!success) {
        return nullptr;
      }
      sp->structs.push_back(st);
      st->space = sp;
    } else if (t.type == TK_ENUM) {
      auto* en = parseEnum();
      if (!success) {
        return nullptr;
      }
      sp->enums.push_back(en);
      en->space = sp;
    } else if (t.type == TK_CONST) {
      auto* cst = parseConst();
      if (!success) {
        return nullptr;
      }
      sp->consts.push_back(cst);
      cst->space = sp;
    } else {
      Error("Unrecognized keyword inside a namespace: %s\n", TokenTypeToStr(t.type));
      return nullptr;
    }
  }
  lex->consumeToken();

  return sp;
}

void Parser::parseImport() {
  Token t;
  lex->getNextToken(t);

  if (t.type != TK_IMPORT) {
    Error("Keyword 'import' expected, found: %s\n", TokenTypeToStr(t.type));
    return;
  }

  lex->getNextToken(t);
  if (t.type != TK_STRING) {
    Error("After a #import there has to be a filename, in quotes, found: %s\n", TokenTypeToStr(t.type));
    return;
  }

  for (auto& alread_imported : top_level_ast->imported_files) {
    if (!strcmp(alread_imported, t.string)) {
      // This is like doing a #pragma once
      return;
    }
  }

  // Declare a local parser and parse the input file as needed
  Parser local_parser;
  local_parser.args = this->args;
  local_parser.interp = this->interp;

  // try to find the file we have to import, t.string;
  for (auto folder : args->incs) {
    char impfile[128] = {};
    sprintf(impfile, "%s/%s", folder, t.string);
    if (file_exists(impfile)) {
      local_parser.Parse(impfile, pool, top_level_ast);
      success = local_parser.success;
      top_level_ast->imported_files.push_back(t.string);
      return;
    }
  }
  interp->ErrorWithLoc(t.loc, lex->getFileData(), "Could not find import file: %s\n", t.string);
  success = false;
}

ast_global* Parser::ParseBuffer(const char* buffer, u64 buf_size, Allocator* pool, ast_global* top) {
  Lexer local_lex;
  this->lex = &local_lex;
  this->pool = pool;

  lex->setPoolAllocator(pool);

  if (!lex->loadString(buffer, buf_size)) {
    interp->Error("Error: String Buffer could not be opened to be processed\n");
    return nullptr;
  }

  return ParseInternal(top);
}

ast_global* Parser::Parse(const char* filename, Allocator* pool, ast_global* top) {
  Lexer local_lex;
  this->lex = &local_lex;
  this->pool = pool;

  lex->setPoolAllocator(pool);

  if (!lex->openFile(filename)) {
    interp->Error("Error: File [%s] could not be opened to be processed\n", filename);
    return nullptr;
  }

  return ParseInternal(top);
}

ast_global* Parser::ParseInternal(ast_global* top) {
  ast_global* top_ast;
  if (top == nullptr) {
    top_ast = new (pool) ast_global;
    top_ast->global_space.name = CreateTextType(pool, GLOBAL_NAMESPACE);
    top_ast->main_file = lex->getFileData();
  } else {
    top_ast = top;
  }

  success = true;
  top_level_ast = top_ast;

  lex->parseFile();
  while (!lex->checkToken(TK_LAST_TOKEN)) {
    Token t;
    lex->lookaheadToken(t);
    if (t.type == TK_IMPORT) {
      parseImport();
      if (!success) {
        return nullptr;
      }
    } else if (t.type == TK_NAMESPACE) {
      (void)parseNamespace();
      if (!success) {
        return nullptr;
      }
    } else if (t.type == TK_STRUCT) {
      auto st = parseStruct();
      if (!success) {
        return nullptr;
      }
      top_ast->global_space.structs.push_back(st);
      st->space = &top_ast->global_space;
    } else if (t.type == TK_ENUM) {
      auto* en = parseEnum();
      if (!success) {
        return nullptr;
      }
      top_ast->enums.push_back(en);
      en->space = &top_ast->global_space;
    } else if (t.type == TK_CONST) {
      auto* cst = parseConst();
      if (!success) {
        return nullptr;
      }
      top_ast->consts.push_back(cst);
      cst->space = &top_ast->global_space;
    } else {
      Error("Unknown token [%s], at the top level only structs and namespaces are allowed\n",
            TokenTypeToStr(t.type));
      return nullptr;
    }
  }

  lex = nullptr;
  top_level_ast = nullptr;
  return top_ast;
}

ast_namespace* Parser::find_existing_namespace(const TextType name) {
  for (auto* sp : top_level_ast->spaces) {
    if (!strcmp(sp->name, name)) {
      return sp;
    }
  }
  return nullptr;
}
