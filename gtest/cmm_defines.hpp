#ifndef COMMON_DEF_HPP
#define COMMON_DEF_HPP

//color printf
#define COLOR_RED  "\x1B[31m"
#define COLOR_GREEN "\x1B[32m" 
#define COLOR_BLUE "\x1B[34m"
#define COLOR_RESET "\x1B[0m"

#define  LOG_WHERE "("<<__FILE__<<"-"<<__func__<<"-"<<__LINE__<<") "
#define  WHERE_DEF __FILE__,__func__,__LINE__
#define  ERROR_FMT LV_ERROR,"(%s-%s-%d) "
#define  INFO_FMT  LV_INFO,"(%s-%s-%d) "
#define  DBG_FMT LV_DEBUG,"(%s-%s-%d) "

///////////////////////////////////////////////////////////////////////////////
#ifdef DEBUG_PRINTF
#define  DEBUG_LOG(x)  std::cout<<LOG_WHERE << x << "\n"
#define  DEBUG_RED_LOG(x) std::cout<<LOG_WHERE << COLOR_RED<< x << COLOR_RESET << "\n"
#define  DEBUG_GREEN_LOG(x) std::cout<<LOG_WHERE << COLOR_GREEN<< x << COLOR_RESET << "\n"
#define  DEBUG_ELOG(x) std::cerr<<LOG_WHERE << COLOR_RED<< x << COLOR_RESET << "\n"
#else
#define  DEBUG_LOG(x) 
#define  DEBUG_ELOG(x) 
#define  DEBUG_RED_LOG(x) 
#define  DEBUG_GREEN_LOG(x)
#endif

#define  PRINT_ELOG(x) std::cerr<<LOG_WHERE << COLOR_RED<< x << COLOR_RESET << "\n"


#endif // COMMON_DEF_HPP


