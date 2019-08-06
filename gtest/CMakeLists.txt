CMAKE_MINIMUM_REQUIRED(VERSION 2.8)

#SET(CMAKE_BUILD_TYPE Debug)
#SET(CMAKE_CXX_FLAGS_DEBUG " -g -fno-omit-frame-pointer ")
SET(CMAKE_BUILD_TYPE Release)

SET(CMAKE_VERBOSE_MAKEFILE true)
SET(CMAKE_CXX_FLAGS "-std=c++11 " )

ADD_COMPILE_OPTIONS ( -Wall -fPIC )
ADD_DEFINITIONS ( -D_DEBUG_PRINTF_ )

INCLUDE_DIRECTORIES	( 	/usr/include
						/usr/local/include
						./ 
						../
					)
                   
LINK_DIRECTORIES	( 	/usr/lib 
						/usr/lib64
						/usr/local/lib
						/usr/local/lib64
					)

LINK_LIBRARIES(hiredis pthread )

SET(CMAKE_RUNTIME_OUTPUT_DIRECTORY "./")

ADD_EXECUTABLE	(test_main 
                 gtest_main.cpp 
                 gtest_1.cpp 
                 gtest_2.cpp
                )

TARGET_LINK_LIBRARIES (test_main LINK_PUBLIC gtest)
TARGET_LINK_LIBRARIES (test_main LINK_PUBLIC gtest_main)

