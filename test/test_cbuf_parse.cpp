/*
 * This file will test the parsing of a cbuf file which contain various
 * types of data.
 * Author: Kartik Arcot
 */

#include <gtest/gtest.h>

#include <string>

#include "AstPrinter.h"
#include "CPrinter.h"
#include "Interp.h"
#include "Parser.h"
#include "ast.h"

static void PrintAst(ast_global* top_ast) {
  AstPrinter printer;
  StdStringBuffer buf;
  printer.print_ast(&buf, top_ast);
  printf("%s", buf.get_buffer());
}

TEST(CBufParser, ParseStructWithLiterals) {
  // Create an R string with the cbuf file
  constexpr const char* simple_cbuf = R"(
  namespace test {
    struct silly {
      int a = 10;
      int b = 20;
      int c = 30;
    }
  }
  )";
  Interp interp;
  Parser parser;
  parser.interp = &interp;
  PoolAllocator pool;
  auto ast = parser.ParseBuffer(simple_cbuf, std::string::traits_type::length(simple_cbuf), &pool, nullptr);
  EXPECT_TRUE(parser.success);
  ASSERT_TRUE(ast != nullptr);
  PrintAst(ast);
}

TEST(CBufParser, ParseStructWithArrayInit) {
  // A test for parsing a struct with an array initializer
  constexpr const char* simple_cbuf = R"(
  namespace test {
    struct silly {
      int a = 10;
      int b = 20;
      int c = 30;
      int d[3] = {5,4,6};
    }
  }
  )";
  Interp interp;
  Parser parser;
  parser.interp = &interp;
  PoolAllocator pool;
  auto ast = parser.ParseBuffer(simple_cbuf, std::string::traits_type::length(simple_cbuf), &pool, nullptr);
  EXPECT_TRUE(parser.success);
  printf("%s", interp.getErrorString());
  ASSERT_TRUE(ast != nullptr);
  PrintAst(ast);
}

TEST(CBufParser, ParseStructWithArrayInitFail) {
  // A test for parsing a struct with an array initializer
  constexpr const char* simple_cbuf = R"(
  namespace test {
    struct silly {
      int a = 10;
      int b = 20;
      int c = 30;
      int d[10] = {5,4,"6"};
    }
  }
  )";
  Interp interp;
  Parser parser;
  parser.interp = &interp;
  PoolAllocator pool;
  auto ast = parser.ParseBuffer(simple_cbuf, std::string::traits_type::length(simple_cbuf), &pool, nullptr);
  EXPECT_FALSE(parser.success);
  EXPECT_TRUE(ast == nullptr);
}

TEST(CBufParser, ParseStructWithDynamicArrayInit) {
  // A test for parsing a struct with an dynamic array initializer
  constexpr const char* simple_cbuf = R"(
  namespace test {
    struct silly {
      int a = 10;
      int b = 20;
      int c = 30;
      int d[] = {5,4,6};
    }
  }
  )";
  Interp interp;
  Parser parser;
  parser.interp = &interp;
  PoolAllocator pool;
  auto ast = parser.ParseBuffer(simple_cbuf, std::string::traits_type::length(simple_cbuf), &pool, nullptr);
  EXPECT_TRUE(parser.success);
  ASSERT_TRUE(ast != nullptr);
  PrintAst(ast);
}

TEST(CBufParser, ParseStructWithArrayInitFailMoreElements) {
  // A test for parsing a struct with an array initializer
  // the list has more elements than the array size
  constexpr const char* simple_cbuf = R"(
  namespace test {
    struct silly {
      int a = 10;
      int b = 20;
      int c = 30;
      int d[2] = {5,4,6,7};
    }
  }
  )";
  Interp interp;
  Parser parser;
  parser.interp = &interp;
  PoolAllocator pool;
  auto ast = parser.ParseBuffer(simple_cbuf, std::string::traits_type::length(simple_cbuf), &pool, nullptr);
  EXPECT_FALSE(parser.success);
  EXPECT_TRUE(ast == nullptr);
}

TEST(CBufParser, ParseStructWithArrayInitFailComma) {
  // A test where the initialiser list has a comma at the end
  constexpr const char* simple_cbuf2 = R"(
  namespace test {
    struct silly {
      int a = 10;
      int b = 20;
      int c = 30;
      int d[3] = {5,4,6,};
    }
  }
  )";
  Interp interp;
  Parser parser;
  parser.interp = &interp;
  PoolAllocator pool;
  auto ast = parser.ParseBuffer(simple_cbuf2, std::string::traits_type::length(simple_cbuf2), &pool, nullptr);
  EXPECT_FALSE(parser.success);
  EXPECT_TRUE(ast == nullptr);
}

