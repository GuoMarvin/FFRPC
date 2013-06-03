#include "rpc/ffrpc.h"
#include "rpc/ffrpc_ops.h"
#include "base/log.h"
#include "net/net_factory.h"

using namespace ff;

#define FFRPC                   "FFRPC"

ffrpc_t::ffrpc_t(const string& service_name_, uint16_t service_id_):
    m_service_name(service_name_),
    m_service_id(service_id_),
    m_node_id(0),
    m_bind_broker_id(0),
    m_callback_id(0),
    m_master_broker_sock(NULL)
{
    
}

ffrpc_t::~ffrpc_t()
{
    
}

int ffrpc_t::open(const string& opt_)
{
    arg_helper_t arg(opt_);
    net_factory_t::start(1);
    string host = arg.get_option_value("-l");

    m_master_broker_sock = net_factory_t::connect(host, this);
    if (NULL == m_master_broker_sock)
    {
        LOGERROR((FFRPC, "ffrpc_t::open failed, can't connect to remote broker<%s>", host.c_str()));
        return -1;
    }
    session_data_t* psession = new session_data_t(BROKER_MASTER_NODE_ID);
    m_master_broker_sock->set_data(psession);

    m_ffslot.bind(BROKER_SYNC_DATA_MSG, ffrpc_ops_t::gen_callback(&ffrpc_t::handle_broker_sync_data, this))
            .bind(BROKER_TO_CLIENT_MSG, ffrpc_ops_t::gen_callback(&ffrpc_t::handle_broker_route_msg, this));
    
    register_all_interface(m_master_broker_sock);
    m_thread.create_thread(task_binder_t::gen(&task_queue_t::run, &m_tq), 1);
    LOGTRACE((FFRPC, "ffrpc_t::open end ok"));
    return 0;
}

//!  register all interface
int ffrpc_t::register_all_interface(socket_ptr_t sock)
{
    register_broker_client_t::in_t msg;
    msg.service_name = m_service_name;
    msg.service_id   = m_service_id;
    msg.bind_broker_id = m_bind_broker_id;
    
    for (map<string, ffslot_t::callback_t*>::iterator it = m_reg_iterface.begin(); it != m_reg_iterface.end(); ++it)
    {
        msg.msg_names.insert(it->first);
    }
    msg_sender_t::send(sock, BROKER_CLIENT_REGISTER, msg);
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
    LOGTRACE((FFRPC, "ffrpc_t::handle_msg_impl cmd[%u] begin", cmd));

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
    LOGTRACE((FFRPC, "ffrpc_t::handle_broker_sync_data node_id[%u] begin", msg_.node_id));
    if (msg_.node_id != 0)
    {
        m_node_id = msg_.node_id;
    }
    
    m_msg2id = msg_.msg2id;
    map<string, uint32_t>::iterator it = msg_.msg2id.begin();
    for (; it != msg_.msg2id.end(); ++it)
    {
        map<string, ffslot_t::callback_t*>::iterator it2 = m_reg_iterface.find(it->first);
        if (it2 != m_reg_iterface.end() && NULL == m_ffslot_callback.get_callback(it->second))
        {
            m_ffslot_interface.bind(it->second, it2->second);
            LOGTRACE((FFRPC, "ffrpc_t::handle_broker_sync_data msg name[%s] -> msg id[%u]", it->first.c_str(), it->second));
        }
    }
    
    map<uint32_t, broker_sync_all_registered_data_t::slave_broker_info_t>::iterator it3 = msg_.slave_broker_info.begin();
    for (; it3 != msg_.slave_broker_info.end(); ++it3)
    {
        if (m_slave_broker_sockets.find(it3->first) == m_slave_broker_sockets.end())//! new broker
        {
            //connect to and register to
            m_slave_broker_sockets[it3->first].host = it3->second.host;
            m_slave_broker_sockets[it3->first].port = it3->second.port;
            m_slave_broker_sockets[it3->first].sock = NULL;//! TODO
        }
    }
    
    m_broker_client_info.clear();
    m_broker_client_name2nodeid.clear();
    map<uint32_t, broker_sync_all_registered_data_t::broker_client_info_t>::iterator it4 = msg_.broker_client_info.begin();
    for (; it4 != msg_.broker_client_info.end(); ++it4)
    {
        ffrpc_t::broker_client_info_t& broker_client_info = m_broker_client_info[it4->first];
        broker_client_info.bind_broker_id = it4->second.bind_broker_id;
        broker_client_info.service_name   = it4->second.service_name;
        broker_client_info.service_id     = it4->second.service_id;

        char name[512];
        GEN_SERVICE_NAME(name, broker_client_info.service_name.c_str(), broker_client_info.service_id);
        m_broker_client_name2nodeid[name] = it4->first;
        LOGTRACE((FFRPC, "ffrpc_t::handle_broker_sync_data name[%s] -> node id[%u]", name, it4->first));
    }
    LOGTRACE((FFRPC, "ffrpc_t::handle_broker_sync_data end ok"));
    return 0;
}

