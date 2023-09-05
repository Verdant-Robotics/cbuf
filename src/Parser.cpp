#include "Parser.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "TokenType.h"
#include "ast.h"

static bool file_exists(const char* file) {
  struct stat buffer;
  return (stat(file, &buffer) == 0);
}

#define CASE_ELEM_TYPE(e) \
  case TYPE_##e:          \
    return #e

static const char* ElementTypeToStr(ElementType t) {
  switch (t) {
    CASE_ELEM_TYPE(U8);
    CASE_ELEM_TYPE(U16);
    CASE_ELEM_TYPE(U32);
    CASE_ELEM_TYPE(U64);
    CASE_ELEM_TYPE(S8);
    CASE_ELEM_TYPE(S16);
    CASE_ELEM_TYPE(S32);
    CASE_ELEM_TYPE(S64);
    CASE_ELEM_TYPE(F32);
    CASE_ELEM_TYPE(F64);
    CASE_ELEM_TYPE(STRING);
    CASE_ELEM_TYPE(SHORT_STRING);
    CASE_ELEM_TYPE(BOOL);
    CASE_ELEM_TYPE(CUSTOM);
    default:
      return "UNKNOWN_ELEMENT_TYPE";
  }
}

#define CASE_VAL_TYPE(e) \
  case VALTYPE_##e:      \
    return #e

static const char* ValueTypeToStr(ValueType vt) {
  switch (vt) {
    CASE_VAL_TYPE(INVALID);
    CASE_VAL_TYPE(INTEGER);
    CASE_VAL_TYPE(FLOAT);
    CASE_VAL_TYPE(STRING);
    CASE_VAL_TYPE(IDENTIFIER);
    CASE_VAL_TYPE(BOOL);
    CASE_VAL_TYPE(ARRAY);
  }
#ifdef __llvm__
  __builtin_debugtrap();
#else
  __builtin_trap();
#endif
  return "";
}

static double getDoubleVal(const ast_value* val) noexcept {
  if (val->valtype == VALTYPE_INTEGER) {
    return (double)val->int_val;
  } else if (val->valtype == VALTYPE_FLOAT) {
    return val->float_val;
  }
  assert(false);  // "Code should never get here!");
  return 0.0;
}

