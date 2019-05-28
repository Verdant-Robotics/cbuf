#pragma once
#include <stddef.h>

class StringBuffer
{
    char *buffer;
    char *end;
    int ident;
    size_t buf_size;
    size_t rem_size;

    void realloc_buffer(int nsize);

public:
    StringBuffer();
    ~StringBuffer();

    void print(const char *fmt, ...);
    void print_no(const char *fmt, ...);
    const char *get_buffer();
    void reset();
    void increase_ident() {ident += 4;}
    void decrease_ident() {ident -= 4;}
    int get_ident() {return ident;}
    void set_ident(int i) {ident = i;}
    void prepend(const StringBuffer *buf);
};
