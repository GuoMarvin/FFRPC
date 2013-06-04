
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

    //! 如果绑定回调函数的时候，有时需要一些临时参数被保存直到回调函数被调用
    template <typename R, typename CLASS_TYPE, typename IN, typename OUT, typename FUNC_ARG1, typename ARG1>
    static ffslot_t::callback_t* gen_callback(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&, FUNC_ARG1), CLASS_TYPE* obj_, ARG1 arg1_);
    template <typename R, typename CLASS_TYPE, typename IN, typename OUT, typename FUNC_ARG1, typename ARG1,
              typename FUNC_ARG2, typename ARG2>
    static ffslot_t::callback_t* gen_callback(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&, FUNC_ARG1, FUNC_ARG2),
                                                CLASS_TYPE* obj_, ARG1 arg1_, ARG2 arg2_);
    template <typename R, typename CLASS_TYPE, typename IN, typename OUT, typename FUNC_ARG1, typename ARG1,
              typename FUNC_ARG2, typename ARG2, typename FUNC_ARG3, typename ARG3>
    static ffslot_t::callback_t* gen_callback(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&, FUNC_ARG1, FUNC_ARG2, FUNC_ARG3),
                                                CLASS_TYPE* obj_, ARG1 arg1_, ARG2 arg2_, ARG3 arg3_);
    template <typename R, typename CLASS_TYPE, typename IN, typename OUT, typename FUNC_ARG1, typename ARG1,
              typename FUNC_ARG2, typename ARG2, typename FUNC_ARG3, typename ARG3, typename FUNC_ARG4, typename ARG4>
    static ffslot_t::callback_t* gen_callback(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&, FUNC_ARG1, FUNC_ARG2, FUNC_ARG3, FUNC_ARG4),
                                                CLASS_TYPE* obj_, ARG1 arg1_, ARG2 arg2_, ARG3 arg3_, ARG4 arg4_);
    
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

//! 如果绑定回调函数的时候，有时需要一些临时参数被保存直到回调函数被调用
template <typename R, typename CLASS_TYPE, typename IN, typename OUT, typename FUNC_ARG1, typename ARG1>
ffslot_t::callback_t* ffrpc_ops_t::gen_callback(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&, FUNC_ARG1), CLASS_TYPE* obj_, ARG1 arg1_)
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        typedef R (CLASS_TYPE::*func_t)(ffreq_t<IN, OUT>&, FUNC_ARG1);
        lambda_cb(func_t func_, CLASS_TYPE* obj_, const ARG1& arg1_):
            m_func(func_), m_obj(obj_), m_arg1(arg1_){}
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
            (m_obj->*(m_func))(req, m_arg1);
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_func, m_obj, m_arg1); }
        func_t      m_func;
        CLASS_TYPE* m_obj;
        typename fftraits_t<ARG1>::value_t m_arg1;
    };
    return new lambda_cb(func_, obj_, arg1_);
}

//! 如果绑定回调函数的时候，有时需要一些临时参数被保存直到回调函数被调用
template <typename R, typename CLASS_TYPE, typename IN, typename OUT, typename FUNC_ARG1, typename ARG1, typename FUNC_ARG2, typename ARG2>
ffslot_t::callback_t* ffrpc_ops_t::gen_callback(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&, FUNC_ARG1, FUNC_ARG2),
                                                CLASS_TYPE* obj_, ARG1 arg1_, ARG2 arg2_)
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        typedef R (CLASS_TYPE::*func_t)(ffreq_t<IN, OUT>&, FUNC_ARG1, FUNC_ARG2);
        lambda_cb(func_t func_, CLASS_TYPE* obj_, const ARG1& arg1_, const ARG2& arg2_):
            m_func(func_), m_obj(obj_), m_arg1(arg1_), m_arg2(arg2_)
        {}
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
            (m_obj->*(m_func))(req, m_arg1, m_arg2);
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_func, m_obj, m_arg1, m_arg2); }
        func_t      m_func;
        CLASS_TYPE* m_obj;
        typename fftraits_t<ARG1>::value_t m_arg1;
        typename fftraits_t<ARG2>::value_t m_arg2;
    };
    return new lambda_cb(func_, obj_, arg1_, arg2_);
}

