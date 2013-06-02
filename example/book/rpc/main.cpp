#include <stdio.h>
#include "base/daemon_tool.h"
#include "base/arg_helper.h"
#include "base/strtool.h"
#include "base/smart_ptr.h"

#include "rpc/ffrpc.h"
#include "rpc/ffbroker.h"

using namespace ff;


struct echo_t//!broker 转发消息
{
    struct in_t: public ffmsg_t<in_t>
    {
        virtual string encode()
        {
            return (init_encoder() << node_id << msg_id << callback_id << body).get_buff() ;
        }
        virtual void decode(const string& src_buff_)
        {
            init_decoder(src_buff_) >> node_id >> msg_id >> callback_id >> body;
        }
        uint32_t                    node_id;//! 需要转发到哪个节点上
        uint32_t                    msg_id;//! 调用的是哪个接口
        uint32_t                    callback_id;
        string                      body;
    };
};


struct foo_t
{
    static void print(ffreq_t<echo_t::in_t>& req_)
    {
        //!
    }
    void echo(ffreq_t<echo_t::in_t>& req_)
    {
        //!
    }
};
int main(int argc, char* argv[])
{
    ffrpc_t ffrpc("echo");
    foo_t foo;
    ffrpc.reg(&foo_t::print)
         .reg(&foo_t::echo, &foo);
/*    
    if (argc == 1)
    {
        printf("usage: app -broker -client -l tcp://127.0.0.1:10241 -service db_service@1-4,logic_service@1-4\n");
        return 1;
    }
    arg_helper_t arg_helper(argc, argv);
    if (arg_helper.is_enable_option("-broker"))
    {
        broker_application_t::run(argc, argv);
    }

    if (arg_helper.is_enable_option("-d"))
    {
        daemon_tool_t::daemon();
    }
    
  */  
    return 0;
}
