//201907 kojh create 
#include <gtest/gtest.h>
#include <iostream>

int main(int argc, char* argv[])
{
    ::testing::InitGoogleTest(&argc, argv);
    if(RUN_ALL_TESTS()) {
        std::cerr << "TEST FAILED!!\n";
    }
    return 0;
}