TEST(CBufParser, ParseArrayOfStrings) {
  // A test where the initialiser list has a comma at the end
  constexpr const char* simple_cbuf2 = R"(
  namespace test {
    struct silly {
      int a = 10;
      int b = 20;
      int c = 30;
      string d[3] = {"hello", "world", "!"};
    }
  }
  )";
  Interp interp;
  Parser parser;
  parser.interp = &interp;
  PoolAllocator pool;
  auto ast = parser.ParseBuffer(simple_cbuf2, std::string::traits_type::length(simple_cbuf2), &pool, nullptr);
  EXPECT_TRUE(parser.success);
  ASSERT_TRUE(ast != nullptr);
  PrintAst(ast);
  EXPECT_EQ(ast->spaces[0]->structs[0]->elements.size(), 4);
  EXPECT_EQ(std::string(ast->spaces[0]->structs[0]->elements[0]->name), "a");
  EXPECT_EQ(std::string(ast->spaces[0]->structs[0]->elements[1]->name), "b");
  EXPECT_EQ(std::string(ast->spaces[0]->structs[0]->elements[2]->name), "c");
  EXPECT_EQ(std::string(ast->spaces[0]->structs[0]->elements[3]->name), "d");
  std::vector<std::string> expected = {"hello", "world", "!"};
  auto arr_val = static_cast<ast_array_value*>(ast->spaces[0]->structs[0]->elements[3]->init_value);
  for (int i = 0; i < 3; i++) {
    EXPECT_EQ(std::string(arr_val->values[i]->str_val), expected[i]);
  }
}

TEST(CBufParser, ParseArrayWithExpressions) {
  // A test where the initialiser list has a comma at the end
  constexpr const char* simple_cbuf2 = R"(
  namespace test {
    struct silly {
      int a = 10;
      int b = 20;
      int c = 30;
      int d[3] = {10 + 20, 30 - (50 * (12 / 6)), 97};
    }
  }
  )";
  Interp interp;
  Parser parser;
  parser.interp = &interp;
  PoolAllocator pool;
  auto ast = parser.ParseBuffer(simple_cbuf2, std::string::traits_type::length(simple_cbuf2), &pool, nullptr);
  EXPECT_TRUE(parser.success);
  if (!parser.success) {
    printf("%s\n", interp.getErrorString());
  }
  ASSERT_TRUE(ast != nullptr);
  EXPECT_EQ(ast->spaces[0]->structs[0]->elements.size(), 4);
  EXPECT_EQ(std::string(ast->spaces[0]->structs[0]->elements[0]->name), "a");
  EXPECT_EQ(std::string(ast->spaces[0]->structs[0]->elements[1]->name), "b");
  EXPECT_EQ(std::string(ast->spaces[0]->structs[0]->elements[2]->name), "c");
  EXPECT_EQ(std::string(ast->spaces[0]->structs[0]->elements[3]->name), "d");
  std::vector<int> expected = {30, -70, 97};
  auto arr_val = static_cast<ast_array_value*>(ast->spaces[0]->structs[0]->elements[3]->init_value);
  for (int i = 0; i < 3; i++) {
    auto val = arr_val->values[i];
    EXPECT_EQ(val->valtype, VALTYPE_INTEGER);
    EXPECT_EQ(val->int_val, expected[i]);
  }
  PrintAst(ast);
}

TEST(CBufParser, FailArrayInitialisationForStructs) {
  // A test where the initialiser list has a comma at the end
  constexpr const char* simple_cbuf2 = R"(
  namespace test {
    struct silly {
      int a = 10;
      int b = 20;
      int c = 30;
    }
    struct silly2 {
      silly d[3] = {{10, 20, 30}, {40, 50, 60}, {70, 80, 90}};
    }
  }
  )";
  Interp interp;
  Parser parser;
  parser.interp = &interp;
  PoolAllocator pool;
  auto ast = parser.ParseBuffer(simple_cbuf2, std::string::traits_type::length(simple_cbuf2), &pool, nullptr);
  if (!parser.success) printf("Error: %s", interp.getErrorString());
  EXPECT_FALSE(parser.success);
  ASSERT_TRUE(ast == nullptr);
}