//! 如果绑定回调函数的时候，有时需要一些临时参数被保存直到回调函数被调用
template <typename R, typename CLASS_TYPE, typename IN, typename OUT, typename FUNC_ARG1, typename ARG1, typename FUNC_ARG2, typename ARG2,
          typename FUNC_ARG3, typename ARG3>
ffslot_t::callback_t* ffrpc_ops_t::gen_callback(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&, FUNC_ARG1, FUNC_ARG2, FUNC_ARG3),
                                                CLASS_TYPE* obj_, ARG1 arg1_, ARG2 arg2_, ARG3 arg3_)
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        typedef R (CLASS_TYPE::*func_t)(ffreq_t<IN, OUT>&, FUNC_ARG1, FUNC_ARG2, FUNC_ARG3);
        lambda_cb(func_t func_, CLASS_TYPE* obj_, const ARG1& arg1_, const ARG2& arg2_, const ARG3& arg3_):
            m_func(func_), m_obj(obj_), m_arg1(arg1_), m_arg2(arg2_), m_arg3(arg3_)
        {}
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
            (m_obj->*(m_func))(req, m_arg1, m_arg2, m_arg3);
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_func, m_obj, m_arg1, m_arg2, m_arg3); }
        func_t      m_func;
        CLASS_TYPE* m_obj;
        typename fftraits_t<ARG1>::value_t m_arg1;
        typename fftraits_t<ARG2>::value_t m_arg2;
        typename fftraits_t<ARG3>::value_t m_arg3;
    };
    return new lambda_cb(func_, obj_, arg1_, arg2_, arg3_);
}

//! 如果绑定回调函数的时候，有时需要一些临时参数被保存直到回调函数被调用
template <typename R, typename CLASS_TYPE, typename IN, typename OUT, typename FUNC_ARG1, typename ARG1, typename FUNC_ARG2, typename ARG2,
          typename FUNC_ARG3, typename ARG3, typename FUNC_ARG4, typename ARG4>
ffslot_t::callback_t* ffrpc_ops_t::gen_callback(R (CLASS_TYPE::*func_)(ffreq_t<IN, OUT>&, FUNC_ARG1, FUNC_ARG2, FUNC_ARG3, FUNC_ARG4),
                                                CLASS_TYPE* obj_, ARG1 arg1_, ARG2 arg2_, ARG3 arg3_, ARG4 arg4_)
{
    struct lambda_cb: public ffslot_t::callback_t
    {
        typedef R (CLASS_TYPE::*func_t)(ffreq_t<IN, OUT>&, FUNC_ARG1, FUNC_ARG2, FUNC_ARG3, FUNC_ARG4);
        lambda_cb(func_t func_, CLASS_TYPE* obj_, const ARG1& arg1_, const ARG2& arg2_, const ARG3& arg3_, const ARG4& arg4_):
            m_func(func_), m_obj(obj_), m_arg1(arg1_), m_arg2(arg2_), m_arg3(arg3_), m_arg4(arg4_)
        {}
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
            (m_obj->*(m_func))(req, m_arg1, m_arg2, m_arg3, m_arg4);
        }
        virtual ffslot_t::callback_t* fork() { return new lambda_cb(m_func, m_obj, m_arg1, m_arg2, m_arg3, m_arg4); }
        func_t      m_func;
        CLASS_TYPE* m_obj;
        typename fftraits_t<ARG1>::value_t m_arg1;
        typename fftraits_t<ARG2>::value_t m_arg2;
        typename fftraits_t<ARG3>::value_t m_arg3;
        typename fftraits_t<ARG4>::value_t m_arg4;
    };
    return new lambda_cb(func_, obj_, arg1_, arg2_, arg3_, arg4_);
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
