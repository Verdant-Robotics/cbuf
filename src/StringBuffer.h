#pragma once

class StringBuffer
{
    char *buffer;
    char *end;

public:
    StringBuffer();
    ~StringBuffer();

    void print(const char *fmt, ...);
    const char *get_buffer();
    void reset();
};