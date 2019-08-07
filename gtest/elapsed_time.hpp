
#ifndef __ELAPSED_TIME_HPP__
#define __ELAPSED_TIME_HPP__

#include <chrono>

typedef std::chrono::duration<int, std::milli> millisecs_t;
typedef std::chrono::duration<long long, std::micro> microsecs_t;

typedef enum _ENUM_TIME_RESOLUTION_ {
    MILLI_SEC_RESOLUTION,
    MICRO_SEC_RESOLUTION,
    SEC_RESOLUTION

} ENUM_TIME_RESOLUTION;
 
///////////////////////////////////////////////////////////////////////////////
class ElapsedTime
{
  protected:

    std::chrono::steady_clock::time_point startT_; 
    std::chrono::steady_clock::time_point endT_;   

  public:
    void SetStartTime() {
        startT_ = std::chrono::steady_clock::now(); 
    }

    size_t SetEndTime( ENUM_TIME_RESOLUTION resolution) {
        endT_ = std::chrono::steady_clock::now(); 

        if(resolution == SEC_RESOLUTION) {
            std::chrono::seconds secs=std::chrono::duration_cast<std::chrono::seconds> (endT_ - startT_); ;
            return secs.count();
        } else if(resolution == MILLI_SEC_RESOLUTION) {
            millisecs_t duration(std::chrono::duration_cast<millisecs_t>(endT_ - startT_));
            return duration.count();
        } else if (resolution == MICRO_SEC_RESOLUTION) {
            microsecs_t duration(std::chrono::duration_cast<microsecs_t>(endT_ - startT_));
            return duration.count();
        }
        return -1; //error
    }
};

#endif




