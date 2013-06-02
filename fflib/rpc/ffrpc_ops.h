
#ifndef _FF_RPC_OPS_H_
#define _FF_RPC_OPS_H_

#include <assert.h>
#include <string>
using namespace std;

#include "base/ffslot.h"
#include "net/socket_i.h"
#include "base/fftype.h"

namespace ff
{

class ffslot_msg_arg: public ffslot_t::callback_arg_t
{
public:
    ffslot_msg_arg(const string& s_, socket_ptr_t sock_):
        body(s_),
        sock(sock_)
    {}
    virtual int type()
    {
        return TYPEID(ffslot_msg_arg);
    }
    string       body;
    socket_ptr_t sock;
};

class ffslot_req_arg: public ffslot_t::callback_arg_t
{
public:
    ffslot_req_arg(const string& s_):
        body(s_)
    {}
    virtual int type()
    {
        return TYPEID(ffslot_req_arg);
    }
    string       body;
};

struct ffrpc_ops_t
{
    template <typename R, typename T>
    static ffslot_t::callback_t* gen_callback(R (*)(T&, socket_ptr_t));
    template <typename R, typename CLASS_TYPE, typename T>
    static ffslot_t::callback_t* gen_callback(R (CLASS_TYPE::*)(T&, socket_ptr_t), CLASS_TYPE* obj_);
};

template <typename R, typename T>
ffslot_t::callback_t* ffrpc_ops_t::gen_callback(R (*func_)(T&, socket_ptr_t))
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        typedef R (*func_t)(T&, socket_ptr_t);
        lambda_cb(func_t func_):m_func(func_){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(ffslot_msg_arg))
            {
                return;
            }
            ffslot_msg_arg* msg_data = (ffslot_msg_arg*)args_;
            T msg;
            msg.decode(msg_data->body);
            m_func(msg, msg_data->sock);
        }
        func_t m_func;
    };
    return new lambda_cb(func_);
}
template <typename R, typename CLASS_TYPE, typename T>
ffslot_t::callback_t* ffrpc_ops_t::gen_callback(R (CLASS_TYPE::*func_)(T&, socket_ptr_t), CLASS_TYPE* obj_)
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        typedef R (CLASS_TYPE::*func_t)(T&, socket_ptr_t);
        lambda_cb(func_t func_, CLASS_TYPE* obj_):m_func(func_), m_obj(obj_){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(ffslot_msg_arg))
            {
                return;
            }
            ffslot_msg_arg* msg_data = (ffslot_msg_arg*)args_;
            T msg;
            msg.decode(msg_data->body);
            (m_obj->*(m_func))(msg, msg_data->sock);
        }
        func_t      m_func;
        CLASS_TYPE* m_obj;
    };
    return new lambda_cb(func_, obj_);
}


class null_type_t {};
template<typename T, typename R = null_type_t>
struct ffreq_t
{
};


enum ffrpc_cmd_def_e
{
    BROKER_SLAVE_REGISTER  = 1,
    BROKER_CLIENT_REGISTER,
    BROKER_ROUTE_MSG,
    BROKER_SYNC_DATA_MSG,
    BROKER_TO_CLIENT_MSG,
};

}

#endif
