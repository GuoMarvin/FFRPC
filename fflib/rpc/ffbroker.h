
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
#include "net/codec.h"
#include "base/thread.h"
namespace ff
{
class ffbroker_t: public msg_handler_i
{
    //! ÿ�����Ӷ�Ҫ����һ��session�����ڼ�¼��socket����Ӧ����Ϣ
    struct session_data_t;
    //! ��¼ÿ��broker slave �Ľӿ���Ϣ
    struct slave_broker_info_t;
    //! ��¼ÿ��broker client �Ľӿ���Ϣ
    struct broker_client_info_t;
public:
    ffbroker_t();
    virtual ~ffbroker_t();

    //! �������ӶϿ����򱻻ص�
    int handle_broken(socket_ptr_t sock_);
    //! ������Ϣ���������ص�
    int handle_msg(const message_t& msg_, socket_ptr_t sock_);

    int open(const string& opt_);
    //! ����һ��nodeid
    uint32_t alloc_id();
private:
    //! �������ӶϿ����򱻻ص�
    int handle_broken_impl(socket_ptr_t sock_);
    //! ������Ϣ���������ص�
    int handle_msg_impl(const message_t& msg_, socket_ptr_t sock_);
    //! ͬ�����еĵ�ǰ��ע��ӿ���Ϣ
    int sync_all_register_info(socket_ptr_t sock_);
    //! ����borker slave ע����Ϣ
    int handle_slave_register(register_slave_broker_t::in_t& msg_, socket_ptr_t sock_);
    //! ����borker client ע����Ϣ
    int handle_client_register(register_broker_client_t::in_t& msg_, socket_ptr_t sock_);
    //! ת����Ϣ
    int handle_route_msg(broker_route_t::in_t& msg_, socket_ptr_t sock_);
    //! ������� TODO
    template<typename T>
    int send_msg(socket_ptr_t sock_, uint16_t cmd_, T& msg_) { return 0; }
private:
    //! ���ڷ���nodeid
    uint32_t                                m_node_id_index;
    //! ��¼����ע�ᵽ�˽ڵ��ϵ�����
    task_queue_t                            m_tq;
    thread_t                                m_thread;
    //! ���ڰ󶨻ص�����
    ffslot_t                                m_ffslot;
    //! ��¼���е�broker socket��Ӧnode id
    map<uint32_t, slave_broker_info_t>      m_slave_broker_sockets;
    //! ��¼���е���Ϣ��ƶ�Ӧ����Ϣidֵ
    map<string, uint32_t>                   m_msg2id;
    //! ��¼���з���/�ӿ���Ϣ
    map<uint32_t, broker_client_info_t>     m_broker_client_info;//! node id -> service
};

//! ÿ�����Ӷ�Ҫ����һ��session�����ڼ�¼��socket����Ӧ����Ϣ
struct ffbroker_t::session_data_t
{
    session_data_t(uint32_t n = 0):
        node_id(n)
    {}
    uint32_t get_node_id() { return node_id; }
    //! �������Ψһ�Ľڵ�id
    uint32_t node_id;
};
//! ��¼ÿ��broker client �Ľӿ���Ϣ
struct ffbroker_t::broker_client_info_t
{
    broker_client_info_t():
        bind_broker_id(0),
        sock(NULL)
    {}
    //! ���󶨵Ľڵ�broker node id
    uint32_t bind_broker_id;
    string   service_name;
    uint16_t service_id;
    socket_ptr_t sock;
};
//! ��¼ÿ��broker slave �Ľӿ���Ϣ
struct ffbroker_t::slave_broker_info_t
{
    slave_broker_info_t():
        port(0),
        sock(NULL)
    {}
    string          host;
    int32_t         port;
    socket_ptr_t    sock;
};

}

#endif
