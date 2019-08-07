#include <gtest/gtest.h>
#include <iostream>
#include "elapsed_time.hpp"
#include "cmm_defines.hpp"
#include "redis_helper.hpp"

//const size_t MAX_LOOP = 100000;
const size_t MAX_LOOP = 100;

///////////////////////////////////////////////////////////////////////////////
class RedisTest : public ::testing::Test {
    public:
        // Per-test-suite set-up.Called before the first test in this test suite.
        static void SetUpTestSuite() {
        }
        // Per-test-suite tear-down. Called after the last test in this test suite.
        static void TearDownTestSuite() {
        }
        void SetUp()  {  
        } 
        void TearDown() {
        }//per every test.before the destructor.
    protected:
};

///////////////////////////////////////////////////////////////////////////////
TEST_F(RedisTest, ConnectServer_success)
{
    DEBUG_LOG("connect master");
    RedisHelper redis_client1;
    ASSERT_TRUE(redis_client1.ConnectServer("127.0.0.1", 6379, ROLE_MASTER));
    ASSERT_TRUE(redis_client1.ConnectServer("127.0.0.1", 6379));

    RedisHelper redis_client2;
    ASSERT_TRUE(redis_client2.ConnectServer("127.0.0.1", 6379));
    ASSERT_TRUE(redis_client2.ConnectServer("127.0.0.1", 6379));
    
    //DEBUG_LOG("connect replica(slave)");
    //RedisHelper redis_client3;
    //ASSERT_TRUE(redis_client3.ConnectServer("127.0.0.1", 7000, ROLE_REPLICA));
}
///////////////////////////////////////////////////////////////////////////////
TEST_F(RedisTest, ConnectServer_fail)
{
    RedisHelper redis_client1;
    EXPECT_FALSE(redis_client1.ConnectServer("127.0.0.111", 6379));
    std::cout << redis_client1.GetLastErrMsg() << "\n";

    RedisHelper redis_client2;
    redis_client2.SetConnectTimeoutSecs(10); //sec
    EXPECT_FALSE(redis_client2.ConnectServer("127.0.0.1", 99999));
    std::cout << redis_client2.GetLastErrMsg() << "\n";
}
///////////////////////////////////////////////////////////////////////////////
TEST_F(RedisTest, ConnectRedisServerWithSvrList_1)
{
    RedisHelper redis_helper;
    EXPECT_TRUE(redis_helper.ConnectServerOneOfThese("127.0.0.1:6379,127.0.0.2:6379"));
}
///////////////////////////////////////////////////////////////////////////////
TEST_F(RedisTest, ConnectRedisServerWithSvrList_2)
{
    RedisHelper redis_helper;
    EXPECT_TRUE(redis_helper.ConnectServerOneOfThese("127.0.0.2:6379,127.0.0.1:6379"));
}

///////////////////////////////////////////////////////////////////////////////
TEST_F(RedisTest, SetGetDel)
{
    ElapsedTime elapsed;
    elapsed.SetStartTime();

    RedisHelper redis_helper;
    ASSERT_TRUE(redis_helper.ConnectServer("127.0.0.1", 6379));
    EXPECT_TRUE(redis_helper.DoCommand("SET kojh_k1 kojh_val_1"));
    EXPECT_TRUE(redis_helper.DoCommand("SET kojh_k2 kojh_val_2"));
    EXPECT_TRUE(redis_helper.DoCommand("GET kojh_k1"));
    ASSERT_TRUE(redis_helper.GetReply() !=NULL);
    DEBUG_LOG("GET : type " << redis_helper.GetReply()->type << "," 
             << redis_helper.GetReply()->str);//REDIS_REPLY_STRING 1
    EXPECT_STREQ(redis_helper.GetReply()->str,"kojh_val_1");

    redisReply* reply ;
    EXPECT_TRUE(redis_helper.DoCommand("GET kojh_k2"));
    reply=redis_helper.GetReply();
    ASSERT_TRUE(reply !=NULL);
    EXPECT_STREQ(reply->str,"kojh_val_2");
    
    EXPECT_TRUE(redis_helper.DoCommand("DEL kojh_k1 kojh_k2 kojh_k3"));//3 requests
    reply=redis_helper.GetReply();
    DEBUG_LOG("DEL : type =" <<  reply->type << "," << reply->integer 
            << "," << reply->str);//REDIS_REPLY_INTEGER 3
    EXPECT_EQ(reply->integer, 2); // but 2 deleted.

    size_t elapsed_mili= elapsed.SetEndTime(MILLI_SEC_RESOLUTION);
    std::cout << "elapsed =" << elapsed_mili << " ms\n";
}

