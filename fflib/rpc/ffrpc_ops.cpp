
#include "rpc/ffbroker.h"
#include "rpc/ffrpc.h"

using namespace ff;

int ffrpc_memory_route_t::add_node(uint32_t node_id_, ffrpc_t* ffrpc_)
{
    m_node_info[node_id_].ffrpc = ffrpc_;
    return 0;
}

int ffrpc_memory_route_t::add_node(uint32_t node_id_, ffbroker_t* ffbroker_)
{
    m_node_info[node_id_].ffbroker = ffbroker_;
    return 0;
}
int ffrpc_memory_route_t::del_node(uint32_t node_id_)
{
    m_node_info.erase(node_id_);
    return 0;
}

//! 判断目标节点是否在同一进程中
bool ffrpc_memory_route_t::is_same_process(uint32_t node_id_)
{
    if (m_node_info.find(node_id_) != m_node_info.end())
    {
        return true;
    }
    return false;
}

//! broker 转发消息到rpc client
int ffrpc_memory_route_t::broker_route_to_client(broker_route_t::in_t& msg_)
{
    ffrpc_t* ffrpc = m_node_info[msg_.dest_node_id].ffrpc;
    if (NULL == ffrpc)
    {
        return -1;
    }
    ffrpc->get_tq().produce(task_binder_t::gen(&ffrpc_t::trigger_callback, ffrpc, msg_));
    return 0;
}

//! client 转发消息到 broker, 再由broker转发到client
int ffrpc_memory_route_t::client_route_to_broker(broker_route_t::in_t& msg_)
{
    ffbroker_t* ffbroker = m_node_info[msg_.dest_node_id].ffbroker;
    if (NULL == ffbroker)
    {
        return -1;
    }
    ffbroker->get_tq().produce(task_binder_t::gen(&ffbroker_t::route_msg_to_broker_client, ffbroker, msg_));
    return 0;
}
//! 所有已经注册的本进程的节点
vector<uint32_t> ffrpc_memory_route_t::get_node_same_process()
{
    vector<uint32_t> ret;
    map<uint32_t/*node id*/, dest_node_info_t>::iterator it = m_node_info.begin();
    for (; it != m_node_info.end(); ++it)
    {
        ret.push_back(it->first);
    }
    return ret;
}
