/****************************************************************************
 Copyright (c) 2019 ko jung hyun
 
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

#ifndef __REDIS_HELPER_HPP__
#define __REDIS_HELPER_HPP__

#include <hiredis/hiredis.h>
#include <unistd.h> //sleep
#include <string.h>
#include <signal.h>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

///////////////////////////////////////////////////////////////////////////////
const size_t MAX_CMD_LEN = 1024 * 1024 ; //1 mb
const size_t MAX_CONNECT_RETRY = 50;

typedef enum _EnumRoleReplication_ {
    ROLE_MASTER    =0,
    ROLE_REPLICA   //slave
} EnumRoleReplication  ;

///////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////
class RedisHelper 
{
  public:
    RedisHelper() {
        ctx_=NULL;
        reply_=NULL;
        is_connected_=false;
        pipe_appended_cnt_ = 0;
        max_reconn_retry_ = MAX_CONNECT_RETRY ;
        connect_timeout_ = 10; //default 10 secs
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

    ///////////////////////////////////////////////////////////////////////////////
    bool ConnectServer (const char* hostname, const size_t port, 
                        EnumRoleReplication role=ROLE_MASTER ) 
    {
        signal(SIGHUP, SIG_IGN);
        signal(SIGPIPE, SIG_IGN);

        if(role != ROLE_MASTER && role != ROLE_REPLICA){
            err_msg_ = "invalied role";
            DEBUG_ELOG (err_msg_ );
            return false;
        }
        my_server_role_ = role;
        redis_svr_ip_ = hostname;
        redis_svr_port_ = port;

        struct timeval timeout = { (long)connect_timeout_, 0 };
        if ( ctx_ ) {
            redisFree(ctx_);
        }
        ctx_ = redisConnectWithTimeout(redis_svr_ip_.c_str(), redis_svr_port_, timeout);
        if ( ctx_ == 0x00 || ctx_->err ) {
            if ( ctx_ ) {
                char temp_buf [100];
                snprintf(temp_buf, sizeof(temp_buf),"%ld", port);
                err_msg_ = std::string(hostname) + std::string(":") + 
                    std::string(temp_buf) +std::string(",") + ctx_->errstr;
                redisFree(ctx_);
                ctx_=NULL;
            } else {
                err_msg_ = std::string(hostname) +std::string(",") + 
                    std::string("failed to allocate redis context");
            }
            DEBUG_ELOG (err_msg_ );
            is_connected_ =false;
            return false;  
        }
        if(reply_){
            freeReplyObject(reply_); 
            reply_=NULL;
        }
        //for replica ==> check wanted role.
        reply_ = (redisReply*)redisCommand(ctx_, "ROLE");
        for ( unsigned int fld=0 ; fld < reply_->elements ; fld++ ) {
            if ( reply_->element[fld]->type != REDIS_REPLY_STRING ) {
                continue;
            }
            DEBUG_LOG ( "my_server_role_=>" << my_server_role_
                    << ", reply role -->" << reply_->element[fld]->str );
            if ( my_server_role_ == ROLE_MASTER && 
                    !strcmp(reply_->element[fld]->str, "master") ) {
                is_connected_ =true;
                freeReplyObject(reply_);
                reply_=NULL;
                return true;
            }
            else if ( my_server_role_ == ROLE_REPLICA && 
                    !strcmp(reply_->element[fld]->str, "slave") ) {
                is_connected_ =true;
                freeReplyObject(reply_);
                reply_=NULL;
                return true;
            }
        }
        err_msg_ = std::string(hostname) +std::string(",") + 
            std::string("connected but role not exists");//role not found...
        DEBUG_ELOG (err_msg_ );
        is_connected_ =false;
        return false;
    }

    ///////////////////////////////////////////////////////////////////////////////
    // host:port,host:port
    bool ConnectServerOneOfThese(const std::string& servers,
            EnumRoleReplication role = ROLE_MASTER ) 
    {
        if(role != ROLE_MASTER && role != ROLE_REPLICA){
            err_msg_ = "invalied role";
            DEBUG_ELOG (err_msg_ );
            return false;
        }
        my_server_role_ = role;
        std::istringstream stream(servers);
        std::string endpoint;

        while (std::getline(stream, endpoint, ',')) {
            if( endpoint.size() ) {
                std::size_t pos = endpoint.find(":");
                if ( pos == std::string::npos ) {
                    err_msg_ = std::string("invalid connection string");
                    return false;
                }
                std::string token = endpoint.substr(pos+1);
                std::string host  = endpoint.substr(0, endpoint.size() - token.size() - 1);
                int port = atoi(token.c_str());
                if ( ConnectServer(host.c_str(), port, role) ) {
                    return true;
                }
            }
        }
        err_msg_ = std::string("connect failed");
        return false;
    }


    // zscan zadd_key1 0
    // zscan zadd_key1 0 match za*
    void   SetConnectTimeoutSecs(size_t secs) { connect_timeout_ = secs;} 
    ///////////////////////////////////////////////////////////////////////////////
    bool ZAddTimeStamp (const char* key, const char* value )
    {
        time_t cur_time = time(NULL);
        DEBUG_LOG("zadd : " << cur_time );
        return DoCommand("ZADD %s %ld %s", key, cur_time, value);
    }

    ///////////////////////////////////////////////////////////////////////////////
    bool DoCommand(const char* format, ...) 
    {        
        if(reply_){
            freeReplyObject(reply_); 
            reply_=NULL;
        }
        while(true){
            va_list ap;
            va_start(ap, format);
            reply_ = (redisReply*)redisvCommand(ctx_, format, ap);

            if ( !reply_ || reply_->type == REDIS_REPLY_ERROR ) {
                err_msg_ = std::string("failed,") + ctx_->errstr + std::string(",") + 
                    std::to_string(ctx_->err);
                DEBUG_ELOG (err_msg_ << ", ctx_->err=" << ctx_->err );
                if(reply_){
                    freeReplyObject(reply_); 
                    reply_=NULL;
                }
                if( IsThisConnectionError(ctx_->err) ){ //reconnect and retry
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

    ///////////////////////////////////////////////////////////////////////////////
    bool AppendCmdPipeline(const char* format, ... )
    {
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

    ///////////////////////////////////////////////////////////////////////////////
    //XXX this function set reply null.
    bool EndCmdPipeline()
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
                err_msg_ = std::string("error : redisGetReply,") + ctx_->errstr;
                DEBUG_ELOG (err_msg_ << ", code=" <<result );
                return false;
            }
            if ( !reply_ || reply_->type == REDIS_REPLY_ERROR ) {
                DEBUG_LOG ("ctx_->err= " << ctx_->err );
                //DEBUG_LOG ("reply : type= " << reply_->type << ", str=" << reply_->str );
                err_msg_ = std::string("failed,") +  std::string(reply_->str);
                DEBUG_ELOG (err_msg_ );
                is_error=true;
            }
            if(reply_){
                freeReplyObject(reply_); 
                reply_=NULL;
            }
            pipe_appended_cnt_--;
        }
        if(is_error){
            DEBUG_ELOG ("false" );
            return false;
        }
        return true;
    } 

    ///////////////////////////////////////////////////////////////////////////////
    redisReply* GetReply() 
    { 
        return reply_; 
    }

    bool   IsConnected        (){ return is_connected_  ;}  
    size_t GetMaxReconnTryCnt (){ return max_reconn_retry_ ;}  
    void   SetMaxReconnTryCnt (size_t max_cnt){ max_reconn_retry_ = max_cnt ;}  
    size_t GetAppendedCmdCnt  (){ return pipe_appended_cnt_  ; }
    const char* GetLastErrMsg (){ return err_msg_.c_str(); }
    EnumRoleReplication GetMyServerRole() { return my_server_role_ ; }

  protected:

    ///////////////////////////////////////////////////////////////////////////////
    bool IsThisConnectionError(int err)
    {
        if(err==REDIS_ERR_IO || err==REDIS_ERR_EOF || err==REDIS_ERR_TIMEOUT){
            DEBUG_LOG ( "connection error : " << err );
            return true;
        }
        DEBUG_LOG ( "not a connection error : " << err );
        return false;
    }

    ///////////////////////////////////////////////////////////////////////////////
    //XXX reconnect only  --> blocking call
    bool Reconnect()
    {
        is_connected_ = false;
        size_t retry = 0;
        while(true){
            if(!ConnectServer(redis_svr_ip_.c_str(), redis_svr_port_,my_server_role_)) {
                DEBUG_ELOG ("connect failed, retry..." );
                sleep(1);
            }else{
                DEBUG_GREEN_LOG ("connect OK" );
                is_connected_ = true;
                return true;
            }
            if(max_reconn_retry_>0 && retry >= max_reconn_retry_){
                DEBUG_ELOG (" !!!! connect failed, give up !!!!" );
                break;
            }
            if(max_reconn_retry_>0){
                retry++;
            }
        }//while
        return false;
    }

  protected:

    redisContext*       ctx_               ; //not thread-safe
    redisReply*         reply_             ;
    std::string         redis_svr_ip_      ;
    size_t              redis_svr_port_    ;
    bool                is_connected_      ;
    size_t              connect_timeout_   ;
    size_t              max_reconn_retry_  ;
    size_t              pipe_appended_cnt_ ;
    std::string         err_msg_           ;
    EnumRoleReplication my_server_role_    ;

};  

typedef std::vector<RedisHelper> VecMemDbHelper ;
typedef VecMemDbHelper::iterator ItMemDbHelper ;

#endif



