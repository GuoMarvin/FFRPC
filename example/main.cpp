#include <stdio.h>
#include "base/daemon_tool.h"
#include "base/arg_helper.h"
#include "base/strtool.h"
#include "base/smart_ptr.h"

#include "rpc/ffrpc.h"
#include "rpc/ffbroker.h"
#include "base/log.h"

using namespace ff;

//! 定义echo 接口的消息， in_t代表输入消息，out_t代表的结果消息
//! 提醒大家的是，这里没有为echo_t定义神马cmd，也没有制定其名称，ffmsg_t会自动能够获取echo_t的名称
struct echo_t
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
    //! echo接口，返回请求的发送的消息ffreq_t可以提供两个模板参数，第一个表示输入的消息（请求者发送的）
    //! 第二个模板参数表示该接口要返回的结果消息类型
    void echo(ffreq_t<echo_t::in_t, echo_t::out_t>& req_)
    {
        echo_t::out_t out;
        out.data = req_.arg.data;
        LOGDEBUG(("XX", "foo_t::echo: %s", req_.arg.data.c_str()));
        req_.response(out);
    }
    //! 远程调用接口，可以指定回调函数（也可以留空），同样使用ffreq_t指定输入消息类型，并且可以使用lambda绑定参数
    void echo_callback(ffreq_t<echo_t::out_t>& req_, int index)
    {
        LOGDEBUG(("XX", "%s %s %d", __FUNCTION__, req_.arg.data.c_str(), index));
    }
};

int main(int argc, char* argv[])
{
    //! 美丽的日志组件，shell输出是彩色滴！！
    LOG.start("-log_path ./log -log_filename log -log_class XX,BROKER,FFRPC -log_print_screen true -log_print_file true -log_level 6");

    //! 启动broker，负责网络相关的操作，如消息转发，节点注册，重连等
    ffbroker_t ffbroker;
    ffbroker.open("app -l tcp://127.0.0.1:10241");

    //! broker客户端，可以注册到broker，并注册服务以及接口，也可以远程调用其他节点的接口
    ffrpc_t ffrpc_service("echo");
    foo_t foo;
    ffrpc_service.reg(&foo_t::echo, &foo);
    ffrpc_service.open("app -broker tcp://127.0.0.1:10241");
    
    ffrpc_t ffrpc_client;
    ffrpc_client.open("app -broker tcp://127.0.0.1:10241");
    echo_t::in_t in;
    in.data = "helloworld";

    //! 你没有看见get_type_name定义，但是他确定存在
    printf("测试获取类名:%s\n", in.get_type_name());//输出为:测试获取类名:echo_t::in_t
    
    for (int i = 0; i < 100; ++i)
    {
        //! 如你所想，echo接口被调用，然后echo_callback被调用，每一秒重复该过程
        ffrpc_client.call("echo", in, ffrpc_ops_t::gen_callback(&foo_t::echo_callback, &foo, i));
        sleep(1);
    }
    
    sleep(300);
    ffbroker.close();
    return 0;
}
