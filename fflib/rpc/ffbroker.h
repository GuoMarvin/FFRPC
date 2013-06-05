
#ifndef _FF_BROKER_H_
#define _FF_BROKER_H_

#include <assert.h>
#include <string>
#include <map>
#include <set>
using namespace std;

#include "net/msg_handler_i.h"
#include "base/task_queue_impl.h"
#include "base/ffslot.h"
#include "base/thread.h"
#include "base/smart_ptr.h"
#include "net/net_factory.h"
#include "rpc/ffrpc_ops.h"

namespace ff
{
class ffbroker_t: public msg_handler_i
{
    //! 每个连接都要分配一个session，用于记录该socket，对应的信息s
    struct session_data_t;
    //! 记录每个broker slave 的接口信息
    struct slave_broker_info_t;
    //! 记录每个broker client 的接口信息
    struct broker_client_info_t;
public:
    ffbroker_t();
    virtual ~ffbroker_t();

    //! 当有连接断开，则被回调
    int handle_broken(socket_ptr_t sock_);
    //! 当有消息到来，被回调
    int handle_msg(const message_t& msg_, socket_ptr_t sock_);

    int open(const string& opt_);
    int close();
    //! 分配一个nodeid
    uint32_t alloc_id();
    //! 获取任务队列对象
    task_queue_t& get_tq();
    //! 定时器
    timer_service_t& get_timer();
    
    //! 连接到broker master
    int connect_to_master_broker();
private:
    //! 当有连接断开，则被回调
    int handle_broken_impl(socket_ptr_t sock_);
    //! 当有消息到来，被回调
    int handle_msg_impl(const message_t& msg_, socket_ptr_t sock_);
    //! 同步所有的当前的注册接口信息
    int sync_all_register_info(socket_ptr_t sock_);
    //! 处理borker slave 注册消息
    int handle_slave_register(register_slave_broker_t::in_t& msg_, socket_ptr_t sock_);
    //! 处理borker client 注册消息
    int handle_client_register(register_broker_client_t::in_t& msg_, socket_ptr_t sock_);
    //! 转发消息
    int handle_route_msg(broker_route_t::in_t& msg_, socket_ptr_t sock_);
    //! 处理broker client连接到broker slave的消息
    int handle_client_register_slave_broker(register_client_to_slave_broker_t::in_t& msg_, socket_ptr_t sock_);
    //! 处理broker同步消息，broker master 会把master上注册的所有信息同步给所有的client
    int handle_broker_sync_data(broker_sync_all_registered_data_t::out_t& msg_, socket_ptr_t sock_);
    //! 判断是否是master broker
    bool is_master();
private:
    //!broker master 的host信息
    string                                  m_broker_host;
    //! 连接到master 的连接socket
    socket_ptr_t                            m_master_broker_sock;
    //! broker 分配的slave node id
    uint32_t                                m_node_id;
    timer_service_t                         m_timer;
    //! 用于分配nodeid
    uint32_t                                m_node_id_index;
    //! 记录所有注册到此节点上的连接
    task_queue_t                            m_tq;
    thread_t                                m_thread;
    //! 用于绑定回调函数
    ffslot_t                                m_ffslot;
    //! 记录所有的broker socket对应node id
    map<uint32_t, slave_broker_info_t>      m_slave_broker_sockets;
    //! 记录所有的消息名称对应的消息id值
    map<string, uint32_t>                   m_msg2id;
    //! 记录所有服务/接口信息
    map<uint32_t, broker_client_info_t>     m_broker_client_info;//! node id -> service
};

//! 每个连接都要分配一个session，用于记录该socket，对应的信息
struct ffbroker_t::session_data_t
{
    session_data_t(uint32_t n = 0):
        node_id(n)
    {}
    uint32_t get_node_id() { return node_id; }
    //! 被分配的唯一的节点id
    uint32_t node_id;
};
//! 记录每个broker client 的接口信息
struct ffbroker_t::broker_client_info_t
{
    broker_client_info_t():
        bind_broker_id(0),
        sock(NULL)
    {}
    //! 被绑定的节点broker node id
    uint32_t bind_broker_id;
    string   service_name;
    socket_ptr_t sock;
};
//! 记录每个broker slave 的接口信息
struct ffbroker_t::slave_broker_info_t
{
    slave_broker_info_t():
        sock(NULL)
    {}
    string          host;
    socket_ptr_t    sock;
};

}

#endif