static s64 getIntVal(const ast_value* val) noexcept {
  if (val->valtype == VALTYPE_INTEGER) {
    return val->int_val;
  }
  assert(false);  // "Code should never get here!");
  return 0;
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

static bool isUnaryPrefixOperator(TOKEN_TYPE type) {
  // clang-format off
    return (type == TK_PLUS)
        || (type == TK_MINUS);
  // clang-format on
}

static bool isBinOperator(TOKEN_TYPE type) {
  // clang-format off
    return (type == TK_STAR)
        || (type == TK_DIV)
        || (type == TK_MOD)
        || (type == TK_PLUS)
        || (type == TK_MINUS);
  // clang-format on
}

static u32 getPrecedence(TOKEN_TYPE t) {
  switch (t) {
    case TK_STAR:
    case TK_DIV:
    case TK_MOD:
      return 1;
    case TK_MINUS:
    case TK_PLUS:
      return 2;
    default:
      return 0;
  }
}

static bool isHexNumber(Token& t) {
  if (t.type == TK_NUMBER) {
    return strcmp("0x", t.string) == 0;
  }
  return false;
}

static f64 computeDecimal(Token& t) {
  assert(t.type == TK_NUMBER);
  assert(!isHexNumber(t));

  double total = 0;
  double divisor = 10;
  char* s = t.string;
  while (*s != 0) {
    float num = (float)(*s - '0');
    total += num / divisor;
    divisor *= 10;
    s++;
  }

  return total;
}

static ValueType ElemTypeToValType(ElementType t) {
  switch (t) {
    case TYPE_U8:
    case TYPE_U16:
    case TYPE_U32:
    case TYPE_U64:
    case TYPE_S8:
    case TYPE_S16:
    case TYPE_S32:
    case TYPE_S64:
      return VALTYPE_INTEGER;
    case TYPE_F32:
    case TYPE_F64:
      return VALTYPE_FLOAT;
    case TYPE_BOOL:
      return VALTYPE_BOOL;
    case TYPE_STRING:
    case TYPE_SHORT_STRING:
      return VALTYPE_STRING;
    default:
      return VALTYPE_INVALID;
  }
}

static bool isValTypeOperable(ValueType vt) { return (vt == VALTYPE_INTEGER) || (vt == VALTYPE_FLOAT); }

// Checks if something of type 2 can be assigned to type 1
// loosely
static bool checkTypes(ElementType t1, const ast_value* val) {
  auto vt1 = ElemTypeToValType(t1);

  if (vt1 == VALTYPE_INVALID || val->valtype == VALTYPE_INVALID) {
    return false;
  }

  if (vt1 == VALTYPE_FLOAT && val->valtype == VALTYPE_INTEGER) {
    // Allow for implicit integer promotion to float
    return true;
  }

  if (val->valtype == VALTYPE_ARRAY) {
    // check all it's elements which could be arrays themselves in the future
    bool success = true;
    auto arr_val = static_cast<const ast_array_value*>(val);
    for (auto& elem : arr_val->values) {
      success &= checkTypes(t1, elem);
    }
    return success;
  }
  return vt1 == val->valtype;
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

/*
 * Parses an array initializer. This is a comma separated list of expressions
 * enclosed in braces. No type checking is done here.
 */
ast_array_expression* Parser::parseArrayExpression() {
  if (!lex->checkToken(TK_OPEN_BRACKET)) {
    Error("Expected an array initializer starting with a \'{\'\n");
    return nullptr;
  }
  lex->consumeToken();
  ast_array_expression* arr = new (pool) ast_array_expression();
  while (true) {
    ast_expression* expr = parseExpression();
    if (!success) {
      return nullptr;
    }
    arr->expressions.push_back(expr);
    if (!lex->checkToken(TK_COMMA) && !lex->checkToken(TK_CLOSE_BRACKET)) {
      Error("Expected a comma or a closing bracket");
      return nullptr;
    }
    if (lex->checkToken(TK_COMMA)) {
      lex->consumeToken();
    } else if (lex->checkToken(TK_CLOSE_BRACKET)) {
      break;
    }
  }
  // Consume the closing bracket
  lex->consumeToken();
  success = true;
  return arr;
}

ast_element* Parser::parseElementDeclaration() {
  Token t;
  lex->getNextToken(t);

  if (!isBuiltInType(t.type) && (t.type != TK_IDENTIFIER)) {
    Error("To define an element of a struct please use a built in type or defined struct");
    return nullptr;
  }
  ast_element* elem = new (pool) ast_element();
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
      auto* ar = new (pool) ast_array_definition();
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

      // After parsing <type> <name>[<size>] we check for the compact array
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
      lex->lookaheadToken(t);

      ast_value* val = nullptr;

      if (t.type == TOKEN_TYPE::TK_IDENTIFIER) {
        val = new (pool) ast_value();
        val->exptype = EXPTYPE_LITERAL;
        val->valtype = VALTYPE_IDENTIFIER;
        val->type = TYPE_STRING;
        val->str_val = t.string;
        lex->consumeToken();
      } else {
        auto* expr = parseExpression();
        if (!success) return nullptr;

        // We have an expression initialization, first compute its value if we can
        if (!(val = computeExpressionValue(expr))) {
          return nullptr;
        }
      }
      // Check that the value and the type agree
      if (!checkTypes(elem->type, val)) {
        // TODO(kartikarcot): should we allow special cases for array values?
        // Do special support for legacy cases
        bool allow_special_case = false;
        if ((elem->type == TYPE_BOOL) && (val->valtype == VALTYPE_INTEGER)) {
          allow_special_case = (val->int_val == 0 || val->int_val == 1);
        }
        if ((elem->type == TYPE_CUSTOM) && (val->valtype == VALTYPE_IDENTIFIER)) {
          allow_special_case = true;
        }
        if (!allow_special_case) {
          // TODO(kartikarcot): How to make this error more informative for array values
          Error("Could not assign an initialization of type %s to member %s of type %s\n",
                ValueTypeToStr(val->valtype), elem->name, ElementTypeToStr(elem->type));
          return nullptr;
        }
      }
      if (elem->array_suffix || val->valtype == VALTYPE_ARRAY) {
        // check if either the element is an array or the value is an array then both should be arrays
        if (!(elem->array_suffix && val->valtype == VALTYPE_ARRAY)) {
          Error("Could not assign an initialization of type %s to member %s of type %s\n",
                ValueTypeToStr(val->valtype), elem->name, ElementTypeToStr(elem->type));
          return nullptr;
        }
        auto arr_val = static_cast<ast_array_value*>(val);
        // TODO(kartikarcot): Enable Multidimensional arrays when needed
        // check that the array sizes match
        if (elem->array_suffix->size != 0 && arr_val->values.size() != elem->array_suffix->size) {
          Error("Could not assign an initialization of size %d to member %s of size %d\n",
                arr_val->values.size(), elem->name, elem->array_suffix->size);
          return nullptr;
        }
      }
      elem->init_value = val;
    } else {
      MustMatchToken(TK_SEMICOLON, "Expected semicolon\n");
      return nullptr;
    }
  }
  lex->consumeToken();  // eat the semicolon
  return elem;
}

