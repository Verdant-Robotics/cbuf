#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "Parser.h"
#include "SymbolTable.h"
#include "AstPrinter.h"
#include "CPrinter.h"
#include <inttypes.h>

void checkParsing(const char *filename)
{
    Lexer lex;
    Token tok;
    PoolAllocator pool;

    lex.setPoolAllocator(&pool);
    lex.openFile(filename);
    lex.parseFile();

    while(tok.type != TK_LAST_TOKEN) {
        lex.getNextToken(tok);
        tok.print();
//        printf("%s\n", TokenTypeToStr(tok.type));
    }
}

template< typename T >
void loop_all_structs(ast_global *ast, SymbolTable *symtable, T func)
{
    for(auto *sp: ast->spaces) {
        for(auto *st: sp->structs) {
            func(st, symtable);
        }
    }

    for(auto *st: ast->global_space.structs) {
        func(st, symtable);
    }
}

bool compute_simple(ast_struct *st, SymbolTable *symtable)
{
    if (st->simple_computed) return st->simple;
    st->simple = true;
    for (auto *elem: st->elements) {
        if (elem->type == TYPE_STRING) {
            st->simple = false;
            st->simple_computed = true;
            return false;
        }
        if (elem->type == TYPE_CUSTOM) {
            if (!symtable->find_symbol(elem->custom_name)) {
                fprintf(stderr, "Struct %s, element %s was referencing type %s and could not be found\n",
                    st->name, elem->name, elem->custom_name);
                exit(-1);
            }
            auto *inner_st = symtable->find_struct(elem->custom_name);
            if (inner_st == nullptr) {
                // Must be an enum, it is simple
                continue;
            }
            bool elem_simple = compute_simple(inner_st, symtable);
            if (!elem_simple) {
                st->simple = false;
                st->simple_computed = true;
                return false;
            }
        }
    }
    st->simple_computed = true;
    return true;
}

u64 hash(const unsigned char *str)
{
    u64 hash = 5381;
    int c;

    while ((c = *str++))
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */

    return hash;
}

bool compute_hash(ast_struct *st, SymbolTable *symtable) 
{
    StringBuffer buf;
    AstPrinter printer;
    if (st->hash_computed) return true;

    for(auto *elem: st->elements) {
        if (elem->type == TYPE_CUSTOM) {
            auto *inner_st = symtable->find_struct(elem->custom_name);
            if (!inner_st) {
                printer.print_ast(&buf, elem);
                continue;
            }
            assert(inner_st);
            bool bret = compute_hash(inner_st, symtable);
            if (!bret) return false;
            buf.print("%" PRIX64 " ", inner_st->hash_value);
        } else {
            printer.print_ast(&buf, elem);
        }
    }

    st->hash_value = hash((const unsigned char*)buf.get_buffer());
    st->hash_computed = true;
    return true;
}

template<class T>
void print(T elem)
{
    printf("%d", elem);
}

template<>
void print<u64>(u64 elem)
{
    printf("%" PRIu64, elem);
}

template<>
void print<s64>(s64 elem)
{
    printf("%" PRId64, elem);
}

template<>
void print<f64>(f64 elem)
{
    printf("%f", elem);
}

template<>
void print<f32>(f32 elem)
{
    printf("%f", elem);
}

template<class T>
void process_element(const ast_element* elem, u8* &bin_buffer, u64 &buf_size)
{
    T val;
    if (elem->array_suffix) {
        // This is an array
        u32 array_size;
        if (elem->is_dynamic_array) {
            // This is a dynamic array
            array_size = *(u32 *)bin_buffer;
            bin_buffer += sizeof(array_size);
            buf_size -= sizeof(array_size);
        } else {
            // this is a static array
            array_size = elem->array_suffix->size;
        }
        if (array_size > 5) {
            printf("%s[%d] = ...\n", elem->name, array_size);
            bin_buffer += sizeof(val)*array_size;
            buf_size -= sizeof(val)*array_size;
        } else {
            printf("%s[%d] = ", elem->name, array_size);
            for(int i=0; i<array_size; i++) {
                val = *(T *)bin_buffer;
                bin_buffer += sizeof(val);
                buf_size -= sizeof(val);
                print(val);
                if (i<array_size-1) printf(", ");
            }
            printf("\n");
        }
    } else {
        // This is a single element, not an array
        val = *(T *)bin_buffer;
        bin_buffer += sizeof(val);
        buf_size -= sizeof(val);
        printf("%s: ", elem->name);
        print(val); printf("\n");
    }
}

