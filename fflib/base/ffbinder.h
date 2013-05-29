
#ifndef _FF_BINDER_H_
#define _FF_BINDER_H_

#include <assert.h>
#include <string>
#include <map>
using namespace std;

#include "base/ffslot.h"

namespace ff
{

template<typename T>
struct func_binder_t;

template<typename R>
struct func_binder_t<R (*)(fflost_t::callback_arg_t*)>: public fflost_t::callback_t
{
    typedef R (*func_t)(fflost_t::callback_arg_t*);
    static fflost_t::callback_t* gen(func_t func_)
    {
        return new func_binder_t<func_t>(func_);
    }
    func_binder_t(func_t func_):m_func(func_){}
    virtual void exe(fflost_t::callback_arg_t* args_)
    {
        m_func(args_);
    }
    func_t m_func;
};

struct ffslot_tool_t
{
    template <typename T>
    static fflost_t::callback_t* gen(T func_)
    {
        return func_binder_t<T>::gen(func_);
    }
};
}

#endif
