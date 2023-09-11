#include <stdlib.h>

#include <chrono>
#include <filesystem>
#include <thread>

#include "cbuf_stream.h"
#include "gtest/gtest.h"
#include "image.h"
#include "inctype.h"
#include "ulogger.h"

namespace fs = std::filesystem;

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

TEST(RecordOneMessage, ULogger) {
  std::string currentpath = fs::current_path();
  // printf( "currentpath %s\n", currentpath.c_str() );
  ULogger::getULogger()->setLogPath(currentpath);

  // add some messages
  messages::image img;
  set_data(img, 13);
  ULogger::getULogger()->serialize(img);

  // FIX: The test should pass without this sleep.
  // But the filename isn't available until ULogger's thread creates it.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::string filename = ULogger::getULogger()->getCurrentUlogPath();
  // printf( "the filename is %s\n", filename.c_str() );
  EXPECT_TRUE(filename.size() != 0);

  // Note, no calls to getULogger() are valid after endLogging
  ULogger::endLogging();

  // printf( "the filename is %s\n", filename2.c_str() );
  EXPECT_TRUE(fs::exists(filename.c_str()));

  // cleanup test residue
  unlink(filename.c_str());
}

TEST(RecordManyMessages, ULogger) {
  std::string currentpath = fs::current_path();
  // printf( "currentpath %s\n", currentpath.c_str() );
  ULogger::getULogger()->setLogPath(currentpath.c_str());

  // add some messages
  messages::image img;
  set_data(img, 12);
  ULogger::getULogger()->serialize(img);
  set_data(img, 14);
  ULogger::getULogger()->serialize(img);
  set_data(img, 15);
  ULogger::getULogger()->serialize(img);

  // FIX: The test should pass without this sleep.
  // But the filename isn't available until ULogger's thread creates it.
  std::this_thread::sleep_for(std::chrono::milliseconds(100));

  std::string filename = ULogger::getULogger()->getCurrentUlogPath();
  // printf( "the filename is %s\n", filename.c_str() );
  EXPECT_TRUE(filename.size() != 0);

  // Note, no calls to getULogger() are valid after endLogging
  ULogger::endLogging();

  // printf( "the filename is %s\n", filename2.c_str() );
  EXPECT_TRUE(fs::exists(filename.c_str()));

  // cleanup test residue
  unlink(filename.c_str());
}

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
