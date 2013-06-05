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
    //! 记录每个broker slave 的接口信息
    struct slave_broker_info_t: public ffmsg_t<slave_broker_info_t>
    {
        void encode()
        {
            encoder() << host;
        }
        void decode()
        {
            decoder() >> host;
        }
        string          host;
    };

    struct in_t: public ffmsg_t<in_t>
    {
        void encode()
        {
            encoder() << node_id << msg_id << callback_id << body << slave_broker_info;
        }
        void decode()
        {
            decoder() >> node_id >> msg_id >> callback_id >> body ;
        }
        uint32_t                    node_id;//! 需要转发到哪个节点上
        uint32_t                    msg_id;//! 调用的是哪个接口
        uint32_t                    callback_id;
        string                      body;
        vector<slave_broker_info_t>         slave_broker_info;
    };
};


struct foo_t
{
    static void print(ffreq_t<echo_t::in_t, echo_t::in_t>& req_)
    {
        //printf("TTTTTTTTTTTxxx=%s\n", TYPE_NAME(echo_t::in_t).c_str());
        LOGDEBUG(("XX", "FFFFF %s", req_.arg.body.c_str()));
        req_.response(req_.arg);
    }
    void echo(ffreq_t<echo_t::in_t>& req_, int& arg1_, const string& arg2_, int* pin, const char* pstr)
    {
        LOGDEBUG(("XX", "%s %s %d %s %d %s", __FUNCTION__, req_.arg.body.c_str(), arg1_, arg2_, *pin, pstr));
    }
};

struct broker_sync_all_registered_data_t2
{
    //! 记录每个broker slave 的接口信息
    struct slave_broker_info_t: public ffmsg_t<slave_broker_info_t>
    {
        void encode()
        {
            encoder() << host;
        }
        void decode()
        {
            decoder() >> host;
        }
        string          host;
    };

    struct broker_client_info_t: public ffmsg_t<broker_client_info_t>
    {
        void encode()
        {
            encoder() << bind_broker_id << service_name;
        }
        void decode()
        {
            decoder() >> bind_broker_id >> service_name;
        }
        //! 被绑定的节点broker node id
        uint32_t bind_broker_id;
        string   service_name;
    };
    struct out_t: public ffmsg_t<out_t>
    {
        out_t():
            node_id(0)
        {}
        void encode()
        {
            encoder() << node_id << msg2id ;//<< slave_broker_info << broker_client_info;
        }
        void decode()
        {
            decoder() >> node_id >> msg2id ;//>> slave_broker_info >> broker_client_info;
        }
        uint32_t                                node_id;//! 被分配的node id
        map<string, uint32_t>                   msg2id; //! 消息名称对应的消息id 值
        //!记录所有的broker slave 信息
        map<uint32_t, slave_broker_info_t>      slave_broker_info;//! node id -> broker slave
        //! 记录所有服务/接口信息
        map<uint32_t, broker_client_info_t>     broker_client_info;//! node id -> service
    };
};
int main(int argc, char* argv[])
{
    broker_sync_all_registered_data_t2::out_t msgtest, msgtest2;
    msgtest.node_id = 100;
    bin_encoder_t bin_encoder;
    uint32_t aa = 200;
    uint32_t b = 0;
    string str_c = "Xxxxx";
    bin_encoder << aa << str_c;
    string data = bin_encoder.get_buff();
    bin_decoder_t bin_decoder;
    bin_decoder.init(data);
    string str_d;
    bin_decoder >> b >> str_d;
    
    string datatest = msgtest.encode_data();
    msgtest2.decode_data(datatest);
    printf("xxx=%s id=%u, b=%u, d=%s\n", TYPE_NAME(echo_t::in_t).c_str(), msgtest2.node_id, b, str_d.c_str());
    LOG.start("-log_path ./log -log_filename log -log_class XX,BROKER,FFRPC -log_print_screen true -log_print_file true -log_level 6");
	//LOGDEBUG(("XX", "FFFFF"));


    ffbroker_t ffbroker;
    ffbroker.open("app -broker -client -l tcp://127.0.0.1:10241");
    
    ffrpc_t ffrpc("echo");
    foo_t foo;
    ffrpc.reg(&foo_t::print);
         //.reg(&foo_t::echo, &foo);
    
    ffrpc.open("app -broker -client -l tcp://127.0.0.1:10241");
    sleep(1);
    
    echo_t::in_t in;
    in.body = "helloworld";
    int a = 199;
    static int g_int = 33445;
    const char* pstr = "ohnice";
    ffrpc.call("echo", in, ffrpc_ops_t::gen_callback(&foo_t::echo, &foo, a, "nihao", &g_int, pstr));
    
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