TEST(CBufParser, ParseFailWhenInvalidAssignment) {
  // parsing should fail when a non array type is assigned
  // an array type
  constexpr const char* simple_cbuf2 = R"(
    namespace test {
      struct silly {
        int a = 10;
        int b = 20;
        int c = 30;
        int d = {10 + 20, 30 - (50 * (12 / 6)), 97};
        int e = d;
      }
    }
    )";
  Interp interp;
  Parser parser;
  parser.interp = &interp;
  PoolAllocator pool;
  auto ast = parser.ParseBuffer(simple_cbuf2, std::string::traits_type::length(simple_cbuf2), &pool, nullptr);
  if (!parser.success) printf("Error: %s", interp.getErrorString());
  EXPECT_FALSE(parser.success);
  EXPECT_TRUE(ast == nullptr);
}

TEST(CPrinter, ParseAndPrintIntArrayInitialisation) {
  // A test where the initialiser list has a comma at the end
  constexpr const char* simple_cbuf2 = R"(
  namespace test {
    struct silly {
      int d[3] = {10 + 20, 30 - (50 * (12 / 6)), 97};
    }
  }
  )";
  Interp interp;
  Parser parser;
  parser.interp = &interp;
  PoolAllocator pool;
  auto ast = parser.ParseBuffer(simple_cbuf2, std::string::traits_type::length(simple_cbuf2), &pool, nullptr);
  EXPECT_TRUE(parser.success);
  EXPECT_TRUE(ast != nullptr);
  PrintAst(ast);
  SymbolTable symtable;
  bool bret = symtable.initialize(ast);
  EXPECT_TRUE(bret);

  CPrinter printer;
  StdStringBuffer buf;
  printer.print(&buf, ast, &symtable);
  std::string expected = "int32_t d[3] = {30, -70, 97};";
  // printf("buf:\n%s", buf.get_buffer());
  // expect to find this in the buffer
  EXPECT_TRUE(std::string(buf.get_buffer()).find(expected) != std::string::npos);
}

TEST(CPrinter, ParseAndPrintDynamicIntArrayInitialisation) {
  // A test where the initialiser list has a comma at the end
  constexpr const char* simple_cbuf2 = R"(
  namespace test {
    struct silly {
      int d[] = {10 + 20, 30 - (50 * (12 / 6)), 97};
    }
  }
  )";
  Interp interp;
  Parser parser;
  parser.interp = &interp;
  PoolAllocator pool;
  auto ast = parser.ParseBuffer(simple_cbuf2, std::string::traits_type::length(simple_cbuf2), &pool, nullptr);
  EXPECT_TRUE(parser.success);
  EXPECT_TRUE(ast != nullptr);
  PrintAst(ast);
  SymbolTable symtable;
  bool bret = symtable.initialize(ast);
  EXPECT_TRUE(bret);

  CPrinter printer;
  StdStringBuffer buf;
  printer.print(&buf, ast, &symtable);
  std::string expected = "std::vector< int32_t > d = {30, -70, 97};";
  // printf("buf:\n%s", buf.get_buffer());
  // expect to find this in the buffer
  EXPECT_TRUE(std::string(buf.get_buffer()).find(expected) != std::string::npos);
}

TEST(CPrinter, ParseAndPrintStringArrayInitialisation) {
  // A test where the initialiser list has a comma at the end
  constexpr const char* simple_cbuf2 = R"(
  namespace test {
    struct silly {
      string d[3] = {"hello", "world", "!"};
    }
  }
  )";
  Interp interp;
  Parser parser;
  parser.interp = &interp;
  PoolAllocator pool;
  auto ast = parser.ParseBuffer(simple_cbuf2, std::string::traits_type::length(simple_cbuf2), &pool, nullptr);
  EXPECT_TRUE(parser.success);
  EXPECT_TRUE(ast != nullptr);
  PrintAst(ast);
  SymbolTable symtable;
  bool bret = symtable.initialize(ast);
  EXPECT_TRUE(bret);

  CPrinter printer;
  StdStringBuffer buf;
  printer.print(&buf, ast, &symtable);
  std::string expected = "std::string d[3] = {\"hello\", \"world\", \"!\"};";
  // printf("buf:\n%s", buf.get_buffer());
  // expect to find this in the buffer
  EXPECT_TRUE(std::string(buf.get_buffer()).find(expected) != std::string::npos);
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