ast_value* Parser::computeExpressionValue(ast_expression* expr) {
  if (expr->exptype == EXPTYPE_LITERAL) {
    return static_cast<ast_value*>(expr);
  } else if (expr->exptype == EXPTYPE_ARRAY_LITERAL) {
    auto val = new (pool) ast_array_value;
    // compute the ast_array_value from the ast_array_expression
    ast_array_expression* arr_expr = static_cast<ast_array_expression*>(expr);
    val->valtype = VALTYPE_ARRAY;
    val->exptype = EXPTYPE_ARRAY_LITERAL;
    for (auto& e : arr_expr->expressions) {
      ast_value* v = nullptr;
      if (!(v = computeExpressionValue(e))) {
        return nullptr;
      }
      val->values.push_back(v);
    }
    return val;
  } else if (expr->exptype == EXPTYPE_UNARY) {
    ast_unaryexp* un = static_cast<ast_unaryexp*>(expr);
    ast_value* val = nullptr;
    if (!(val = computeExpressionValue(un->expr))) {
      return nullptr;
    }
    if (!isValTypeOperable(val->valtype)) {
      Error("Unable to apply unary operand %s to %s\n", TokenTypeToStr(un->op), ValueTypeToStr(val->valtype));
      return nullptr;
    }
    if (un->op == TK_PLUS) {
      return val;
    } else if (un->op == TK_MINUS) {
      if (val->valtype == VALTYPE_FLOAT) {
        val->float_val = -val->float_val;
      } else {
        val->int_val = -val->int_val;
      }
      return val;
    }
  } else if (expr->exptype == EXPTYPE_BINARY) {
    ast_binaryexp* bin = static_cast<ast_binaryexp*>(expr);
    ast_value* lval = nullptr;
    ast_value* rval = nullptr;
    if (!(lval = computeExpressionValue(bin->lhs)) || !(rval = computeExpressionValue(bin->rhs))) {
      return nullptr;
    }

    if (!isValTypeOperable(lval->valtype) || !isValTypeOperable(rval->valtype)) {
      Error("Unable to apply binary operand %s to %s , %s\n", TokenTypeToStr(bin->op),
            ValueTypeToStr(lval->valtype), ValueTypeToStr(rval->valtype));
      return nullptr;
    }

    ValueType finalvt;
    if (lval->valtype == VALTYPE_INTEGER && rval->valtype == VALTYPE_INTEGER) {
      finalvt = VALTYPE_INTEGER;
    } else {
      assert(lval->valtype == VALTYPE_INTEGER || lval->valtype == VALTYPE_FLOAT ||
             rval->valtype == VALTYPE_INTEGER || rval->valtype == VALTYPE_FLOAT);
      finalvt = VALTYPE_FLOAT;
    }
    ast_value* val = new (pool) ast_value;
    val->valtype = finalvt;
    val->is_hex = false;

    if (finalvt == VALTYPE_INTEGER) {
      switch (bin->op) {
        case TK_PLUS:
          val->int_val = getIntVal(lval) + getIntVal(rval);
          break;
        case TK_MINUS:
          val->int_val = getIntVal(lval) - getIntVal(rval);
          break;
        case TK_DIV:
          val->int_val = getIntVal(lval) / getIntVal(rval);
          break;
        case TK_STAR:
          val->int_val = getIntVal(lval) * getIntVal(rval);
          break;
        case TK_MOD:
          val->int_val = getIntVal(lval) % getIntVal(rval);
          break;
        default:
          Error("Binary operator %s not supported for integers\n", TokenTypeToStr(bin->op));
          return nullptr;
      }
    } else {
      switch (bin->op) {
        case TK_PLUS:
          val->float_val = getDoubleVal(lval) + getDoubleVal(rval);
          break;
        case TK_MINUS:
          val->float_val = getDoubleVal(lval) - getDoubleVal(rval);
          break;
        case TK_DIV:
          val->float_val = getDoubleVal(lval) / getDoubleVal(rval);
          break;
        case TK_STAR:
          val->float_val = getDoubleVal(lval) * getDoubleVal(rval);
          break;
        default:
          Error("Binary operator %s not supported for floating point\n", TokenTypeToStr(bin->op));
          return nullptr;
      }
    }
    return val;
  } else {
    Error("Unexpected expression type!\n");
    return nullptr;
  }
  Error("Unable to compute expression value: %zu", expr);
  return nullptr;
}

