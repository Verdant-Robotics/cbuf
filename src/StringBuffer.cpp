#include "StringBuffer.h"
#include <stdio.h>
#include <stdarg.h>

StringBuffer::StringBuffer()
{
    buffer = new char[8196];
    end = buffer;
    end[0] = 0;
}

StringBuffer::~StringBuffer()
{
    delete buffer;
    buffer = nullptr;
    end = nullptr;
}

void StringBuffer::print(const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    end += vsprintf(end, fmt, args);
    va_end(args);
}

const char *StringBuffer::get_buffer()
{
    return buffer;
}

void StringBuffer::reset()
{
    end = buffer;
    end[0] = 0;
}