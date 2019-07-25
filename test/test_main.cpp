#include <stdio.h>
#include <stdlib.h>
#include "inctype.h"
#include "image.h"
#include "assert.h"

#include "cbuf_stream.h"

void set_data(messages::image &img, unsigned seed);
void set_data(messages::complex_thing &th, unsigned seed);
bool compare(const messages::image &a, const messages::image &b);
bool compare(const messages::complex_thing &a, const messages::complex_thing &b);

void test_encode_decode();
void test_serialize();

void ensure(bool check, const char *str)
{
    if (check) return;
    printf("Test failure, reason: %s\n", str);
    assert(false);
}

int main(int argc, char **argv)
{
    test_encode_decode();

    test_serialize();

    return 0;
}

void test_serialize()
{
    messages::image img, img2;
    bool ret;
    set_data(img, 13);
    const char *test_filename = "test_file.ulog";

    remove(test_filename);

    {
      cbuf_ostream cos;
      ret = cos.open_file(test_filename);
      ensure(ret, "Open ostream");
      ret = cos.serialize(&img);
      ensure(ret, "serialize image");
    }

    {
      cbuf_istream cis;
      ret = cis.open_file(test_filename);
      ensure(ret, "Open istream");
      ret = cis.deserialize(&img2);
      ensure(ret, "deserialize image");
    }

    ret = compare(img, img2);
    ensure(ret, "compare images");
}

void test_encode_decode()
{
    messages::image img, *pimg;
    messages::complex_thing th, th2;
    set_data(img, 13);
    set_data(th, 68);

    char *buffer = (char *)malloc(50*1024);
    unsigned int buf_size = 50*1024;
    char *buf = buffer;
    bool ret;

    ret = img.encode(buf, buf_size);
    ensure(ret, "Encode simple image");

    buf += img.encode_size();
    buf_size -= img.encode_size();

    ret = th.encode(buf, buf_size);
    ensure(ret, "Encode complex thing");

    buf += th.encode_size();
    buf_size -= th.encode_size();

    buf = buffer; buf_size = 50*1024;
    ret = messages::image::decode(buf, buf_size, &pimg);
    ensure(ret, "Decode image, return through pointer");
    buf += pimg->encode_size();
    buf_size -= pimg->encode_size();

    ret = compare(img, *pimg);
    ensure(ret, "Compare image before and after encoding");

    ret = th2.decode(buf, buf_size);
    ensure(ret, "Decode complex thing, in place");
    buf += th2.encode_size();
    buf_size -= th2.encode_size();

    ret = compare(th, th2);
    ensure(ret, "Compare image before and after encoding");

    printf("Test encode/decode completed successfully\n");
}


void set_data(messages::image &img, unsigned seed)
{
    srand(seed);
    img.rows = rand();
    img.cols = rand();
    img.format = rand();
    img.type = rand();
    img.timestamp = double(rand());
    for(unsigned i=0; i<sizeof(img.pixels); i++) {
        img.pixels[i] = rand();
    }
}

void set_data(messages::complex_thing &th, unsigned seed)
{
    srand(seed);
    th.one_val = rand();
    for(unsigned i=0; i<73; i++) {
        th.dynamic_array.push_back(rand());
    }
    th.name = "This is my long string";
    set_data(th.img[0], 3456);
    set_data(th.img[1], 3489);
    set_data(th.img[2], 1980);
    for(unsigned i=0; i<5; i++) {
        std::string var = "My simple prefix";
        var += std::to_string(rand());
        th.names[i] = var;
    }
    for(unsigned i=0; i<24; i++) {
        std::string var = "My own prefix";
        var += std::to_string(rand());
        th.hard_dynamic.push_back(var);
    }
}

bool compare(const messages::image &a, const messages::image &b)
{
    ensure(a.preamble.hash == b.preamble.hash, "Compare image hash");
    ensure(a.preamble.size == b.preamble.size, "Compare image size");
    ensure(a.rows == b.rows, "Compare image rows");
    ensure(a.cols == b.cols, "Compare image cols");
    ensure(a.format == b.format, "Compare image format");
    ensure(a.type == b.type, "Compare image type");
    ensure(a.timestamp == b.timestamp, "Compare image timestamp");
    ensure(memcmp(a.pixels, b.pixels, sizeof(a.pixels)) == 0, "Compare image pixels");

    return true;
}

bool compare(const messages::complex_thing &a, const messages::complex_thing &b)
{
    ensure(a.preamble.hash == b.preamble.hash, "Compare complex thing hash");
    ensure(a.preamble.size == b.preamble.size, "Compare complex thing size");
    ensure(a.one_val == b.one_val, "Compare complex thing one_val");

    ensure(a.dynamic_array.size() == b.dynamic_array.size(), "Compare sizes of dynamic_array");
    for(unsigned i=0; i<a.dynamic_array.size(); i++) {
        ensure(a.dynamic_array[i] == b.dynamic_array[i], "Comparing dynamic_array element");
    }
    ensure(a.name == b.name, "Comparing complex thing's name");
    ensure(compare(a.img[0], b.img[0]), "Comparing complex thing img0");
    ensure(compare(a.img[1], b.img[1]), "Comparing complex thing img1");
    ensure(compare(a.img[2], b.img[2]), "Comparing complex thing img2");

    for(unsigned i=0; i<5; i++) {
        ensure(a.names[i] == b.names[i], "Compare complex things names array");
    }

    ensure(a.hard_dynamic.size() == b.hard_dynamic.size(), "Compare sizes of hard_dynamic");
    for(unsigned i=0; i<a.hard_dynamic.size(); i++) {
        ensure(a.hard_dynamic[i] == b.hard_dynamic[i], "Comparing hard_dynamic element");
    }

    printf("CBuf test completed successfully, no errors\n");
    return true;
}
