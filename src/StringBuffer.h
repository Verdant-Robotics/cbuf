#pragma once

class StringBuffer
{
    char *buffer;
    char *end;
    int ident;

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
};