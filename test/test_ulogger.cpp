#include <stdio.h>
#include <stdlib.h>
#include "inctype.h"
#include "image.h"
#include "assert.h"
#include <chrono>
#include <thread>
#include <experimental/filesystem> //#include <filesystem>

#include "catch.hpp"
#include "ulogger.h"
#include "cbuf_stream.h"

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


TEST_CASE( "Record one message", "[ULogger]" ) {
    std::string currentpath = std::experimental::filesystem::current_path(); 
    //printf( "currentpath %s\n", currentpath.c_str() );
    ULogger::getULogger()->setOutputDir( currentpath.c_str() );

    // add some messages
    messages::image img;
    set_data(img, 13);
    ULogger::getULogger()->serialize( img );

    // FIX: The test should pass without this sleep. 
    // But the filename isn't available until ULogger's thread creates it.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string filename = ULogger::getULogger()->getFilename();
    //printf( "the filename is %s\n", filename.c_str() );
    REQUIRE( filename.size() != 0 ); 

    // Note, no calls to getULogger() are valid after endLogging
    ULogger::endLogging();

    //printf( "the filename is %s\n", filename2.c_str() );
    REQUIRE( std::experimental::filesystem::exists( filename.c_str() ) );

    // cleanup test residue
    unlink( filename.c_str() );
}

TEST_CASE( "Record many messages", "[ULogger]" ) {
    std::string currentpath = std::experimental::filesystem::current_path(); 
    //printf( "currentpath %s\n", currentpath.c_str() );
    ULogger::getULogger()->setOutputDir( currentpath.c_str() );

    // add some messages
    messages::image img;
    set_data(img, 12);
    ULogger::getULogger()->serialize( img );
    set_data(img, 14);
    ULogger::getULogger()->serialize( img );
    set_data(img, 15);
    ULogger::getULogger()->serialize( img );

    // FIX: The test should pass without this sleep. 
    // But the filename isn't available until ULogger's thread creates it.
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::string filename = ULogger::getULogger()->getFilename();
    //printf( "the filename is %s\n", filename.c_str() );
    REQUIRE( filename.size() != 0 ); 

    // Note, no calls to getULogger() are valid after endLogging
    ULogger::endLogging();

    //printf( "the filename is %s\n", filename2.c_str() );
    REQUIRE( std::experimental::filesystem::exists( filename.c_str() ) );

    // cleanup test residue
    unlink( filename.c_str() );
}

