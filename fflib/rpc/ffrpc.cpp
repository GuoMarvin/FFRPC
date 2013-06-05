#include "rpc/ffrpc.h"
#include "rpc/ffrpc_ops.h"
#include "base/log.h"
#include "net/net_factory.h"

using namespace ff;

#define FFRPC                   "FFRPC"

ffrpc_t::ffrpc_t(string service_name_):
    m_service_name(service_name_),
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
    m_host = arg.get_option_value("-broker");

    m_ffslot.bind(BROKER_SYNC_DATA_MSG, ffrpc_ops_t::gen_callback(&ffrpc_t::handle_broker_sync_data, this))
            .bind(BROKER_TO_CLIENT_MSG, ffrpc_ops_t::gen_callback(&ffrpc_t::handle_broker_route_msg, this));

    m_master_broker_sock = connect_to_broker(m_host, BROKER_MASTER_NODE_ID);

    if (NULL == m_master_broker_sock)
    {
        LOGERROR((FFRPC, "ffrpc_t::open failed, can't connect to remote broker<%s>", m_host.c_str()));
        return -1;
    }
    m_thread.create_thread(task_binder_t::gen(&task_queue_t::run, &m_tq), 1);

    while(m_node_id == 0)
    {
        usleep(1);
        if (m_master_broker_sock == NULL)
        {
            LOGERROR((FFRPC, "ffrpc_t::open failed"));
            return -1;
        }
    }
    singleton_t<ffrpc_memory_route_t>::instance().add_node(m_node_id, this);
    LOGTRACE((FFRPC, "ffrpc_t::open end ok m_node_id[%u]", m_node_id));
    return 0;
}

//! 连接到broker master
socket_ptr_t ffrpc_t::connect_to_broker(const string& host_, uint32_t node_id_)
{
    socket_ptr_t sock = net_factory_t::connect(host_, this);
    if (NULL == sock)
    {
        LOGERROR((FFRPC, "ffrpc_t::register_to_broker_master failed, can't connect to remote broker<%s>", host_.c_str()));
        return sock;
    }
    session_data_t* psession = new session_data_t(node_id_);
    sock->set_data(psession);

    //! 发送注册消息给master broker
    if (node_id_ == BROKER_MASTER_NODE_ID)
    {
        register_all_interface(sock);
    }
    //! 发送注册消息给master slave broker
    else
    {
        register_client_to_slave_broker_t::in_t msg;
        msg.node_id = m_node_id;
        msg_sender_t::send(sock, CLIENT_REGISTER_TO_SLAVE_BROKER, msg);
    }
    return sock;
}
//! 投递到ffrpc 特定的线程
static void route_call_reconnect(ffrpc_t* ffrpc_)
{
    ffrpc_->get_tq().produce(task_binder_t::gen(&ffrpc_t::timer_reconnect_broker, ffrpc_));
}
//! 定时重连 broker master
void ffrpc_t::timer_reconnect_broker()
{
    LOGERROR((FFRPC, "ffrpc_t::timer_reconnect_broker begin..."));
    m_master_broker_sock = connect_to_broker(m_host, BROKER_MASTER_NODE_ID);
    if (NULL == m_master_broker_sock)
    {
        LOGERROR((FFRPC, "ffrpc_t::timer_reconnect_broker failed, can't connect to remote broker<%s>", m_host.c_str()));
        return;
    }
    //! 设置定时器重连
    m_timer.once_timer(RECONNECT_TO_BROKER_TIMEOUT, task_binder_t::gen(&route_call_reconnect, this));
    LOGERROR((FFRPC, "ffrpc_t::timer_reconnect_broker  end ok"));
}

//!  register all interface
int ffrpc_t::register_all_interface(socket_ptr_t sock)
{
    register_broker_client_t::in_t msg;
    msg.service_name = m_service_name;
    msg.bind_broker_id = m_bind_broker_id;
    
    for (map<string, ffslot_t::callback_t*>::iterator it = m_reg_iterface.begin(); it != m_reg_iterface.end(); ++it)
    {
        msg.msg_names.insert(it->first);
    }
    msg_sender_t::send(sock, BROKER_CLIENT_REGISTER, msg);
    return 0;
}
//! 获取任务队列对象
task_queue_t& ffrpc_t::get_tq()
{
    return m_tq;
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
    if (BROKER_MASTER_NODE_ID == sock_->get_data<session_data_t>()->get_node_id())
    {
        m_master_broker_sock = NULL;
        //! 连接到broker master的连接断开了
        map<uint32_t, slave_broker_info_t>::iterator it = m_slave_broker_sockets.begin();//! node id -> info
        for (; it != m_slave_broker_sockets.end(); ++it)
        {
            slave_broker_info_t& slave_broker_info = it->second;
            delete slave_broker_info.sock->get_data<session_data_t>();
            slave_broker_info.sock->set_data(NULL);
            slave_broker_info.sock->close();
        }
        m_slave_broker_sockets.clear();//! 所有连接到broker slave的连接断开
        m_ffslot_interface.clear();//! 注册的接口清除
        m_ffslot_callback.clear();//! 回调函数清除
        m_msg2id.clear();//! 消息映射表清除
        m_broker_client_info.clear();//! 各个服务的记录表清除
        m_broker_client_name2nodeid.clear();//! 服务名到node id的映射
        //! 设置定时器重练
        m_timer.once_timer(RECONNECT_TO_BROKER_TIMEOUT, task_binder_t::gen(&route_call_reconnect, this));
    }
    else
    {
        m_slave_broker_sockets.erase(sock_->get_data<session_data_t>()->get_node_id());
    }
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
            m_slave_broker_sockets[it3->first].sock = connect_to_broker(it3->second.host, it3->first);
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

        m_broker_client_name2nodeid[broker_client_info.service_name] = it4->first;
        LOGTRACE((FFRPC, "ffrpc_t::handle_broker_sync_data name[%s] -> node id[%u]", broker_client_info.service_name, it4->first));
    }
    LOGTRACE((FFRPC, "ffrpc_t::handle_broker_sync_data end ok"));
    return 0;
}

