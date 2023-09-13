#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "CPrinter.h"
#include "Interp.h"
#include "Parser.h"
#include "SymbolTable.h"
#include "computefuncs.h"
#include "fileutils.h"

const char* get_str_for_elem_type(ElementType t);

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

void usage() {
  printf("cbuf compiler\n");
  printf("  Usage: cbuf [OPTIONS] <input file>\n");
  printf("\n");
  printf("  Options:\n");
  printf("  -o <output file>  : designate which file to write the output to, otherwise\n");
  printf("  -I <include path> : path where to look for import files\n");
  printf("  -j <json file>    : designate which file to write the json output to, or skipped\n");
  printf("  -d <depfile>      : path for depfile, to be used with build system dependencies\n");
  printf("  -h                : show this help\n");
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
    } else if ((argv[i][0] == '-') && (argv[i][1] == 'd')) {
      if (i + 1 == argc) {
        fprintf(stderr, "The -d option needs a filename after it\n");
        return false;
      }
      args.depfile = argv[i + 1];
      i++;
    } else if ((argv[i][0] == '-') && (argv[i][1] == 'h')) {
      args.help = true;
    } else {
      if (args.srcfile == nullptr) {
        args.srcfile = argv[i];
      } else {
        fprintf(stderr, "Two input files are not supported\n");
        return false;
      }
    }
  }

  if (args.srcfile == nullptr) {
    fprintf(stderr, "Please provide a cbuf file, one is mandatory\n");
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

  if (!parseArgs(args, argc, argv) || args.help) {
    usage();
    exit(args.help ? 0 : -1);
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

  if (args.depfile != nullptr) {
    StdStringBuffer deptext;

    if (args.outfile == nullptr) {
      fprintf(stderr, "Please provide an outfile when using depfiles\n");
      return -1;
    }
    const std::string srcFile = getCanonicalPath(args.srcfile);
    const std::string dstFile = getCanonicalPath(args.outfile);

    std::string error;
    if (!printer.printDepfile(&deptext, top_ast, args.incs, error, srcFile.c_str(), dstFile.c_str())) {
      fprintf(stderr, "%s\n", error.c_str());
      return -1;
    }
    FILE* f = fopen(args.depfile, "w");
    if (f == nullptr) {
      fprintf(stderr, "File %s could not be opened for writing\n", args.depfile);
      return -1;
    }
    fprintf(f, "%s", deptext.get_buffer());
    fclose(f);
  }

  return 0;
}
