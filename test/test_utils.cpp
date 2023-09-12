#include "test_utils.h"

void ensure(bool check, const char* str) {
  if (check) return;
  fprintf(stderr, "Test failure, reason: %s\n", str);
  assert(false);
}

void set_data(messages::image& img, unsigned seed) {
  srand(seed);
  img.rows = rand();
  img.cols = rand();
  img.format = rand();
  img.type = rand();
  img.timestamp = double(rand());
  for (unsigned i = 0; i < sizeof(img.pixels); i++) {
    img.pixels[i] = rand();
  }
}

void set_data(messages::complex_thing& th, unsigned seed) {
  srand(seed);
  th.one_val = rand();
  for (unsigned i = 0; i < 73; i++) {
    th.dynamic_array.push_back(rand());
  }
  th.name = "This is my long string";
  set_data(th.img[0], 3456);
  set_data(th.img[1], 3489);
  set_data(th.img[2], 1980);
  for (unsigned i = 0; i < 5; i++) {
    std::string var = "My simple prefix";
    var += std::to_string(rand());
    th.names[i] = var;
  }
  for (unsigned i = 0; i < 24; i++) {
    std::string var = "My own prefix";
    var += std::to_string(rand());
    th.hard_dynamic.push_back(var);
  }
}

bool compare(const messages::image& a, const messages::image& b) {
  ensure(a.preamble.hash == b.preamble.hash, "Compare image hash");
  ensure(a.rows == b.rows, "Compare image rows");
  ensure(a.cols == b.cols, "Compare image cols");
  ensure(a.format == b.format, "Compare image format");
  ensure(a.type == b.type, "Compare image type");
  ensure(a.timestamp == b.timestamp, "Compare image timestamp");
  ensure(memcmp(a.pixels, b.pixels, sizeof(a.pixels)) == 0, "Compare image pixels");

  return true;
}

bool compare(const messages::complex_thing& a, const messages::complex_thing& b) {
  ensure(a.preamble.hash == b.preamble.hash, "Compare complex thing hash");
  ensure(a.one_val == b.one_val, "Compare complex thing one_val");

  ensure(a.dynamic_array.size() == b.dynamic_array.size(), "Compare sizes of dynamic_array");
  for (unsigned i = 0; i < a.dynamic_array.size(); i++) {
    ensure(a.dynamic_array[i] == b.dynamic_array[i], "Comparing dynamic_array element");
  }
  ensure(a.name == b.name, "Comparing complex thing's name");
  ensure(compare(a.img[0], b.img[0]), "Comparing complex thing img0");
  ensure(compare(a.img[1], b.img[1]), "Comparing complex thing img1");
  ensure(compare(a.img[2], b.img[2]), "Comparing complex thing img2");

  for (unsigned i = 0; i < 5; i++) {
    std::string errMsg = "Compare complex things names[" + std::to_string(i) + "]: '" + a.names[i] +
                         "' vs '" + b.names[i] + "'";
    ensure(a.names[i] == b.names[i], errMsg.c_str());
  }

  ensure(a.hard_dynamic.size() == b.hard_dynamic.size(), "Compare sizes of hard_dynamic");
  for (unsigned i = 0; i < a.hard_dynamic.size(); i++) {
    ensure(a.hard_dynamic[i] == b.hard_dynamic[i], "Comparing hard_dynamic element");
  }

  return true;
}
