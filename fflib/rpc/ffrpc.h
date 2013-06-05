//! 消息发送管理
#ifndef _FF_FFRPC_H_
#define _FF_FFRPC_H_

#include <string>
#include <map>
#include <vector>
#include <set>
using namespace std;

#include "net/msg_handler_i.h"
#include "base/task_queue_impl.h"
#include "base/ffslot.h"
#include "net/codec.h"
#include "base/thread.h"
#include "rpc/ffrpc_ops.h"
#include "net/msg_sender.h"
#include "base/timer_service.h"

namespace ff {

class ffrpc_t: public msg_handler_i, ffresponser_t
{
    struct session_data_t;
    struct slave_broker_info_t;
    struct broker_client_info_t;
public:
    ffrpc_t(const string& service_name_);
    virtual ~ffrpc_t();

    int open(const string& opt_);
    
    //! 处理连接断开
    int handle_broken(socket_ptr_t sock_);
    //! 处理消息
    int handle_msg(const message_t& msg_, socket_ptr_t sock_);

    //! 注册接口
    template <typename R, typename IN, typename OUT>
    ffrpc_t& reg(R (*)(ffreq_t<IN, OUT>&));
    template <typename R, typename CLASS_TYPE, typename IN, typename OUT>
    ffrpc_t& reg(R (CLASS_TYPE::*)(ffreq_t<IN, OUT>&), CLASS_TYPE* obj);
    
    //! 调用远程的接口
    template <typename T>
    int call(const string& name_, T& req_, ffslot_t::callback_t* callback_ = NULL);
    
    uint32_t get_callback_id() { return ++m_callback_id; }
    //! call 接口的实现函数，call会将请求投递到该线程，保证所有请求有序
    int call_impl(const string& service_name_, const string& msg_name_, const string& body_, ffslot_t::callback_t* callback_);
    //! 调用接口后，需要回调消息给请求者
    virtual void response(uint32_t node_id_, uint32_t msg_id_, uint32_t callback_id_, const string& body_);
    //! 通过node id 发送消息给broker
    void send_to_broker_by_nodeid(uint32_t node_id_, const string& body_, uint32_t msg_id_ = 0, uint32_t callback_id_ = 0);
    //! 获取任务队列对象
    task_queue_t& get_tq();
    //! 定时重连 broker master
    void timer_reconnect_broker();
    //! 调用消息对应的回调函数
    int trigger_callback(broker_route_t::in_t& msg_);
private:
    //! 处理连接断开
    int handle_broken_impl(socket_ptr_t sock_);
    //! 处理消息
    int handle_msg_impl(const message_t& msg_, socket_ptr_t sock_);

    //!  register all interface
    int register_all_interface(socket_ptr_t sock);
    //! 处理broker同步消息，broker master 会把master上注册的所有信息同步给所有的client
    int handle_broker_sync_data(broker_sync_all_registered_data_t::out_t& msg_, socket_ptr_t sock_);
    int handle_broker_route_msg(broker_route_t::in_t& msg_, socket_ptr_t sock_);
    

    //! 连接到broker master
    socket_ptr_t connect_to_broker(const string& host_, uint32_t node_id_);
    
private:
    string                                  m_host;
    timer_service_t                         m_timer;
    string                                  m_service_name;//! 注册的服务名称
    uint32_t                                m_node_id;     //! 通过注册broker，分配的node id
    uint32_t                                m_bind_broker_id;//! 若不为0， 则为固定绑定一个node id
    uint32_t                                m_callback_id;//! 回调函数的唯一id值
    task_queue_t                            m_tq;
    thread_t                                m_thread;
    ffslot_t                                m_ffslot;//! 注册broker 消息的回调函数
    ffslot_t                                m_ffslot_interface;//! 通过reg 注册的接口会暂时的存放在这里
    ffslot_t                                m_ffslot_callback;//! 
    socket_ptr_t                            m_master_broker_sock;
    map<string, ffslot_t::callback_t*>      m_reg_iterface;
    map<uint32_t, slave_broker_info_t>      m_slave_broker_sockets;//! node id -> info
    map<string, uint32_t>                   m_msg2id;
    map<uint32_t, broker_client_info_t>     m_broker_client_info;//! node id -> service
    map<string, uint32_t>                   m_broker_client_name2nodeid;//! service name -> service node id
};

//! 注册接口
template <typename R, typename IN, typename OUT>
ffrpc_t& ffrpc_t::reg(R (*func_)(ffreq_t<IN, OUT>&))
{
    m_reg_iterface[TYPE_NAME(IN)] = ffrpc_ops_t::gen_callback(func_);
    return *this;
}
template <typename R, typename CLASS_TYPE, typename IN, typename OUT>
ffrpc_t& ffrpc_t::reg(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&), CLASS_TYPE* obj)
{
    m_reg_iterface[TYPE_NAME(IN)] = ffrpc_ops_t::gen_callback(func_, obj);
    return *this;
}

struct ffrpc_t::session_data_t
{
    session_data_t(uint32_t n = 0):
        node_id(n)
    {}
    uint32_t get_node_id() { return node_id; }
    uint32_t node_id;
};
struct ffrpc_t::slave_broker_info_t
{
    slave_broker_info_t():
        sock(NULL)
    {}
    string          host;
    socket_ptr_t    sock;
};
struct ffrpc_t::broker_client_info_t
{
    broker_client_info_t():
        bind_broker_id(0),
        service_id(0)
    {}
    uint32_t bind_broker_id;
    string   service_name;
    uint16_t service_id;
};

//! 调用远程的接口
template <typename T>
int ffrpc_t::call(const string& name_, T& req_, ffslot_t::callback_t* callback_)
{
    m_tq.produce(task_binder_t::gen(&ffrpc_t::call_impl, this, name_, TYPE_NAME(T), req_.encode_data(), callback_));   
    return 0;
}

}
#endif
