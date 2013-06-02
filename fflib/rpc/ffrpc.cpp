#include "rpc/ffrpc.h"
#include "rpc/ffrpc_ops.h"
#include "base/log.h"
#include "net/net_factory.h"

using namespace ff;

#define FFRPC                   "FFRPC"
#define BROKER_MASTER_NODE_ID   0

ffrpc_t::ffrpc_t(const string& service_name_, uint16_t service_id_):
    m_service_name(service_name_),
    m_service_id(service_id_),
    m_bind_broker_id(0)
{
    
}

ffrpc_t::~ffrpc_t()
{
    
}

int ffrpc_t::open(const string& host_)
{
    socket_ptr_t m_master_broker_sock = net_factory_t::connect(host_, this);
    if (NULL == m_master_broker_sock)
    {
        LOGERROR((FFRPC, "ffrpc_t::open failed, can't connect to remote broker<%s>", host_.c_str()));
        return -1;
    }
    session_data_t* psession = new session_data_t(BROKER_MASTER_NODE_ID);
    m_master_broker_sock->set_data(psession);

    m_ffslot.bind(BROKER_SYNC_DATA_MSG, ffrpc_ops_t::gen_callback(&ffrpc_t::handle_broker_sync_data, this))
            .bind(BROKER_TO_CLIENT_MSG, ffrpc_ops_t::gen_callback(&ffrpc_t::handle_broker_route_msg, this));
    
    register_all_interface(m_master_broker_sock);
    m_thread.create_thread(task_binder_t::gen(&task_queue_t::run, &m_tq), 1);
    return 0;
}

//!  register all interface
int ffrpc_t::register_all_interface(socket_ptr_t sock)
{
    register_broker_client_t::in_t msg;
    msg.service_name = m_service_name;
    msg.service_id   = m_service_id;
    msg.bind_broker_id = m_bind_broker_id;
    send_msg(sock, BROKER_CLIENT_REGISTER, m_bind_broker_id);
    return 0;
}

int ffrpc_t::handle_broken(socket_ptr_t sock_)
{
    m_tq.produce(task_binder_t::gen(&ffrpc_t::handle_broken_impl, this, sock_));
    return 0;
}
int ffrpc_t::handle_msg(const message_t& msg_, socket_ptr_t sock_)
{
    m_tq.produce(task_binder_t::gen(&ffrpc_t::handle_msg_impl, this, msg_, sock_));
    return 0;
}

int ffrpc_t::handle_broken_impl(socket_ptr_t sock_)
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

int ffrpc_t::handle_msg_impl(const message_t& msg_, socket_ptr_t sock_)
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

int ffrpc_t::handle_broker_sync_data(broker_sync_all_registered_data_t::out_t& msg_, socket_ptr_t sock_)
{
    return 0;
}

int ffrpc_t::handle_broker_route_msg(broker_route_t::in_t& msg_, socket_ptr_t sock_)
{
    try
    {
        if (msg_.msg_id == 0)//! callback msg
        {
            ffslot_t::callback_t* cb = m_ffslot.get_callback(msg_.msg_id);
            if (cb)
            {
                ffslot_msg_arg arg(msg_.body, sock_);
                cb->exe(&arg);
                return 0;
            }
        }
        else//! call interface
        {
            ffslot_t::callback_t* cb = m_ffslot.get_callback(msg_.callback_id);
            if (cb)
            {
                ffslot_msg_arg arg(msg_.body, sock_);
                cb->exe(&arg);
                return 0;
            }
        }
    }
    catch(exception& e_)
    {
        LOGERROR((BROKER, "ffbroker_t::handle_broker_route_msg exception<%s>", e_.what()));
        return -1;
    }
    return 0;
}
