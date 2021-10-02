#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "AstPrinter.h"
#include "CPrinter.h"
#include "Interp.h"
#include "Parser.h"
#include "SymbolTable.h"

const char* get_str_for_elem_type(ElementType t);

// Implemented in CBufParser.cpp
bool compute_simple(ast_struct* st, SymbolTable* symtable, Interp* interp);

void checkParsing(const char* filename) {
  Lexer lex;
  Token tok;
  PoolAllocator pool;

  lex.setPoolAllocator(&pool);
  lex.openFile(filename);
  lex.parseFile();

  while (tok.type != TK_LAST_TOKEN) {
    lex.getNextToken(tok);
    tok.print();
    //        printf("%s\n", TokenTypeToStr(tok.type));
  }
}

template <typename T>
void loop_all_structs(ast_global* ast, SymbolTable* symtable, Interp* interp, T func) {
  for (auto* sp : ast->spaces) {
    for (auto* st : sp->structs) {
      func(st, symtable, interp);
    }
  }

  for (auto* st : ast->global_space.structs) {
    func(st, symtable, interp);
  }
}

bool compute_compact(ast_struct* st, SymbolTable* symtable, Interp* interp) {
  if (st->compact_computed) return st->has_compact;
  st->has_compact = false;
  for (auto* elem : st->elements) {
    if (elem->type == TYPE_STRING) {
      continue;
    }
    if (elem->is_compact_array) {
      st->has_compact = true;
      st->compact_computed = true;
      return true;
    }
    if (elem->type == TYPE_CUSTOM) {
      if (!symtable->find_symbol(elem)) {
        interp->Error(elem, "Struct %s, element %s was referencing type %s and could not be found\n",
                      st->name, elem->name, elem->custom_name);
        return false;
      }
      auto* inner_st = symtable->find_struct(elem);
      if (inner_st == nullptr) {
        // Must be an enum, it is simple
        continue;
      }
      compute_compact(inner_st, symtable, interp);
      if (inner_st->has_compact) {
        st->has_compact = true;
        st->simple_computed = true;
        return true;
      }
    }
  }
  st->compact_computed = true;
  return true;
}

u64 hash(const unsigned char* str) {
  u64 hash = 5381;
  int c;

  while ((c = *str++)) hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

  return hash;
}

bool compute_hash(ast_struct* st, SymbolTable* symtable, Interp* interp) {
  StdStringBuffer buf;
  if (st->hash_computed) return true;

  buf.print("struct ");
  if (strcmp(st->space->name, GLOBAL_NAMESPACE)) buf.print_no("%s::", st->space->name);
  buf.print("%s \n", st->name);
  for (auto* elem : st->elements) {
    if (elem->array_suffix) {
      buf.print("[%lu] ", elem->array_suffix->size);
    }
    if (elem->type == TYPE_CUSTOM) {
      auto* enm = symtable->find_enum(elem);
      if (enm != nullptr) {
        buf.print("%s %s;\n", elem->custom_name, elem->name);
        continue;
      }
      auto* inner_st = symtable->find_struct(elem);
      if (!inner_st) {
        interp->Error(elem, "Could not find this element for hash\n");
        return false;
      }
      assert(inner_st);
      bool bret = compute_hash(inner_st, symtable, interp);
      if (!bret) return false;
      buf.print("%" PRIX64 " %s;\n", inner_st->hash_value, elem->name);
    } else {
      buf.print("%s %s; \n", get_str_for_elem_type(elem->type), elem->name);
    }
  }

  st->hash_value = hash((const unsigned char*)buf.get_buffer());
  st->hash_computed = true;
  return true;
}

bool parseArgs(Args& args, int argc, char** argv) {
  for (int i = 1; i < argc; i++) {
    if ((argv[i][0] == '-') && (argv[i][1] == 'I')) {
      args.incs.push_back(&argv[i][2]);
    } else if ((argv[i][0] == '-') && (argv[i][1] == 'o')) {
      if (i + 1 == argc) {
        fprintf(stderr, "The -o option needs a filename after it\n");
        return false;
      }
      args.outfile = argv[i + 1];
      i++;
    } else if ((argv[i][0] == '-') && (argv[i][1] == 'j')) {
      if (i + 1 == argc) {
        fprintf(stderr, "The -j option needs a filename after it\n");
        return false;
      }
      args.jsonfile = argv[i + 1];
      i++;
    } else {
      if (args.srcfile == nullptr) {
        args.srcfile = argv[i];
      } else {
        fprintf(stderr, "Please provide a cbuf file\n");
        return false;
      }
    }
  }

  if (args.srcfile == nullptr) {
    fprintf(stderr, "Two input files are not supported\n");
    return false;
  }
  return true;
}

void getFilenameNE(const char* file, char* buf) {
  // find the last slash, to remove folders
  const char* name_start = file;
  const char* ptr = strchr(name_start, '/');
  while (ptr != nullptr) {
    name_start = ptr + 1;
    ptr = strchr(name_start, '/');
  }
  strcpy(buf, name_start);
  char* p = strstr(buf, ".cbuf");
  *p = 0;
}

int main(int argc, char** argv) {
  Args args;
  Interp interp;
  Parser parser;
  PoolAllocator pool;

  if (!parseArgs(args, argc, argv)) {
    exit(0);
  }

  parser.interp = &interp;
  parser.args = &args;

  // Debug statements
  // fprintf(stderr, "For %s:\n", args.srcfile);
  // for(auto f: args.incs) fprintf(stderr, "  Include folders: %s\n", f);

  auto top_ast = parser.Parse(args.srcfile, &pool);
  if (!parser.success) {
    assert(interp.has_error());
    fprintf(stderr, "Error during parsing:\n%s\n", interp.getErrorString());
    return -1;
  }

  SymbolTable symtable;
  bool bret = symtable.initialize(top_ast);
  if (!bret) return -1;

  loop_all_structs(top_ast, &symtable, &interp, compute_simple);
  if (interp.has_error()) {
    fprintf(stderr, "Error during variable checking:\n%s\n", interp.getErrorString());
    return -1;
  }
  loop_all_structs(top_ast, &symtable, &interp, compute_compact);
  if (interp.has_error()) {
    fprintf(stderr, "Error during variable checking:\n%s\n", interp.getErrorString());
    return -1;
  }
  loop_all_structs(top_ast, &symtable, &interp, compute_hash);
  /*
      AstPrinter printer;
      StdStringBuffer buf;
      printer.print_ast(&buf, top_ast);
      printf("%s", buf.get_buffer());
  */

  CPrinter printer;

  if (args.jsonfile != nullptr) {
    char c_name[256] = {};
    getFilenameNE(args.srcfile, c_name);
    StdStringBuffer jtext;
    printer.printLoader(&jtext, top_ast, &symtable, c_name);
    FILE* f = fopen(args.jsonfile, "w");
    if (f == nullptr) {
      fprintf(stderr, "File %s could not be opened for writing\n", args.jsonfile);
      return -1;
    }
    fprintf(f, "%s", jtext.get_buffer());
    fclose(f);
    return 0;
  }

  StdStringBuffer buf;
  printer.print(&buf, top_ast, &symtable);
  if (args.outfile != nullptr) {
    FILE* f = fopen(args.outfile, "w");
    if (f == nullptr) {
      fprintf(stderr, "File %s could not be opened for writing\n", args.outfile);
      return -1;
    }
    fprintf(f, "%s", buf.get_buffer());
    fclose(f);
  } else {
    printf("%s", buf.get_buffer());
  }

  return 0;
}
