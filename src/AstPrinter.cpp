#include "AstPrinter.h"

#include <string.h>

#include "ast.h"

// clang-format off
static const char* ElementTypeToStr[] = {
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
  "string",
  "short_string",
  "bool"
};
// clang-format on

AstPrinter::AstPrinter() {}

AstPrinter::~AstPrinter() {}

static void PrintAstValue(const ast_value* val, StdStringBuffer* buffer) {
  switch (val->valtype) {
    case VALTYPE_INTEGER:
      buffer->print_no("%zd", ssize_t(val->int_val));
      break;
    case VALTYPE_FLOAT:
      buffer->print_no("%f", val->float_val);
      break;
    case VALTYPE_BOOL:
      buffer->print_no("%s", val->bool_val ? "true" : "false");
      break;
    case VALTYPE_STRING:
      buffer->print_no("\"%s\"", val->str_val);
      break;
    case VALTYPE_ARRAY: {
      buffer->print_no("{");
      auto arr_val = static_cast<const ast_array_value*>(val);
      for (auto v = arr_val->values.begin(); v != arr_val->values.end(); ++v) {
        PrintAstValue(*v, buffer);
        if (std::next(v) != arr_val->values.end()) {
          buffer->print_no(", ");
        }
      }
      buffer->print_no("}");
      break;
    }
    case VALTYPE_IDENTIFIER:
      buffer->print_no("%s", val->str_val);
      break;
    default:
      assert(false && "Unknown value type in PrintAstValue");
      break;
  }
}

void AstPrinter::print_elem(ast_element* elem) {
  ast_array_definition* ar = elem->array_suffix;

  if (elem->custom_name) {
    if (elem->namespace_name)
      buffer->print("%s::%s ", elem->namespace_name, elem->custom_name);
    else
      buffer->print("%s ", elem->custom_name);
    if (sym) {
      auto* struc = sym->find_struct(elem);
      if (struc && !printed_types[struc->space]) {
        printed_types[struc->space] = 1;
        // Print the new namespace
        StdStringBuffer new_buf;
        StdStringBuffer* old_buf;
        old_buf = buffer;
        buffer = &new_buf;
        print_namespace(struc->space);
        buffer = old_buf;
        // Append it to our buffer
        buffer->prepend(&new_buf);
      }
      auto* enm = sym->find_enum(elem);
      if (enm && !printed_types[enm->space]) {
        printed_types[enm->space] = 1;
        // Print the new namespace
        StdStringBuffer new_buf;
        StdStringBuffer* old_buf;
        old_buf = buffer;
        buffer = &new_buf;
        print_namespace(enm->space);
        buffer = old_buf;
        // Append it to our buffer
        buffer->prepend(&new_buf);
      }
    }
  } else {
    buffer->print("%s ", ElementTypeToStr[elem->type]);
  }
  buffer->print_no("%s", elem->name);

  while (ar != nullptr) {
    if (ar->size != 0)
      buffer->print_no("[%" PRIu64 "]", ar->size);
    else
      buffer->print_no("[]");
    ar = ar->next;
  }
  if (elem->init_value != nullptr) {
    auto& val = elem->init_value;
    buffer->print_no(" = ");
    PrintAstValue(val, buffer);
  }
  if (elem->is_compact_array) buffer->print_no(" @compact");
  buffer->print_no(";\n");
}

void AstPrinter::print_enum(ast_enum* enm) {
  printed_types[enm] = 1;
  buffer->print("enum %s{\n", enm->name);
  buffer->increase_ident();
  for (auto& el : enm->elements) {
    if (el.item_assigned) {
      buffer->print("%s = %zd,\n", el.item_name, ssize_t(el.item_value));
    } else {
      buffer->print("%s,\n", el.item_name);
    }
  }
  buffer->decrease_ident();
  buffer->print("}\n");
}

void AstPrinter::print_struct(ast_struct* st) {
  printed_types[st] = 1;
  buffer->print("struct %s %s{\n", st->name, st->naked ? "@naked " : "");
  buffer->increase_ident();
  for (auto* elem : st->elements) {
    print_elem(elem);
  }
  buffer->decrease_ident();
  buffer->print("}\n");
}

void AstPrinter::print_namespace(ast_namespace* sp) {
  printed_types[sp] = 1;
  if (strcmp(sp->name, GLOBAL_NAMESPACE)) {
    buffer->print("namespace %s {\n", sp->name);
    buffer->increase_ident();
  }
  for (auto* enm : sp->enums) {
    print_enum(enm);
  }
  for (auto* st : sp->structs) {
    print_struct(st);
  }
  if (strcmp(sp->name, GLOBAL_NAMESPACE)) {
    buffer->decrease_ident();
    buffer->print("}\n\n");
  } else {
    buffer->print("\n");
  }
}

void AstPrinter::print_ast(StdStringBuffer* buf, ast_global* ast) {
  buffer = buf;
  for (auto* sp : ast->spaces) {
    print_namespace(sp);
  }

  print_namespace(&ast->global_space);

  buffer = nullptr;
}

void AstPrinter::print_ast(StdStringBuffer* buf, ast_struct* ast) {
  buffer = buf;
  printed_types.clear();

  print_namespace(ast->space);

  buffer = nullptr;
  printed_types.clear();
}

void AstPrinter::print_ast(StdStringBuffer* buf, ast_element* elem) {
  buffer = buf;
  printed_types.clear();
  print_namespace(elem->enclosing_struct->space);
  buffer = nullptr;
  printed_types.clear();
}
