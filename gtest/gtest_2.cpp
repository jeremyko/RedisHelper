#include <gtest/gtest.h>
#include <iostream>
#include "cmm_defines.hpp"
#include "elapsed_time.hpp"
#include "redis_helper.hpp"
#include <unistd.h>
#include <string.h>
#include <thread>

const size_t MAX_TEST         = 10;
const size_t TOTAL_THREAD_CNT = 50;

///////////////////////////////////////////////////////////////////////////////
class RedisTest2 : public ::testing::Test {
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
TEST_F(RedisTest2, TestPerf1)
{
    RedisHelper redis_client;
    ASSERT_TRUE(redis_client.ConnectServer("127.0.0.1", 6379));
    size_t cnt = 0;
    ElapsedTime elapsed;
    elapsed.SetStartTime();
    while(1){
        EXPECT_TRUE(redis_client.DoCommand("ping"));
        cnt ++;
        if(cnt>MAX_TEST){
            break;
        }
    }
    DEBUG_LOG("DoCommand elapsed(" << MAX_TEST << ") =" 
            << elapsed.SetEndTime(MILLI_SEC_RESOLUTION) << " ms");

    cnt = 0;
    elapsed.SetStartTime();
    while(1){
        EXPECT_TRUE(redis_client.DoCommand("ping"));
        cnt ++;
        if(cnt>MAX_TEST){
            break;
        }
    }
    DEBUG_LOG("DoCommand elapsed(" << MAX_TEST << ") =" 
            << elapsed.SetEndTime(MILLI_SEC_RESOLUTION) << " ms");
}

///////////////////////////////////////////////////////////////////////////////
void WorkerThreadPing(int index)
{
    ElapsedTime elapsed;
    elapsed.SetStartTime();
    size_t max_cnt = MAX_TEST / TOTAL_THREAD_CNT;
    DEBUG_GREEN_LOG("index="<<index<<", max_cnt per clients ==>" << max_cnt);
    RedisHelper redis_client;
    EXPECT_TRUE(redis_client.ConnectServer("127.0.0.1", 6379));
    for(size_t i= 0; i < max_cnt; i++){
        EXPECT_TRUE(redis_client.DoCommand("ping"));
    }
    DEBUG_LOG("ping thread elapsed(" << max_cnt << ") =" 
            << elapsed.SetEndTime(MILLI_SEC_RESOLUTION) << " ms");
}

///////////////////////////////////////////////////////////////////////////////
void WorkerThreadSet(int index)
{
    int max_cnt = MAX_TEST / TOTAL_THREAD_CNT; // 5
	int from_index = index * max_cnt ;
	int to_index = from_index + max_cnt ;
    DEBUG_GREEN_LOG("index="<<index<<",max_cnt per client=" << max_cnt
            << ", (from ~ to) ==>" << from_index <<"~"<<to_index);

    RedisHelper redis_client;
    EXPECT_TRUE(redis_client.ConnectServer("127.0.0.1", 6379));
    for(int index = from_index; index < to_index; index++ ){
        EXPECT_TRUE(redis_client.DoCommand("SET key_%ld val__________________%09ld", 
                    index,index));
    }
    DEBUG_GREEN_LOG("index="<<index<<", end..");
}

///////////////////////////////////////////////////////////////////////////////
void DelAll()
{
    DEBUG_GREEN_LOG(" delete all ");
    RedisHelper redis_client;
    ASSERT_TRUE(redis_client.ConnectServer("127.0.0.1", 6379));
    for(size_t index = 0; index < MAX_TEST; index++ ){
        bool result = redis_client.DoCommand("DEL key_%ld ", index);
        if(!result){
            DEBUG_ELOG ("DEL failed: "<< redis_client.GetLastErrMsg() );
        }
        EXPECT_TRUE(result);
    }
    DEBUG_GREEN_LOG("DEL end..");
}

///////////////////////////////////////////////////////////////////////////////
TEST_F(RedisTest2, ThreadRunTest)
{
    ElapsedTime elapsed;
    elapsed.SetStartTime();
    std::vector<std::thread> vec_thread_ ;
    for(size_t i=0; i < TOTAL_THREAD_CNT ; i++) {
        vec_thread_.push_back(std::thread (&WorkerThreadPing, i)) ;
        //vec_thread_.push_back(std::thread (&WorkerThreadSet, i)) ;
    }
    for(size_t i = 0; i < vec_thread_.size(); i++) {
        if (vec_thread_[i].joinable()) {
            vec_thread_[i].join();
        }
    }
    //-------------------
    DEBUG_LOG("all thread elapsed(" << MAX_TEST << ") =" 
            << elapsed.SetEndTime(MILLI_SEC_RESOLUTION) << " ms");

	//DelAll();
}

///////////////////////////////////////////////////////////////////////////////
TEST_F(RedisTest2, ZAddTimeStamp )
{
    RedisHelper redis_client;
    ASSERT_TRUE(redis_client.ConnectServer("127.0.0.1", 6379));
    EXPECT_TRUE(redis_client.ZAddTimeStamp("zadd_key1", "zadd_value1"));
    EXPECT_TRUE(redis_client.ZAddTimeStamp("zadd_key1", "zadd_value2"));
    EXPECT_TRUE(redis_client.ZAddTimeStamp("zadd_key1", "zadd_value3"));
    EXPECT_TRUE(redis_client.DoCommand("zscan zadd_key1 0" ) );
    redisReply* reply = redis_client.GetReply();
    ASSERT_TRUE(reply != NULL);
    EXPECT_TRUE(reply->type == REDIS_REPLY_ARRAY);
    EXPECT_TRUE(reply->elements == 2);
    EXPECT_STREQ(reply->element[0]->str, "0");
    EXPECT_TRUE( reply->element[1]->elements == 6);
    
    for (size_t j = 0; j < reply->element[1]->elements; j++) {
        DEBUG_LOG(j << ") " << reply->element[1]->element[j]->str);
    }
}