///////////////////////////////////////////////////////////////////////////////
TEST_F(RedisTest, SetPerf)
{
    ElapsedTime elapsed;
    RedisHelper redis_helper;
    ASSERT_TRUE(redis_helper.ConnectServer("127.0.0.1", 6379));
    redisReply* reply ;

    elapsed.SetStartTime();
    char temp_val [1024];
    size_t invok_success = 0;
    for(size_t i=0; i < MAX_LOOP; i++){
        if(!redis_helper.DoCommand("SET perf_k%ld perf_val_%ld", i,i)){
            return; //error 
        }
        invok_success++;
    }
    EXPECT_TRUE(invok_success==MAX_LOOP);

    std::cout << "elapsed =" << elapsed.SetEndTime(MILLI_SEC_RESOLUTION) << " ms\n";
    
    //check inserted
    EXPECT_TRUE(redis_helper.DoCommand("GET perf_k%ld", MAX_LOOP-1));
    reply=redis_helper.GetReply();
    ASSERT_TRUE(reply !=NULL);
    snprintf(temp_val,sizeof(temp_val),"perf_val_%ld", MAX_LOOP-1);
    EXPECT_STREQ(reply->str, temp_val);
    
    //clear all
    invok_success=0;
    for(size_t i=0; i < MAX_LOOP; i++){
        if(!redis_helper.DoCommand("DEL perf_k%ld ", i)) {
            break;
        }
        invok_success++;
    }
    EXPECT_TRUE(invok_success==MAX_LOOP);
}

///////////////////////////////////////////////////////////////////////////////
TEST_F(RedisTest, PipeInvalidCmd)
{
    ElapsedTime elapsed;
    RedisHelper redis_helper;
    ASSERT_TRUE(redis_helper.ConnectServer("127.0.0.1", 6379));

    //invalid command 
    EXPECT_TRUE(redis_helper.AppendCmdPipeline("SETXXXX?X k1 v1"));
    //No error occurs 

    EXPECT_FALSE(redis_helper.EndCmdPipeline());
    ASSERT_TRUE(redis_helper.GetReply() ==NULL);
    ASSERT_TRUE(redis_helper.GetAppendedCmdCnt() ==0);
}

///////////////////////////////////////////////////////////////////////////////
TEST_F(RedisTest, PipeInvalidCmd2)
{
    ElapsedTime elapsed;
    RedisHelper redis_helper;
    ASSERT_TRUE(redis_helper.ConnectServer("127.0.0.1", 6379));

    //invalid command 
    EXPECT_TRUE(redis_helper.AppendCmdPipeline("SET  k0001 v0001"));
    EXPECT_TRUE(redis_helper.AppendCmdPipeline("SET? k0002 v0002"));
    //No error occurs 

    EXPECT_TRUE(redis_helper.AppendCmdPipeline("SET  k0003 v0003"));
    EXPECT_FALSE(redis_helper.EndCmdPipeline()); //failed finally
    DEBUG_LOG("redis_helper.GetAppendedCmdCnt()=" << redis_helper.GetAppendedCmdCnt());
    ASSERT_TRUE(redis_helper.GetAppendedCmdCnt() ==0);

    //clear test
    EXPECT_TRUE(redis_helper.DoCommand("DEL k0001"));
    EXPECT_TRUE(redis_helper.DoCommand("DEL k0002"));
    EXPECT_TRUE(redis_helper.DoCommand("DEL k0003"));
}

///////////////////////////////////////////////////////////////////////////////
TEST_F(RedisTest, PipeSetPerf)
{
    ElapsedTime elapsed;
    RedisHelper redis_helper;
    ASSERT_TRUE(redis_helper.ConnectServer("127.0.0.1", 6379));

    elapsed.SetStartTime();
    char temp_val [1024];

    //---------------------------
    for(size_t i=0; i < MAX_LOOP; i++){
        EXPECT_TRUE(redis_helper.AppendCmdPipeline("SET perf_k%ld perf_val_%ld", i,i));
    }

    redisReply* reply ;
    EXPECT_TRUE(redis_helper.EndCmdPipeline());
    ASSERT_TRUE(redis_helper.GetReply() ==NULL);
    ASSERT_TRUE(redis_helper.GetAppendedCmdCnt() ==0);

    EXPECT_TRUE(redis_helper.EndCmdPipeline());
    ASSERT_TRUE(redis_helper.GetAppendedCmdCnt() ==0);

    //---------------------------
    size_t elapsed_mili= elapsed.SetEndTime(MILLI_SEC_RESOLUTION);
    std::cout << "elapsed =" << elapsed_mili << " ms\n";
    
    //check inserted
    EXPECT_TRUE(redis_helper.DoCommand("GET perf_k%ld", MAX_LOOP-1));
    reply=redis_helper.GetReply();
    ASSERT_TRUE(reply !=NULL);
    snprintf(temp_val,sizeof(temp_val),"perf_val_%ld", MAX_LOOP-1);
    EXPECT_STREQ(reply->str, temp_val);
    
    //clear all
    for(size_t i=0; i < MAX_LOOP; i++){
        EXPECT_TRUE(redis_helper.AppendCmdPipeline("DEL perf_k%ld",i));
    }
    EXPECT_TRUE(redis_helper.EndCmdPipeline());
    ASSERT_TRUE(redis_helper.GetAppendedCmdCnt() ==0);
}

