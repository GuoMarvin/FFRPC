
#ifndef _FF_RPC_OPS_H_
#define _FF_RPC_OPS_H_

#include <assert.h>
#include <string>
using namespace std;

#include "base/ffslot.h"
#include "net/socket_i.h"
#include "base/fftype.h"
#include "net/codec.h"

namespace ff
{

#define BROKER_MASTER_NODE_ID   0
#define GEN_SERVICE_NAME(M, X, Y) snprintf(M, sizeof(M), "%s@%u", X, Y)

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


class ffresponser_t
{
public:
    virtual ~ffresponser_t(){}
    virtual void response(uint32_t node_id_, uint32_t msg_id_, uint32_t callback_id_, const string& body_) = 0;
};

class ffslot_req_arg: public ffslot_t::callback_arg_t
{
public:
    ffslot_req_arg(const string& s_, uint32_t n_, uint32_t cb_id_, ffresponser_t* p):
        body(s_),
        node_id(n_),
        callback_id(cb_id_),
        responser(p)
    {}
    virtual int type()
    {
        return TYPEID(ffslot_req_arg);
    }
    string          body;
    uint32_t        node_id;
    uint32_t        callback_id;
    ffresponser_t*  responser;
};



class null_type_t: public ffmsg_t<null_type_t>
{
    virtual string encode()
    {
        return (init_encoder()).get_buff() ;
    }
    virtual void decode(const string& src_buff_)
    {
    }
};

template<typename IN, typename OUT = null_type_t>
struct ffreq_t
{
    ffreq_t():
        node_id(0),
        callback_id(0),
        responser(NULL)
    {}
    IN              arg;
    uint32_t        node_id;
    uint32_t        callback_id;
    ffresponser_t*  responser;
    void response(OUT& out_)
    {
        responser->response(node_id, 0, callback_id, out_.encode());
    }
};

struct ffrpc_ops_t
{
    template <typename R, typename T>
    static ffslot_t::callback_t* gen_callback(R (*)(T&, socket_ptr_t));
    template <typename R, typename CLASS_TYPE, typename T>
    static ffslot_t::callback_t* gen_callback(R (CLASS_TYPE::*)(T&, socket_ptr_t), CLASS_TYPE* obj_);
    
    //! 注册接口
    template <typename R, typename IN, typename OUT>
    static ffslot_t::callback_t* gen_callback(R (*)(ffreq_t<IN, OUT>&));
    template <typename R, typename CLASS_TYPE, typename IN, typename OUT>
    static ffslot_t::callback_t* gen_callback(R (CLASS_TYPE::*)(ffreq_t<IN, OUT>&), CLASS_TYPE* obj);
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
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_func); }
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
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_func, m_obj); }
        func_t      m_func;
        CLASS_TYPE* m_obj;
    };
    return new lambda_cb(func_, obj_);
}

//! 注册接口
template <typename R, typename IN, typename OUT>
ffslot_t::callback_t* ffrpc_ops_t::gen_callback(R (*func_)(ffreq_t<IN, OUT>&))
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        typedef R (*func_t)(ffreq_t<IN, OUT>&);
        lambda_cb(func_t func_):m_func(func_){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(ffslot_req_arg))
            {
                return;
            }
            ffslot_req_arg* msg_data = (ffslot_req_arg*)args_;
            ffreq_t<IN, OUT> req;
            req.arg.decode(msg_data->body);
            req.node_id = msg_data->node_id;
            req.callback_id = msg_data->callback_id;
            req.responser = msg_data->responser;
            m_func(req);
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_func); }
        func_t m_func;
    };
    return new lambda_cb(func_);
}
template <typename R, typename CLASS_TYPE, typename IN, typename OUT>
ffslot_t::callback_t* ffrpc_ops_t::gen_callback(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&), CLASS_TYPE* obj_)
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        typedef R (CLASS_TYPE::*func_t)(ffreq_t<IN, OUT>&);
        lambda_cb(func_t func_, CLASS_TYPE* obj_):m_func(func_), m_obj(obj_){}
        virtual void exe(ffslot_t::callback_arg_t* args_)
        {
            if (args_->type() != TYPEID(ffslot_req_arg))
            {
                return;
            }
            ffslot_req_arg* msg_data = (ffslot_req_arg*)args_;
            ffreq_t<IN, OUT> req;
            req.arg.decode(msg_data->body);
            req.callback_id = msg_data->callback_id;
            req.responser = msg_data->responser;
            (m_obj->*(m_func))(req);
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_func, m_obj); }
        func_t      m_func;
        CLASS_TYPE* m_obj;
    };
    return new lambda_cb(func_, obj_);
}


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
