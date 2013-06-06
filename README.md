# FFRPC

FFRPC 已经陆陆续续开发了1年，6月6日这天终于完成了我比较满意的版本，暂称之为 V0.2，FFRPC实现了一个C++版本
的异步进程间通讯库。我本身是做游戏服务器程序的，在服务器程序领域，系统是分布式的，各个节点需要异步的进行通信，
我的初衷是开发一个易用、易测试的进程间socket通信组件。实际上FFRPC 已经是一个框架。

## FFRPC 主要特性
 * FFRPC 采用Epoll Edge Trigger模式，这里特别提一下ET是因为在异步工作模式，ET方式才是epoll最简单也是最高效的方式
 网上的很多帖子写LT简单易用，那纯碎是没有理解ET的精髓之所在，如果读者想要从ffrpc中探究一下ET的奥妙，提醒读者的是
 请把Epoll 看成一个状态机！FFRPC 采用Broker模式，这样的好处是 Scalability!! 在游戏领域的开发者一定很熟悉Master/Gateway/Logic Server的概念，
 实际上Master 实际上扮演的Broker master的角色，而gateway扮演的是Broker slave的角色，Broker Slave负责转发客户端的
 请求到Logic Service，提供一个转发层虽然会增加延迟，但是系统变得可扩展，大大提高了吞吐量，这就是Scalability!! 
 而Broker master负责管理所有的Master Slave，负责负载均衡。不同的client分配不同的Broker SLave。
 * FFRPC 就是基于以上的思路，有如下四个关键的概念：
 	* 一：broker master 负责负载均衡，同步所有节点的信息，所有的slave broker和rpc service/ rpc cleint都要连接broker master。
 	* 二：slave broker负责完成service和client间转发消息，如果service、client和broker在同一进程，那么直接在内存间投递消息，
  这是v0。2的重要的优化，v0。1时没有此功能，网友很多反应这个问题，看来大伙对优化还是太敏感！
  另一个创新之处在于ffmsg_t，封装了消息的序列化和反序列化，我已经厌倦了protobuff，如果你也研究了为每个消息定义cmd
  和为cmd写switch（有些人可能已经用上注册回调函数，但还有更好用的）。实际上定义消息结构体时一个消息本身就是独一无二的，
  所以为什么我们还要给消息在定义一个cmd呢？比如定义了struct echo_t{int a;}消息，echo_t名称本身就是独一无二的，否则编译
  器肯定报错了，那么为什么不直接用echo_t这个名称作为cmd呢？在FFRPC中可以使用TYPE_NAME(echo_t)获得消息体名称字符串，
  是滴TYPE_NAME是一个很有意思的实现，c++中并没哟关键字可以获取一个类的名称，但是所有的编译器都实际上已经提供了这个功能！
  详情请看源码。有读者可能会纠结使用消息体结构的名称做cmd固然省事，但是浪费了流量！32位的cmd总是比字符串省流量，是的这个
  结论虽然我很不喜欢（我总是懒的优化，除非...被逼的），但是他是对的！ffrpc中很好的解决了这个问题，当每个节点初始化时都要
  注册到broker master，这时所有的消息都会在master中分配一个唯一的msg id，这样就可以用整数1代表echo_t结构了，由于每个节点
  都知道echo_t到1的映射，所以程序员再也不用手动定义cmd了，broker唯一初始化时动态定义。
 	* 三:ffrpc service，提供接口的模块，也就就是服务端，通过ffrpc类注册的接口基于异步模式，推荐的模式是每个消息都返回
  一个结果消息
 	* 四：ffrpc client是调用的ffrpc service的模块，基于异步模式，记住服务名成和消息名称唯一的确定一个接口，这个c++的类和类接口
  概念是一致的，而且调用远程接口时可以指定回调函数，而且回调函数还支持lambda参数绑定！
 * 想快速见证ffrpc库的魅力可以小看如下的示例，只要你有linux系统，可以1分钟内测试这个示例，ffrpc没有其他依赖，提醒你的是
  FFRPC的日志组件是彩色的哦！
 
## 示例代码

``` c++
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

```


## 总结
* ffrpc中broker、client、service可以启动在不同的进程，如果在同一进程，那么直接内存间投递消息
* ffrpc 每个实例单独启动一个线程和任务队列，保证service和client的操作都是有序、线程安全的。
* 如果你研究过protobuff、thrift、zeromq、ice等等类库/框架, 更要试用一下ffrpc。