// Parses simple literals like numbers, strings, etc
ast_expression* Parser::parseSimpleLiteral() {
  Token t;
  lex->getNextToken(t);
  if (t.type == TK_IDENTIFIER) {
    Error("Identifiers are not allowed on expressions in cbuf");
    return nullptr;
  } else if ((t.type == TK_NUMBER) || (t.type == TK_TRUE) || (t.type == TK_FNUMBER) ||
             (t.type == TK_STRING) || (t.type == TK_FALSE)) {
    auto ex = new (pool) ast_value;

    if (t._is_hex) {
      Error("Hexadecimal values are not supported for initialization assignment\n");
      return nullptr;
    }

    if (t.type == TK_NUMBER) {
      ex->elemtype = TYPE_S64;
      ex->int_val = t._u64;
      ex->valtype = VALTYPE_INTEGER;
    } else if (t.type == TK_FNUMBER) {
      ex->elemtype = TYPE_F64;
      ex->float_val = t._f64;
      ex->valtype = VALTYPE_FLOAT;
    } else if (t.type == TK_STRING) {
      ex->elemtype = TYPE_STRING;
      ex->str_val = t.string;
      ex->valtype = VALTYPE_STRING;
    } else if (t.type == TK_TRUE) {
      ex->elemtype = TYPE_BOOL;
      ex->bool_val = true;
      ex->valtype = VALTYPE_BOOL;
    } else if (t.type == TK_FALSE) {
      ex->elemtype = TYPE_BOOL;
      ex->bool_val = false;
      ex->valtype = VALTYPE_BOOL;
    }
    return ex;
  } else if (t.type == TK_OPEN_PAREN) {
    SrcLocation loc;
    loc = t.loc;

    ast_expression* expr = parseExpression();
    if (!success) return nullptr;

    lex->getNextToken(t);
    if (t.type != TK_CLOSE_PAREN) {
      Error("Cound not find a matching close parentesis, open parenthesis was at %d:%d\n", loc.line, loc.col);
      if (!success) {
        return nullptr;
      }
    }
    return expr;
  } else if (t.type == TK_PERIOD) {
    // We support period for expressions like `.5`
    if (lex->checkAheadToken(TK_NUMBER, 1)) {
      lex->getNextToken(t);

      if (isHexNumber(t)) {
        Error("After a period we need to see normal numbers, not hex numbers");
        return nullptr;
      }

      auto ex = new (pool) ast_value;

      ex->elemtype = TYPE_F64;
      ex->float_val = computeDecimal(t);
      ex->valtype = VALTYPE_FLOAT;
      return ex;
    } else {
      Error("Could not parse a literal expression! Unknown token type: %s", TokenTypeToStr(t.type));
      return nullptr;
    }
  }
  Error("Could not parse a literal expression! Unknown token type: %s", TokenTypeToStr(t.type));
  return nullptr;
}

