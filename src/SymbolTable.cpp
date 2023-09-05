#include "SymbolTable.h"

#include <string.h>

/*
bool SymbolTable::add_struct(ast_struct *st)
{
    if (!find_symbol(st->name)) {
        ar.push_back(st);
        return true;
    }
    return false;
}

bool SymbolTable::add_enum(ast_enum *en)
{
    auto *sp = find_namespace(en->space->name);
    if (sp == nullptr) {
        assert(!"We should never add an enum before its namespace");
        return false;
    }
    if (!find_symbol(en->name, en->space->name)) {
        sp->enums.push_back(en);
        return true;
    }
    return false;
}
*/

bool SymbolTable::add_namespace(ast_namespace* sp) {
  if (!find_namespace(sp->name)) {
    spaces.push_back(sp);
    return true;
  }
  return false;
}

ast_namespace* SymbolTable::find_namespace(TextType name) const {
  for (auto* sp : spaces) {
    if (!strcmp(name, sp->name)) {
      return sp;
    }
  }
  return nullptr;
}

bool SymbolTable::find_symbol(const TextType name, const TextType current_nspace) const {
  ast_namespace* sp = nullptr;
  if (current_nspace == nullptr) {
    sp = find_namespace(global_namespace_name);
  } else {
    sp = find_namespace(current_nspace);
  }
  if (sp == nullptr) {
    return false;
  }
  for (auto* st : sp->structs) {
    if (!strcmp(name, st->name)) {
      return true;
    }
  }
  for (auto* en : sp->enums) {
    if (!strcmp(name, en->name)) {
      return true;
    }
  }

  if (current_nspace != nullptr) {
    // fallback to search on the global namespace
    return find_symbol(name, nullptr);
  }
  return false;
}

ast_struct* SymbolTable::find_struct(const TextType name, const TextType current_nspace) const {
  ast_namespace* sp = nullptr;
  if (current_nspace == nullptr) {
    sp = find_namespace(global_namespace_name);
  } else {
    sp = find_namespace(current_nspace);
  }
  if (sp == nullptr) {
    return nullptr;
  }
  for (auto* st : sp->structs) {
    if (!strcmp(name, st->name)) {
      return st;
    }
  }

  if (current_nspace != nullptr) {
    return find_struct(name, nullptr);
  }
  return nullptr;
}

ast_enum* SymbolTable::find_enum(const TextType name, const TextType current_nspace) const {
  ast_namespace* sp = nullptr;
  if (current_nspace == nullptr) {
    sp = find_namespace(global_namespace_name);
  } else {
    sp = find_namespace(current_nspace);
  }
  if (sp == nullptr) {
    return nullptr;
  }
  for (auto* en : sp->enums) {
    if (!strcmp(name, en->name)) {
      return en;
    }
  }
  if (current_nspace != nullptr) {
    return find_enum(name, nullptr);
  }
  return nullptr;
}

bool SymbolTable::find_symbol(const ast_element* elem) const {
  TextType spname = elem->enclosing_struct->space->name;
  if (elem->namespace_name != nullptr) spname = elem->namespace_name;
  return find_symbol(elem->custom_name, spname);
}

ast_struct* SymbolTable::find_struct(const ast_element* elem) const {
  TextType spname = elem->enclosing_struct->space->name;
  if (elem->namespace_name != nullptr) spname = elem->namespace_name;
  return find_struct(elem->custom_name, spname);
}

ast_enum* SymbolTable::find_enum(const ast_element* elem) const {
  TextType spname = elem->enclosing_struct->space->name;
  if (elem->namespace_name != nullptr) spname = elem->namespace_name;
  return find_enum(elem->custom_name, spname);
}

bool SymbolTable::initialize(ast_global* top_ast) {
  global_namespace_name = top_ast->global_space.name;
  add_namespace(&top_ast->global_space);
  /*
      for(auto *st: top_ast->global_space.structs) {
          bool bret = add_struct(st);
          if (!bret) {
              fprintf(stderr, "Found repeated struct name: %s\n", st->name);
              return false;
          }
      }
      for(auto *en: top_ast->global_space.enums) {
          bool bret = add_enum(en);
          if (!bret) {
              fprintf(stderr, "Found repeated enum name: %s\n", en->name);
              return false;
          }
      }
  */
  for (auto* sp : top_ast->spaces) {
    add_namespace(sp);
    /*
            for (auto *st: sp->structs) {
                bool bret = add_struct(st);
                if (!bret) {
                    fprintf(stderr, "Found repeated struct name: %s\n", st->name);
                    return false;
                }
            }
            for(auto *en: sp->enums) {
                bool bret = add_enum(en);
                if (!bret) {
                    fprintf(stderr, "Found repeated enum name: %s\n", en->name);
                    return false;
                }
            }
    */
  }
  return true;
}
