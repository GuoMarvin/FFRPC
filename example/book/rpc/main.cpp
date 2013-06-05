#include <stdio.h>
#include "base/daemon_tool.h"
#include "base/arg_helper.h"
#include "base/strtool.h"
#include "base/smart_ptr.h"

#include "rpc/ffrpc.h"
#include "rpc/ffbroker.h"
#include "base/log.h"

using namespace ff;


struct echo_t//!broker 转发消息
{
    struct in_t: public ffmsg_t<in_t>
    {
        void encode()
        {
            encoder() << data;
        }
        void decode()
        {
            decoder() >> data;
        }
        string data;
    };
    struct out_t: public ffmsg_t<out_t>
    {
        void encode()
        {
            encoder() << data;
        }
        void decode()
        {
            decoder() >> data;
        }
        string data;
    };
};


struct foo_t
{
    void echo(ffreq_t<echo_t::in_t, echo_t::out_t>& req_)
    {
        echo_t::out_t out;
        out.data = req_.arg.data;
        LOGDEBUG(("XX", "foo_t::echo: %s", req_.arg.data.c_str()));
        req_.response(out);
    }
    void echo_callback(ffreq_t<echo_t::out_t>& req_, int index)
    {
        LOGDEBUG(("XX", "%s %s %d", __FUNCTION__, req_.arg.data.c_str(), index));
    }
};

int main(int argc, char* argv[])
{
    LOG.start("-log_path ./log -log_filename log -log_class XX,BROKER,FFRPC -log_print_screen true -log_print_file true -log_level 6");

    //! 启动broker，负责网络相关的操作，如消息转发，节点注册，重连等
    ffbroker_t ffbroker;
    ffbroker.open("app -l tcp://127.0.0.1:10241");

    //! broker客户端，可以注册到broker，并注册服务以及接口，也可以远程调用其他节点的接口
    ffrpc_t ffrpc("echo");
    foo_t foo;
    ffrpc.reg(&foo_t::echo, &foo);
    
    ffrpc.open("app -broker tcp://127.0.0.1:10241");
    sleep(1);
    
    echo_t::in_t in;
    in.data = "helloworld";
    
    for (int i = 0; i < 100; ++i)
    {
        ffrpc.call("echo", in, ffrpc_ops_t::gen_callback(&foo_t::echo_callback, &foo, i));
        sleep(1);
    }
    
    sleep(300);
    ffbroker.close();
    return 0;
    
    
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
