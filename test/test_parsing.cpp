// Unit tests for CBufParser class
#include <stdlib.h>

#include <chrono>

#include "CBufParser.h"
#include "gtest/gtest.h"
#include "inctype.h"
#include "test_utils.h"

// built from samples/image.cbuf
#include "image.h"

#if defined(HJSON_PRESENT)

#include "image_json.h"

// Test FillHjson method.
TEST(HJsonParsing, Simple) {
  messages::image img1;
  set_data(img1, 43);
  std::vector<char> v;
  v.resize(img1.encode_net_size());
  ASSERT_TRUE(img1.encode_net(v.data(), v.size()));

  CBufParser parser;
  ASSERT_TRUE(parser.ParseMetadata(messages::image::cbuf_string, messages::image::TYPE_STRING));

  messages::image img2;
  Hjson::Value hjson;
  EXPECT_GT(parser.FillHjson(messages::image::TYPE_STRING, (unsigned char*)v.data(), v.size(), hjson, false),
            0);
  loadFromJson(hjson, img2);
  ASSERT_TRUE(compare(img1, img2));
}

// Test FillHjson method with more complex example.
TEST(HJsonParsing, Complex) {
  messages::complex_thing th1;
  set_data(th1, 43);
  std::vector<char> v;
  v.resize(th1.encode_net_size());
  ASSERT_TRUE(th1.encode_net(v.data(), v.size()));

  CBufParser parser1;
  ASSERT_TRUE(
      parser1.ParseMetadata(messages::complex_thing::cbuf_string, messages::complex_thing::TYPE_STRING));

  messages::complex_thing th2;
  Hjson::Value val;
  EXPECT_GT(
      parser1.FillHjson(messages::complex_thing::TYPE_STRING, (unsigned char*)v.data(), v.size(), val, false),
      0);
  loadFromJson(val, th2);
  ASSERT_TRUE(compare(th1, th2));

  /*
      uncomment this to test performance
      for(int i=0; i<50; i++) {
        messages::complex_thing th3;
        EXPECT_GT(parser1.FillHjson(messages::complex_thing::TYPE_STRING, (unsigned char*)v.data(), v.size(),
     val, false), 0); loadFromJson(val, th3);
      }
  */
}

TEST(JStrParsing, Simple) {
  messages::image img1;
  set_data(img1, 43);
  std::vector<char> v;
  v.resize(img1.encode_net_size());
  ASSERT_TRUE(img1.encode_net(v.data(), v.size()));

  CBufParser parser1;
  ASSERT_TRUE(parser1.ParseMetadata(messages::image::cbuf_string, messages::image::TYPE_STRING));

  messages::image img2;
  std::string str;
  EXPECT_GT(parser1.FillJstr(messages::image::TYPE_STRING, (unsigned char*)v.data(), v.size(), str), 0);
  Hjson::Value val = Hjson::Unmarshal(str);
  loadFromJson(val, img2);
  ASSERT_TRUE(compare(img1, img2));
}

TEST(JStrParsing, Complex) {
  messages::complex_thing th1;
  set_data(th1, 43);
  std::vector<char> v;
  v.resize(th1.encode_net_size());
  ASSERT_TRUE(th1.encode_net(v.data(), v.size()));

  CBufParser parser1;
  ASSERT_TRUE(
      parser1.ParseMetadata(messages::complex_thing::cbuf_string, messages::complex_thing::TYPE_STRING));

  messages::complex_thing th2;
  std::string str;
  EXPECT_GT(parser1.FillJstr(messages::complex_thing::TYPE_STRING, (unsigned char*)v.data(), v.size(), str),
            0);
  Hjson::Value val = Hjson::Unmarshal(str);
  loadFromJson(val, th2);
  ASSERT_TRUE(compare(th1, th2));
}

#endif

TEST(CParsing, Simple) {
  messages::image img1;
  set_data(img1, 43);
  std::vector<char> v;
  v.resize(img1.encode_net_size());
  ASSERT_TRUE(img1.encode_net(v.data(), v.size()));

  CBufParser parser1;
  ASSERT_TRUE(parser1.ParseMetadata(messages::image::cbuf_string, messages::image::TYPE_STRING));

  // Do two parsers for consistency
  messages::image img2;
  CBufParser parser2;
  ASSERT_TRUE(parser2.ParseMetadata(messages::image::cbuf_string, messages::image::TYPE_STRING));
  EXPECT_GT(parser1.FastConversion(messages::image::TYPE_STRING, (unsigned char*)v.data(), v.size(), parser2,
                                   messages::image::TYPE_STRING, (unsigned char*)&img2, sizeof(img2)),
            0);
  ASSERT_TRUE(compare(img1, img2));
}

TEST(CParsing, Complex) {
  messages::complex_thing th1;
  set_data(th1, 43);
  std::vector<char> v;
  v.resize(th1.encode_net_size());
  ASSERT_TRUE(th1.encode_net(v.data(), v.size()));

  CBufParser parser1;
  ASSERT_TRUE(
      parser1.ParseMetadata(messages::complex_thing::cbuf_string, messages::complex_thing::TYPE_STRING));

  // Do two parsers for consistency
  messages::complex_thing th2;
  CBufParser parser2;
  ASSERT_TRUE(
      parser2.ParseMetadata(messages::complex_thing::cbuf_string, messages::complex_thing::TYPE_STRING));
  EXPECT_GT(parser1.FastConversion(messages::complex_thing::TYPE_STRING, (unsigned char*)v.data(), v.size(),
                                   parser2, messages::complex_thing::TYPE_STRING, (unsigned char*)&th2,
                                   sizeof(th2)),
            0);
  ASSERT_TRUE(compare(th1, th2));

  /*
      Uncomment this to test performance
      for(int i=0; i<50; i++) {
        messages::complex_thing th3;
        EXPECT_GT(parser1.FastConversion(messages::complex_thing::TYPE_STRING, (unsigned char*)v.data(),
     v.size(), parser2, messages::complex_thing::TYPE_STRING, (unsigned char *)&th3, sizeof(th3)), 0);
      }
  */
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
