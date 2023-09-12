// Run a set of tests using support code in test_utils.h

#include <stdio.h>
#include <stdlib.h>

#include <cstdio>

#include "assert.h"
#include "cbuf_stream.h"
#include "image.h"
#include "test_utils.h"

void test_encode_decode();
void test_serialize();
void test_large();

int main(int argc, char** argv) {
  test_encode_decode();

  test_serialize();

  test_large();

  return 0;
}

// Create an image and ensure round-trip to/from file works.
void test_serialize() {
  messages::image img, img2;
  bool ret;
  set_data(img, 13);
  const char* test_filename = std::tmpnam(nullptr);

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

void test_encode_decode() {
  messages::image img, *pimg;
  messages::complex_thing th, th2;
  set_data(img, 13);
  set_data(th, 68);

  char* buffer = (char*)malloc(50 * 1024);
  unsigned int buf_size = 50 * 1024;
  char* buf = buffer;
  bool ret;

  ret = img.encode(buf, buf_size);
  ensure(ret, "Encode simple image");

  buf += img.encode_size();
  buf_size -= img.encode_size();

  ret = th.encode(buf, buf_size);
  ensure(ret, "Encode complex thing");

  buf += th.encode_size();
  buf_size -= th.encode_size();

  buf = buffer;
  buf_size = 50 * 1024;
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

void test_large() {
  messages::getslarge st, loaded, var, var2;
  st.vec.resize(128);
  size_t total_size = st.encode_size();
  size_t megabyte = 1024 * 1024;
  ensure(total_size > 256 * megabyte, "Create a large buffer");
  ensure(total_size < 1200 * megabyte, "Ensure the size is not too big");

  char* buf = st.encode();
  bool ret = loaded.decode(buf, total_size);
  ensure(ret, "Decoding large struct");
  ensure(loaded.encode_size() == total_size, "We have retrieved the size successfully");
  st.free_encode(buf);

  var.vec.resize(16);
  total_size = var.encode_size();
  var.preamble.setVariant(7);
  buf = var.encode();
  ret = var2.decode(buf, total_size);
  ensure(ret, "Decoding large variant struct");
  ensure(var2.encode_size() == total_size, "We have retrieved the size successfully");
  var.free_encode(buf);
}
