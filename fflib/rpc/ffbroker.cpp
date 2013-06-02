#include "rpc/ffbroker.h"
#include "rpc/ffrpc_ops.h"
#include "base/log.h"
using namespace ff;

ffbroker_t::ffbroker_t():
    m_node_id_index(0)
{
}
ffbroker_t::~ffbroker_t()
{

}

//! ����һ��nodeid
uint32_t ffbroker_t::alloc_id()
{
    return ++m_node_id_index;
}

int ffbroker_t::open(const string& opt_)
{
    //! ��cmd ��Ӧ�Ļص�����
    m_ffslot.bind(BROKER_SLAVE_REGISTER, ffrpc_ops_t::gen_callback(&ffbroker_t::handle_slave_register, this))
            .bind(BROKER_CLIENT_REGISTER, ffrpc_ops_t::gen_callback(&ffbroker_t::handle_client_register, this))
            .bind(BROKER_ROUTE_MSG, ffrpc_ops_t::gen_callback(&ffbroker_t::handle_route_msg, this));

    //! ������а��߳�
    m_thread.create_thread(task_binder_t::gen(&task_queue_t::run, &m_tq), 1);
    return 0;
}

int ffbroker_t::handle_broken(socket_ptr_t sock_)
{
    m_tq.produce(task_binder_t::gen(&ffbroker_t::handle_broken_impl, this, sock_));
    return 0;
}
int ffbroker_t::handle_msg(const message_t& msg_, socket_ptr_t sock_)
{
    m_tq.produce(task_binder_t::gen(&ffbroker_t::handle_msg_impl, this, msg_, sock_));
    return 0;
}
//! �������ӶϿ����򱻻ص�
int ffbroker_t::handle_broken_impl(socket_ptr_t sock_)
{
    if (NULL == sock_->get_data<session_data_t>())
    {
        sock_->safe_delete();
        return 0;
    }
    m_slave_broker_sockets.erase(sock_->get_data<session_data_t>()->get_node_id());
    m_broker_client_info.erase(sock_->get_data<session_data_t>()->get_node_id());
    delete sock_->get_data<session_data_t>();
    sock_->set_data(NULL);
    sock_->safe_delete();
    return 0;
}
//! ������Ϣ���������ص�
int ffbroker_t::handle_msg_impl(const message_t& msg_, socket_ptr_t sock_)
{
    uint16_t cmd = msg_.get_cmd();
    ffslot_t::callback_t* cb = m_ffslot.get_callback(cmd);
    if (cb)
    {
        try
        {
            ffslot_msg_arg arg(msg_.get_body(), sock_);
            cb->exe(&arg);
            return 0;
        }
        catch(exception& e_)
        {
            LOGERROR((BROKER, "ffbroker_t::handle_msg_impl exception<%s>", e_.what()));
            return -1;
        }
    }
    return -1;
}

//! ����borker slave ע����Ϣ
int ffbroker_t::handle_slave_register(register_slave_broker_t::in_t& msg_, socket_ptr_t sock_)
{
    LOGTRACE((BROKER, "ffbroker_t::handle_slave_register begin"));
    session_data_t* psession = new session_data_t(alloc_id());
    sock_->set_data(psession);
    slave_broker_info_t& slave_broker_info = m_slave_broker_sockets[psession->get_node_id()];
    slave_broker_info.host = msg_.host;
    slave_broker_info.port = msg_.port;
    slave_broker_info.sock = sock_;
    sync_all_register_info(sock_);
    LOGTRACE((BROKER, "ffbroker_t::handle_slave_register end ok"));
    return 0;
}