void process_element_string(const ast_element* elem, u8* &bin_buffer, u64 &buf_size)
{
    char *str;
    if (elem->array_suffix) {
        assert(false);
        // Array of strings not implemented
    }
        // This is an array
    u32 str_size;
    // This is a dynamic array
    str_size = *(u32 *)bin_buffer;
    bin_buffer += sizeof(str_size);
    buf_size -= sizeof(str_size);
    str = new char[str_size+1];
    memset(str, 0, str_size+1);
    memcpy(str, bin_buffer, str_size);
    bin_buffer += str_size;
    buf_size -= str_size;

    printf("%s = [ %s ]\n", elem->name, str);
    delete[] str;
}

void parseBinBuffer(ast_global *ast, SymbolTable *sym, const char *st_name, u8* &bin_buffer, u64 &buf_size)
{
    auto tname = CreateTextType(getMallocAllocator(), st_name);
    ast_struct *st = sym->find_struct(tname);

    // All structs have a preamble, skip it
    u32 sizeof_preamble = 12; // 8 bytes hash, 4 bytes size
    bin_buffer += sizeof_preamble;
    buf_size -= sizeof_preamble;

    for(auto& elem: st->elements) {
        switch(elem->type) {
        case TYPE_U8:  { process_element< u8>(elem, bin_buffer, buf_size); break; }
        case TYPE_U16: { process_element<u16>(elem, bin_buffer, buf_size); break; }
        case TYPE_U32: { process_element<u32>(elem, bin_buffer, buf_size); break; }
        case TYPE_U64: { process_element<u64>(elem, bin_buffer, buf_size); break; }
        case TYPE_S8:  { process_element< s8>(elem, bin_buffer, buf_size); break; }
        case TYPE_S16: { process_element<s16>(elem, bin_buffer, buf_size); break; }
        case TYPE_S32: { process_element<s32>(elem, bin_buffer, buf_size); break; }
        case TYPE_S64: { process_element<s64>(elem, bin_buffer, buf_size); break; }
        case TYPE_F32: { process_element<f32>(elem, bin_buffer, buf_size); break; }
        case TYPE_F64: { process_element<f64>(elem, bin_buffer, buf_size); break; }
        case TYPE_BOOL:{ process_element< u8>(elem, bin_buffer, buf_size); break; }
        case TYPE_STRING:
        {
            process_element_string(elem, bin_buffer, buf_size);
            break;
        }
        case TYPE_CUSTOM:
        {
            auto *inst = sym->find_struct(elem->custom_name);
            if (inst != nullptr) {
                parseBinBuffer(ast, sym, elem->custom_name, bin_buffer, buf_size);
            } else {
                auto *enm = sym->find_enum(elem->custom_name);
                if (enm == nullptr) {
                    fprintf(stderr, "Enum %s could not be parsed\n", elem->custom_name);
                    return;
                }
                process_element<u32>(elem, bin_buffer, buf_size);
            }
            break;
        }

        }
    }
    free(tname);
}

void test_dynamic()
{
    Parser parser;
    auto *pool = getMallocAllocator();
    const char meta_str[] = R"(struct mystruct { u32 a; u32 b[3]; })";
    auto top_ast = parser.ParseBuffer(meta_str, sizeof(meta_str)-1, pool);
    if (!parser.success) {
        fprintf(stderr, "Error during parsing:\n%s\n", parser.getErrorString());
        return;
    }

    SymbolTable symtable;
    bool bret = symtable.initialize(top_ast);
    if (!bret) {
        fprintf(stderr, "Error during symbol table parsing:\n%s\n", parser.getErrorString());
        return;
    }

    loop_all_structs(top_ast, &symtable, compute_simple);

    u32 bin_buffer[] = {0xAABBCCDD, 0xAABBCCDD, 20, 5, 8, 65, 23};
    u8 *buf = (u8 *)bin_buffer;
    parseBinBuffer(top_ast, &symtable, "mystruct", buf, sizeof(bin_buffer));
}

int main(int argc, char **argv)
{
    Parser parser;
    PoolAllocator pool;

    if (argc < 2) {
        test_dynamic();
        fprintf(stderr, "Please provide a cbuf file\n");
        exit(0);
    }

    // checkParsing(argv[1]);

    auto top_ast = parser.Parse(argv[1], &pool);
    if (!parser.success) {
        fprintf(stderr, "Error during parsing:\n%s\n", parser.getErrorString());
        return -1;
    }

    SymbolTable symtable;
    bool bret = symtable.initialize(top_ast);
    if (!bret) return -1;

    loop_all_structs(top_ast, &symtable, compute_simple);
    loop_all_structs(top_ast, &symtable, compute_hash);
/*
    AstPrinter printer;
    StringBuffer buf;
    printer.print_ast(&buf, top_ast);
    printf("%s", buf.get_buffer());
*/

    CPrinter printer;
    StringBuffer buf;
    printer.print(&buf, top_ast, &symtable);
    printf("%s", buf.get_buffer());
    return 0;
}