ast_expression* Parser::parseLiteral() {
  Token t;
  lex->lookaheadToken(t);
  // first check if it's an array literal then we will recurse in further
  if (t.type == TK_OPEN_BRACKET) {
    auto expr = parseArrayExpression();
    if (expr == nullptr) {
      printf("Error parsing array expression failed");
    }
    return expr;
  }
  // if we are here it's not an array literal and so we attempt to parse a simple literal
  return parseSimpleLiteral();
}

ast_expression* Parser::parseUnaryExpression() {
  Token t;
  lex->lookaheadToken(t);

  if (isUnaryPrefixOperator(t.type)) {
    lex->consumeToken();
    ast_expression* expr = parseUnaryExpression();
    if (!success) return nullptr;
    ast_unaryexp* un = new (pool) ast_unaryexp;
    un->expr = expr;
    un->op = t.type;
    return un;
  }
  return parseLiteral();
}

ast_expression* Parser::parseBinOpExpressionRecursive(u32 oldprec, ast_expression* lhs) {
  TOKEN_TYPE cur_type;

  while (1) {
    cur_type = lex->getTokenType();
    if (isBinOperator(cur_type)) {
      u32 cur_prec = getPrecedence(cur_type);
      if (cur_prec < oldprec) {
        return lhs;
      } else {
        lex->consumeToken();
        ast_expression* rhs = parseUnaryExpression();
        if (isBinOperator(lex->getTokenType())) {
          u32 newprec = getPrecedence(lex->getTokenType());
          if (cur_prec < newprec) {
            rhs = parseBinOpExpressionRecursive(cur_prec + 1, rhs);
          }
        }
        ast_binaryexp* bin = new (pool) ast_binaryexp;
        bin->lhs = lhs;
        bin->rhs = rhs;
        bin->op = cur_type;
        lhs = bin;
      }
    } else {
      break;
    }
  }
  return lhs;
}

ast_expression* Parser::parseBinOpExpression() {
  ast_expression* lhs = parseUnaryExpression();
  return parseBinOpExpressionRecursive(0, lhs);
}

ast_expression* Parser::parseExpression() { return parseBinOpExpression(); }

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
  ast_struct* nst = new (pool) ast_struct();
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
  ast_enum* en = new (pool) ast_enum();
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
      if (!strcmp(el.item_name, t.string)) {
        found = true;
        break;
      }
    }
    if (found) {
      Error("Found duplicate identifier inside enum: %s\n", t.string);
    }
    enum_item item;
    item.item_name = t.string;

    lex->consumeToken();
    lex->lookaheadToken(t);
    if (t.type == TK_ASSIGN) {
      lex->consumeToken();
      auto* expr = parseExpression();
      if (!success) return nullptr;

      ast_value* val = nullptr;
      if (!(val = computeExpressionValue(expr))) {
        return nullptr;
      }

      if (val->valtype != VALTYPE_INTEGER) {
        Error("Only integers numbers can be used for enums, found %s\n", TokenTypeToStr(t.type));
        return nullptr;
      }

      item.item_assigned = true;
      item.item_value = val->int_val;
      lex->lookaheadToken(t);
    }
    if (!item.item_assigned && en->elements.size() > 0) {
      item.item_value = en->elements[en->elements.size() - 1].item_value + 1;
    }
    en->elements.push_back(item);

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

  ast_const* cst = new (pool) ast_const();
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
    sp = new (pool) ast_namespace();
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
