#pragma once
#include "image.h"

void ensure(bool check, const char* str);
void set_data(messages::image& img, unsigned seed);
void set_data(messages::complex_thing& th, unsigned seed);
bool compare(const messages::image& a, const messages::image& b);
bool compare(const messages::complex_thing& a, const messages::complex_thing& b);
