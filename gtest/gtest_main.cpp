#include <gtest/gtest.h>
#include "cmm_defines.hpp"

int main(int argc, char* argv[])
{
    //::testing::GTEST_FLAG(filter) = "RedisTest.PipeSetPerf";
    //::testing::GTEST_FLAG(filter) = "RedisTest.*";
    //::testing::GTEST_FLAG(filter) = "RedisTest.ConnectServer_success";
    //::testing::GTEST_FLAG(filter) = "RedisTest.SetGetDel" ;
    //::testing::GTEST_FLAG(filter) = "RedisTest.SetReplyOff" ;
    //::testing::GTEST_FLAG(filter) = "RedisTest.SetReplyOffOnReconnect" ;
    //::testing::GTEST_FLAG(filter) = "RedisTest.PipeInvalidCmd2" ;
    //::testing::GTEST_FLAG(filter) = "RedisTest2.ZAddTimeStamp" ;
    //::testing::GTEST_FLAG(filter) = "RedisTest.SetPerf" ;

    ::testing::InitGoogleTest(&argc, argv);
    if(RUN_ALL_TESTS()) {
        DEBUG_ELOG(  "TEST FAILED!!");
    }
    return 0;
}