int ffrpc_t::handle_broker_route_msg(broker_route_t::in_t& msg_, socket_ptr_t sock_)
{
    return trigger_callback(msg_);
}

//! 调用消息对应的回调函数
int ffrpc_t::trigger_callback(broker_route_t::in_t& msg_)
{
    LOGTRACE((FFRPC, "ffrpc_t::handle_broker_route_msg msg_id[%u],callback_id[%u] begin", msg_.msg_id, msg_.callback_id));
    try
    {
        if (msg_.msg_id == 0)//! msg_id 为0表示这是一个回调的消息，callback_id已经有值
        {
            ffslot_t::callback_t* cb = m_ffslot_callback.get_callback(msg_.callback_id);
            if (cb)
            {
                ffslot_req_arg arg(msg_.body, msg_.from_node_id, msg_.callback_id, this);
                cb->exe(&arg);
                m_ffslot_callback.del(msg_.callback_id);
                return 0;
            }
            else
            {
                LOGERROR((FFRPC, "ffrpc_t::handle_broker_route_msg callback_id[%u] not found", msg_.callback_id));
            }
        }
        else//! 表示调用接口
        {
            ffslot_t::callback_t* cb = m_ffslot_interface.get_callback(msg_.msg_id);
            if (cb)
            {
                ffslot_req_arg arg(msg_.body, msg_.from_node_id, msg_.callback_id, this);
                cb->exe(&arg);
                LOGTRACE((FFRPC, "ffrpc_t::handle_broker_route_msg end ok"));
                return 0;
            }
            else
            {
                LOGERROR((FFRPC, "ffrpc_t::handle_broker_route_msg msg_id[%u] not found", msg_.msg_id));
            }
        }
    }
    catch(exception& e_)
    {
        LOGERROR((BROKER, "ffbroker_t::handle_broker_route_msg exception<%s>", e_.what()));
        if (msg_.msg_id == 0)//! callback msg
        {
            m_ffslot.del(msg_.callback_id);
        }
        return -1;
    }
    LOGTRACE((FFRPC, "ffrpc_t::handle_broker_route_msg end"));
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
    uint32_t msg_id      = m_msg2id[msg_name_];
    uint32_t callback_id = 0;

    if (callback_)
    {
        callback_id = get_callback_id();
        m_ffslot_callback.bind(callback_id, callback_);
    }

    send_to_broker_by_nodeid(dest_node_id, body_, msg_id, callback_id);
    
    LOGTRACE((FFRPC, "ffrpc_t::call_impl msgid<%u> end ok", msg_id));
    return 0;
}

//! 通过node id 发送消息给broker
void ffrpc_t::send_to_broker_by_nodeid(uint32_t dest_node_id, const string& body_, uint32_t msg_id_, uint32_t callback_id_)
{
    broker_client_info_t& broker_client_info = m_broker_client_info[dest_node_id];
    uint32_t broker_node_id = broker_client_info.bind_broker_id;

    broker_route_t::in_t msg;
    msg.dest_node_id     = dest_node_id;
    msg.from_node_id     = m_node_id;
    msg.msg_id      = msg_id_;
    msg.body        = body_;
    msg.callback_id = callback_id_;
    
    //!如果在同一个进程内那么，内存转发
    if (0 == singleton_t<ffrpc_memory_route_t>::instance().client_route_to_broker(msg))
    {
        LOGTRACE((FFRPC, "ffrpc_t::send_to_broker_by_nodeid dest_node_id[%u], broker_node_id[%u], msgid<%u>, callback_id_[%u] same process",
                        dest_node_id, broker_node_id, msg_id_, callback_id_));
        return;
    }

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
    LOGTRACE((FFRPC, "ffrpc_t::send_to_broker_by_nodeid dest_node_id[%u], broker_node_id[%u], msgid<%u>, callback_id_[%u] end ok",
                        dest_node_id, broker_node_id, msg_id_, callback_id_));
}

//! 调用接口后，需要回调消息给请求者
void ffrpc_t::response(uint32_t node_id_, uint32_t msg_id_, uint32_t callback_id_, const string& body_)
{
    m_tq.produce(task_binder_t::gen(&ffrpc_t::send_to_broker_by_nodeid, this, node_id_, body_, msg_id_, callback_id_));
}
