/****************************************************************************
 Copyright (c) 2020 ko jung hyun
 
 Permission is hereby granted, free of charge, to any person obtaining a copy
 of this software and associated documentation files (the "Software"), to deal
 in the Software without restriction, including without limitation the rights
 to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the Software is
 furnished to do so, subject to the following conditions:
 
 The above copyright notice and this permission notice shall be included in
 all copies or substantial portions of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 THE SOFTWARE.
 ****************************************************************************/

#ifndef REDIS_HELPER_HPP
#define REDIS_HELPER_HPP

#include <hiredis/hiredis.h>
#include <unistd.h> //usleep
#include <string.h>
#include <signal.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <functional>

///////////////////////////////////////////////////////////////////////////////
//color printf
#define COLOR_RED  "\x1B[31m"
#define COLOR_GREEN "\x1B[32m" 
#define COLOR_BLUE "\x1B[34m"
#define COLOR_RESET "\x1B[0m"

#define  LOG_WHERE "("<<__FILE__<<"-"<<__func__<<"-"<<__LINE__<<") "
#define  WHERE_DEF __FILE__,__func__,__LINE__

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

///////////////////////////////////////////////////////////////////////////////
const size_t MAX_CMD_LEN = 1024 * 1024 ; //1 mb
const size_t MAX_CONNECT_RETRY = 50;

//old_ip, old_port, new_ip, new_port,reconnect_flag
typedef std::function<void(const char*,size_t,const char*,size_t,bool)> CONNECT_CALLBACK ;
typedef std::function<void(const char*,size_t,const char*)> DISCONNECT_CALLBACK ;
typedef std::function<void(const char*)> MSG_CALLBACK ;
typedef std::function<bool(void)>        ABRT_CALLBACK ;