int ffrpc_t::handle_broker_route_msg(broker_route_t::in_t& msg_, socket_ptr_t sock_)
{
    LOGTRACE((FFRPC, "ffrpc_t::handle_broker_route_msg msg_id[%u] begin", msg_.msg_id));
    try
    {
        if (msg_.msg_id == 0)//! callback msg
        {
            ffslot_t::callback_t* cb = m_ffslot_interface.get_callback(msg_.callback_id);
            if (cb)
            {
                ffslot_req_arg arg(msg_.body, sock_->get_data<session_data_t>()->get_node_id());
                cb->exe(&arg);
                m_ffslot.del(msg_.callback_id);
                return 0;
            }
        }
        else//! call interface
        {
            ffslot_t::callback_t* cb = m_ffslot_interface.get_callback(msg_.msg_id);
            if (cb)
            {
                ffslot_req_arg arg(msg_.body, sock_->get_data<session_data_t>()->get_node_id());
                cb->exe(&arg);
                LOGTRACE((FFRPC, "ffrpc_t::handle_broker_route_msg end ok"));
                return 0;
            }
        }
    }
    catch(exception& e_)
    {
        LOGERROR((BROKER, "ffbroker_t::handle_broker_route_msg exception<%s>", e_.what()));
        return -1;
    }
    LOGTRACE((FFRPC, "ffrpc_t::handle_broker_route_msg no register func"));
    return 0;
}

int ffrpc_t::call_impl(const string& service_name_, const string& msg_name_, const string& body_, ffslot_t::callback_t* callback_)
{
    LOGTRACE((FFRPC, "ffrpc_t::call_impl begin service_name_<%s>, msg_name_<%s>", service_name_.c_str(), msg_name_.c_str()));
    map<string, uint32_t>::iterator it = m_broker_client_name2nodeid.find(service_name_);
    if (it == m_broker_client_name2nodeid.end())
    {
        return -1;
    }

    uint32_t dest_node_id = it->second;
    broker_client_info_t& broker_client_info = m_broker_client_info[dest_node_id];

    broker_route_t::in_t msg;
    msg.node_id     = dest_node_id;
    msg.msg_id      = m_msg2id[msg_name_];
    msg.body        = body_;
    LOGTRACE((FFRPC, "ffrpc_t::call_impl msgid<%u>", msg.msg_id));

    if (callback_)
    {
        msg.callback_id = get_callback_id();
        m_ffslot_callback.bind(msg.callback_id, callback_);
    }
    else
    {
        msg.callback_id = 0;
    }

    uint32_t broker_node_id = broker_client_info.bind_broker_id;
    if (broker_node_id == BROKER_MASTER_NODE_ID)
    {
        msg_sender_t::send(m_master_broker_sock, BROKER_ROUTE_MSG, msg);
    }
    else
    {
        map<uint32_t, slave_broker_info_t>::iterator it_slave_broker = m_slave_broker_sockets.find(broker_node_id);
        if (it_slave_broker != m_slave_broker_sockets.end())
        {
            msg_sender_t::send(it_slave_broker->second.sock, BROKER_ROUTE_MSG, msg);
        }
    }
    return 0;
}

