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

namespace ff {

class ffrpc_t: public msg_handler_i
{
    struct session_data_t;
    struct slave_broker_info_t;
    struct broker_client_info_t;
public:
    ffrpc_t(const string& service_name_, uint16_t service_id_ = 1);
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
private:
    //! 处理连接断开
    int handle_broken_impl(socket_ptr_t sock_);
    //! 处理消息
    int handle_msg_impl(const message_t& msg_, socket_ptr_t sock_);
    template<typename T>
    int send_msg(socket_ptr_t sock_, uint16_t cmd_, T& msg_) { return 0; }
    //!  register all interface
    int register_all_interface(socket_ptr_t sock);
    int handle_broker_sync_data(broker_sync_all_registered_data_t::out_t& msg_, socket_ptr_t sock_);
    int handle_broker_route_msg(broker_route_t::in_t& msg_, socket_ptr_t sock_);
private:
    string                                  m_service_name;
    uint16_t                                m_service_id;
    uint32_t                                m_bind_broker_id;
    task_queue_t                            m_tq;
    thread_t                                m_thread;
    ffslot_t                                m_ffslot;
    ffslot_t                                m_ffslot_interface;//! 
    socket_ptr_t                            m_master_broker_sock;
    map<string, ffslot_t::callback_t*>      m_reg_iterface;
    map<uint32_t, slave_broker_info_t>      m_slave_broker_sockets;
    map<string, uint32_t>                   m_msg2id;
    map<uint32_t, broker_client_info_t>     m_broker_client_info;//! node id -> service
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
        port(0),
        sock(NULL)
    {}
    string          host;
    int32_t         port;
    socket_ptr_t    sock;
};
struct ffrpc_t::broker_client_info_t
{
    broker_client_info_t():
        bind_broker_id(0),
        sock(NULL)
    {}
    uint32_t bind_broker_id;
    string   service_name;
    uint16_t service_id;
    socket_ptr_t sock;
};

}
#endif
