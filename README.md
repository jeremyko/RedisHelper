# RedisHelper
hiredis c++ helper class

## features 
- header only library
- connect/disconnect callbacks with ip,port information.
- replication support(managing connection to master) 
- auto reconnecting 
- pipeline support 

## usage
```cpp
//simple usage
RedisHelper redis_helper;
ASSERT_TRUE(redis_helper.SetIpsPorts("localhost", 6379));
//ASSERT_TRUE(redis_helper.SetIpsPorts("replica1_ip", port1));//replica ip,port (if you use replications).
//ASSERT_TRUE(redis_helper.SetIpsPorts("replica2_ip", port2));//replica ip,port (if you use replications).
ASSERT_TRUE(redis_helper.ConnectServer()); //connect to master
EXPECT_TRUE(redis_helper.DoCommand("SET key1 val1"));
EXPECT_TRUE(redis_helper.DoCommand("GET key1"));
ASSERT_TRUE(redis_helper.GetReply() !=NULL);
EXPECT_STREQ(redis_helper.GetReply()->str,"val1");

//pipeline usage
for(size_t i=0; i < 1000; i++){    
    EXPECT_TRUE(redis_helper.AppendCmdPipeline("SET key%ld val%ld", i,i));
}
EXPECT_TRUE(redis_helper.EndCmdPipeline());

for(size_t i=0; i < 1000; i++){    
    EXPECT_TRUE(redis_helper.AppendCmdPipeline("DEL key%ld",i));
}
EXPECT_TRUE(redis_helper.EndCmdPipeline());
```