///////////////////////////////////////////////////////////////////////////////
class ConnIpPort 
{
  public:
    ConnIpPort(){
        ip   ="";
        port = 0;
    }
    std::string  ip ; 
    size_t       port; 
};
typedef std::vector<ConnIpPort> VecConnIpPorts ;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class RedisHelper 
{
  public:
    RedisHelper() {
        ctx_=NULL;
        reply_=NULL;
        pipe_appended_cnt_ = 0;
        max_reconn_retry_ = MAX_CONNECT_RETRY ;
        connect_timeout_ = 10; //default 10 secs
        reconnect_interval_micro_secs_ = 500000 ;//default 0.5 sec
        user_connect_cb_    = NULL;
        user_disconnect_cb_ = NULL;
        user_msg_cb_        = NULL;
        user_abort_cb_      = NULL;
        connected_ip_       =""; 
        connected_port_     =0 ; 
        is_connected_       =false;
    }
    virtual ~RedisHelper() {
        if( ctx_ ) {
            redisFree(ctx_);
            ctx_=NULL;
        }
        if(reply_){
            freeReplyObject(reply_); 
            reply_=NULL;
        }
    }

    ////////////////////////////////////////////////////////////////////////////
    //connecting to  master 
    bool ConnectServer (bool is_reconnect=false) 
    {
        is_connected_  = false;
        signal(SIGHUP, SIG_IGN);
        signal(SIGPIPE, SIG_IGN);
        char port_str [100];

        struct timeval timeout = { (long)connect_timeout_, 0 };
        auto  it_vec  = vec_ip_ports_.begin();
        for(; it_vec != vec_ip_ports_.end(); ++it_vec){ //------ for
            if ( ctx_ ) {
                redisFree(ctx_);
                ctx_=NULL;
            }
            ctx_ = redisConnectWithTimeout(it_vec->ip.c_str(), it_vec->port, timeout);
            snprintf(port_str, sizeof(port_str),"%ld", it_vec->port);
            if ( ctx_ == 0x00 || ctx_->err ) {
                if ( ctx_ ) {
                    err_msg_ =  it_vec->ip+ std::string(":") + 
                        std::string(port_str) +std::string(",") + ctx_->errstr;
                    redisFree(ctx_);
                    ctx_=NULL;
                    DEBUG_ELOG (err_msg_ );
                    if(user_msg_cb_){
                        user_msg_cb_(err_msg_.c_str());
                    }
                } else {
                    err_msg_ = it_vec->ip +std::string(",") + 
                        std::string("failed to allocate redis context");
                    DEBUG_ELOG (err_msg_ );
                    if(user_msg_cb_){
                        user_msg_cb_(err_msg_.c_str());
                    }
                }
                continue;
                //return false;  
            }
            //------------------- check role
            if(reply_){
                freeReplyObject(reply_); 
                reply_=NULL;
            }
            reply_ = (redisReply*)redisCommand(ctx_, "ROLE");
            if(reply_ == NULL){
                DEBUG_RED_LOG("ROLE command returns null reply");
                return false;
            }
            for ( unsigned int fld=0 ; fld < reply_->elements ; fld++ ) {
                if ( reply_->element[fld]->type != REDIS_REPLY_STRING ) {
                    continue;
                }
                DEBUG_LOG ("reply role -->" << reply_->element[fld]->str );
                if (!strcmp(reply_->element[fld]->str, "master") ) {
                    DEBUG_LOG("connect ok :"<<it_vec->ip<<","<<it_vec->port);
                    is_connected_  = true;
                    //user_connect_cb_ --> master only
                    if(user_connect_cb_){
                        if(is_reconnect ){ 
                            //true --> reconnect flag
                            user_connect_cb_(connected_ip_.c_str(),connected_port_,
                                             it_vec->ip.c_str(),it_vec->port,true); 
                        }else{
                            user_connect_cb_(connected_ip_.c_str(),connected_port_,
                                             it_vec->ip.c_str(),it_vec->port,false); 
                        }
                    }
                    connected_ip_   = it_vec->ip;
                    connected_port_ = it_vec->port;
                    freeReplyObject(reply_); 
                    reply_=NULL;
                    return true;
                }
            }//for
            freeReplyObject(reply_); 
            reply_=NULL;
            err_msg_ = it_vec->ip +std::string(",") +std::string(port_str)+
                std::string(" : ") + std::string("connected but NOT master");
            DEBUG_LOG (err_msg_ );
            if(user_msg_cb_){
                user_msg_cb_(err_msg_.c_str());
            }
        } //------ for
        return false;
    }
    ////////////////////////////////////////////////////////////////////////////
    void   SetConnectTimeoutSecs(size_t secs) { connect_timeout_ = secs;} 

    ////////////////////////////////////////////////////////////////////////////
    bool DoCommand(const char* format, ...) {
        if(reply_){
            freeReplyObject(reply_); 
            reply_=NULL;
        }
        while(true){
            va_list ap;
            va_start(ap, format);
            reply_ = (redisReply*)redisvCommand(ctx_, format, ap);

            if ( !reply_ || reply_->type == REDIS_REPLY_ERROR ) {
                char temp_str [10];
                snprintf(temp_str,sizeof(temp_str),"%d", ctx_->err);
                err_msg_ = std::string("failed,") + ctx_->errstr + std::string(",") + 
                    std::string(temp_str);
                DEBUG_ELOG ( format <<":"<<err_msg_ << ", ctx_->err=" << ctx_->err );
                if(reply_){
                    freeReplyObject(reply_); 
                    reply_=NULL;
                }
                if( IsThisConnectionError(ctx_->err) ){ //reconnect and retry
                    is_connected_ = false;
                    if(user_disconnect_cb_){
                        user_disconnect_cb_(connected_ip_.c_str(), 
                                            connected_port_, ctx_->errstr );
                    }
                    char tmp_msg [128];
                    snprintf(tmp_msg,sizeof(tmp_msg),"connect error :%s %ld, %s",
                            connected_ip_.c_str(), connected_port_,ctx_->errstr);
                    if(user_msg_cb_){
                        user_msg_cb_(tmp_msg);
                    }
                    if(!Reconnect()){
                        va_end(ap);
                        return false; 
                    }else{
                        va_end(ap);
                        continue;
                    }
                }
                va_end(ap);
                return false;
            }else{ //if error
                va_end(ap);
                break;
            }
        } //while
        return true;
    }

    ////////////////////////////////////////////////////////////////////////////
    //return false cases : Out of memory, Invalid format string, 
    //sdscatlen(C dynamic strings library) fail
    bool AppendCmdPipeline(const char* format, ... )
    {
        if(ctx_==NULL){
            err_msg_ = "ctx_==NULL";
            DEBUG_ELOG (err_msg_ );
            return false;
        }
        va_list ap;
        va_start(ap, format);
        if(REDIS_OK !=redisvAppendCommand(ctx_,format,ap)){
            va_end(ap);
            err_msg_ = std::string("error : redisAppendCommand,") + ctx_->errstr;
            DEBUG_ELOG (err_msg_ );
            return false;
        }
        va_end(ap);
        pipe_appended_cnt_++;
        return true;
    } 

    ////////////////////////////////////////////////////////////////////////////
    bool EndCmdPipeline(bool do_reconnect = true)
    {
        DEBUG_LOG ("pipe_appended_cnt_ = " << pipe_appended_cnt_ );
        if(reply_){
            freeReplyObject(reply_); 
            reply_=NULL;
        }
        bool is_error = false;
        while( pipe_appended_cnt_ > 0 ){
            int result = redisGetReply(ctx_,(void**) &reply_); 
            if(REDIS_OK != result){
                is_connected_ = false;
                if( IsThisConnectionError(ctx_->err) ){ //reconnect and retry
                    if(user_disconnect_cb_){
                        user_disconnect_cb_(connected_ip_.c_str(), 
                                            connected_port_,ctx_->errstr);
                    }
                    char tmp_msg [128];
                    snprintf(tmp_msg,sizeof(tmp_msg),"(%s:%d) redisGetReply failed :%s %ld, "
                            "pipe_appended_cnt_= %ld, %s",
                            __func__,__LINE__, connected_ip_.c_str(), connected_port_,
                            pipe_appended_cnt_, ctx_->errstr);
                    err_msg_ = std::string(tmp_msg) + std::string(":") + 
                               std::string(strerror(errno)) ;
                    DEBUG_ELOG (err_msg_);
                    if(user_msg_cb_){
                        user_msg_cb_(tmp_msg);
                    }
                    if(do_reconnect){
                        DEBUG_ELOG ("call Reconnect()");
                        if(Reconnect()){  
                            DEBUG_GREEN_LOG ("Reconnect() success");
                        }
                    }
                }
                return false; 
            }
            if ( !reply_ || reply_->type == REDIS_REPLY_ERROR ) {
                DEBUG_LOG ("ctx_->err= " << ctx_->err );
                char tmp_port [8];    
                snprintf(tmp_port,sizeof(tmp_port),"%ld", connected_port_);
                err_msg_ = connected_ip_ + std::string(":") + std::string(tmp_port) +
                    std::string(" -> failed,") + std::string(reply_->str);
                DEBUG_ELOG (err_msg_ );
                is_error=true; 
            }
            if(reply_){
                freeReplyObject(reply_); 
                reply_=NULL;
            }
            pipe_appended_cnt_--;
        } //while
        if(is_error){
            DEBUG_ELOG ("false" );
            return false;
        }
        return true;
    } 

    ////////////////////////////////////////////////////////////////////////////
    redisReply* GetReply() { 
        return reply_; 
    }
    ////////////////////////////////////////////////////////////////////////////
    void SetIpsPorts (const char* ip, size_t port){
        ConnIpPort ip_port;
        ip_port.ip   = std::string(ip);
        ip_port.port = port;
        vec_ip_ports_.push_back(ip_port);
        DEBUG_LOG("ip ="<<ip << ",port=" <<port << ",cnt=" <<vec_ip_ports_.size());
    }
    ////////////////////////////////////////////////////////////////////////////
    //reconnect only  --> blocking call 
    bool Reconnect() {
        size_t retry = 0;
        char tmp_msg [128];
        while(true){
            if(max_reconn_retry_>0){
                retry++;
            }
            if(!ConnectServer(true )) { //true -> is_reconnect
                snprintf(tmp_msg,sizeof(tmp_msg),"connect failed, retry:%s %ld",
                         connected_ip_.c_str(), connected_port_);
                DEBUG_ELOG (tmp_msg);
                usleep(reconnect_interval_micro_secs_); 
            }else{
                snprintf(tmp_msg,sizeof(tmp_msg),"reconnect OK :%s:%ld ",
                         connected_ip_.c_str(),connected_port_ );
                if(user_msg_cb_){
                    user_msg_cb_(tmp_msg);
                }
                DEBUG_GREEN_LOG (tmp_msg);
                return true;
            }
            if(user_abort_cb_!=NULL && user_abort_cb_()){
                snprintf(tmp_msg,sizeof(tmp_msg),"abort reconnect :%s %ld",
                         connected_ip_.c_str(), connected_port_);
                if(user_msg_cb_){
                    user_msg_cb_(tmp_msg);
                }
                DEBUG_ELOG (tmp_msg);
                break;
            }
            if(max_reconn_retry_>0 && retry >= max_reconn_retry_){
                snprintf(tmp_msg,sizeof(tmp_msg),"reconnect failed, give up :%s %ld",
                         connected_ip_.c_str(), connected_port_);
                if(user_msg_cb_){
                    user_msg_cb_(tmp_msg);
                }
                DEBUG_ELOG (tmp_msg);
                break;
            }
        }//while
        return false;
    }

    ////////////////////////////////////////////////////////////////////////////
    void   SetReConnectMsgCallBack(MSG_CALLBACK cb) { 
        user_msg_cb_ = cb ;
    }
    void   SetConnectCallBack(CONNECT_CALLBACK cb) { 
        user_connect_cb_ = cb ;
    }
    void   SetDisConnectCallBack(DISCONNECT_CALLBACK dis_cb) { 
        user_disconnect_cb_ = dis_cb ;
    }
    //if user want to stop reconnect , return true in callback
    void   SetAbortReconnectCallBack(ABRT_CALLBACK cb) { 
        user_abort_cb_ = cb ;
    }
    size_t GetMaxReconnTryCnt (){ return max_reconn_retry_ ;}  

    //max_cnt = 0 --> infinite retry 
    void   SetMaxReconnTryCnt (size_t max_cnt){ max_reconn_retry_ = max_cnt ;}  
    void   SetReconnectInterval (size_t micro_secs){ reconnect_interval_micro_secs_ = micro_secs ;}
    size_t GetAppendedCmdCnt  (){ return pipe_appended_cnt_  ; }
    void   ReSetAppendedCmdCnt(){ pipe_appended_cnt_ =0 ; }
    const char* GetLastErrMsg (){ return err_msg_.c_str(); }
    const char* GetSvrIp() { return connected_ip_.c_str(); }
    size_t GetSvrPort()    { return connected_port_ ; } 
    bool   IsConnected()   { return is_connected_ ; } 
    bool IsThisConnectionError() {
        return IsThisConnectionError(ctx_->err);
    }
  protected:

    ////////////////////////////////////////////////////////////////////////////
    bool IsThisConnectionError(int err) {
        if(err==REDIS_ERR_IO || err==REDIS_ERR_EOF || err==REDIS_ERR_TIMEOUT){
            DEBUG_LOG ( "connection error : " << err );
            return true;
        }
        DEBUG_LOG ( "not a connection error : " << err );
        return false;
    }

  protected:
    CONNECT_CALLBACK    user_connect_cb_   ;
    DISCONNECT_CALLBACK user_disconnect_cb_;
    MSG_CALLBACK        user_msg_cb_       ;
    ABRT_CALLBACK       user_abort_cb_     ; 
    redisContext*       ctx_               ; //not thread-safe
    redisReply*         reply_             ;
    size_t              connect_timeout_   ;
    size_t              max_reconn_retry_  ;
    size_t              pipe_appended_cnt_ ;
    std::string         err_msg_           ;
    std::string         connected_ip_      ; 
    size_t              connected_port_    ; 
    size_t              reconnect_interval_micro_secs_ ; 
    bool                is_connected_      ;
    VecConnIpPorts      vec_ip_ports_      ;

  public:
    int                 user_specific_     ;

};  
typedef std::vector<RedisHelper*> VecRedisHelperPtr ;
typedef VecRedisHelperPtr::iterator ItRedisHelperPtr ;

#endif // REDIS_HELPER_HPP