//! ����borker client ע����Ϣ
int ffbroker_t::handle_client_register(register_broker_client_t::in_t& msg_, socket_ptr_t sock_)
{
    LOGTRACE((BROKER, "ffbroker_t::handle_client_register begin"));

    for (map<uint32_t, broker_client_info_t>::iterator it = m_broker_client_info.begin();
         it != m_broker_client_info.end(); ++it)
    {
        broker_client_info_t& broker_client_info = it->second;
        if (broker_client_info.service_name == msg_.service_name &&
            broker_client_info.service_id   == msg_.service_id)
        {
            sock_->close();
            LOGTRACE((BROKER, "ffbroker_t::handle_client_register service<%s>,id<%u> has been registed",
                                msg_.service_name.c_str(), msg_.service_id));
            return -1;
        }
    }

    session_data_t* psession = new session_data_t(alloc_id());
    sock_->set_data(psession);

    broker_client_info_t& broker_client_info = m_broker_client_info[psession->get_node_id()];
    broker_client_info.bind_broker_id        = msg_.bind_broker_id;
    broker_client_info.service_name          = msg_.service_name;
    broker_client_info.service_id            = msg_.service_id;
    broker_client_info.sock                  = sock_;

    for (std::set<string>::iterator it = msg_.msg_names.begin(); it != msg_.msg_names.end(); ++it)
    {
        if (m_msg2id.find(*it) != m_msg2id.end())
        {
            continue;
        }
        m_msg2id[*it] = alloc_id();
    }

    sync_all_register_info(sock_);
    LOGTRACE((BROKER, "ffbroker_t::handle_client_register end ok"));
    return 0;
}

//! ͬ�����еĵ�ǰ��ע��ӿ���Ϣ
int ffbroker_t::sync_all_register_info(socket_ptr_t sock_)
{
    LOGTRACE((BROKER, "ffbroker_t::sync_all_register_info begin"));
    broker_sync_all_registered_data_t::out_t msg;
    msg.msg2id = m_msg2id;
    for (map<uint32_t, broker_client_info_t>::iterator it = m_broker_client_info.begin();
         it != m_broker_client_info.end(); ++it)
    {
        msg.broker_client_info[it->first].bind_broker_id      = it->second.bind_broker_id;
        msg.broker_client_info[it->first].service_name        = it->second.service_name;
        msg.broker_client_info[it->first].service_id          = it->second.service_id;
    }
    //! ��������ע���broker slave�ڵ㸳ֵ����Ϣ
    for (map<uint32_t, slave_broker_info_t>::iterator it = m_slave_broker_sockets.begin();
         it != m_slave_broker_sockets.end(); ++it)
    {
        msg.slave_broker_info[it->first].host = it->second.host;
        msg.slave_broker_info[it->first].port = it->second.port;
    }
    
    //! ��������ע���broker slave�ڵ��������е���Ϣ
    for (map<uint32_t, slave_broker_info_t>::iterator it = m_slave_broker_sockets.begin();
         it != m_slave_broker_sockets.end(); ++it)
    {
        if (sock_ == it->second.sock)
        {
            msg.node_id = sock_->get_data<session_data_t>()->get_node_id();
        }
        send_msg(it->second.sock, BROKER_SYNC_DATA_MSG, msg);
    }
    //! ��������ע���broker client�ڵ��������е���Ϣ
    for (map<uint32_t, broker_client_info_t>::iterator it = m_broker_client_info.begin();
         it == m_broker_client_info.end(); ++it)
    {
        if (sock_ == it->second.sock)
        {
            msg.node_id = sock_->get_data<session_data_t>()->get_node_id();
        }
        send_msg(it->second.sock, BROKER_SYNC_DATA_MSG, msg);
    }
    LOGTRACE((BROKER, "ffbroker_t::sync_all_register_info end ok"));
    return 0;
}

//! ת����Ϣ
int ffbroker_t::handle_route_msg(broker_route_t::in_t& msg_, socket_ptr_t sock_)
{
    LOGTRACE((BROKER, "ffbroker_t::handle_route_msg begin"));
    map<uint32_t, broker_client_info_t>::iterator it = m_broker_client_info.find(msg_.node_id);
    if (it == m_broker_client_info.end())
    {
        LOGERROR((BROKER, "ffbroker_t::handle_route_msg no this node id[%u]", msg_.node_id));
        return -1;
    }
    send_msg(it->second.sock, BROKER_TO_CLIENT_MSG, msg_);
    LOGTRACE((BROKER, "ffbroker_t::handle_route_msg end ok"));
    return 0;
}
