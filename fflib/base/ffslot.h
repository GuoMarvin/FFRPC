
#ifndef _FF_SLOT_H_
#define _FF_SLOT_H_

#include <assert.h>
#include <string>
#include <map>
using namespace std;

namespace ff
{

class ffslot_t //! 回调函数工具类
{
public:
    class callback_arg_t;
    class callback_t;
public:
    virtual ~ffslot_t();
    ffslot_t& bind(int cmd_, callback_t* callback_);
    ffslot_t& bind(const string& cmd_, callback_t* callback_);
    callback_t* get_callback(int cmd_);
    callback_t* get_callback(const string& cmd_);
private:
    map<int, callback_t*>       m_cmd2callback;
    map<string, callback_t*>    m_name2callback;
};

class ffslot_t::callback_arg_t
{
public:
    virtual ~callback_arg_t(){}
    virtual int type() = 0;
};

class ffslot_t::callback_t
{
public:
    virtual ~callback_t(){}
    virtual void exe(ffslot_t::callback_arg_t* args_) = 0;
    virtual callback_t* fork() { return NULL; }
};

ffslot_t::~ffslot_t()
{
    map<int, callback_t*>::iterator it = m_cmd2callback.begin();
    for (; it != m_cmd2callback.end(); ++it)
    {
        delete it->second;
    }
    map<string, callback_t*>::iterator it2 = m_name2callback.begin();
    for (; it2 != m_name2callback.end(); ++it2)
    {
        delete it2->second;
    }
    m_cmd2callback.clear();
    m_name2callback.clear();
}
ffslot_t& ffslot_t::bind(int cmd_, ffslot_t::callback_t* callback_)
{
    assert(callback_);
    m_cmd2callback.insert(make_pair(cmd_, callback_));
    return *this;
}
ffslot_t& ffslot_t::bind(const string& cmd_, ffslot_t::callback_t* callback_)
{
    assert(callback_);
    m_name2callback.insert(make_pair(cmd_, callback_));
    return *this;
}
ffslot_t::callback_t* ffslot_t::get_callback(int cmd_)
{
    map<int, callback_t*>::iterator it = m_cmd2callback.find(cmd_);
    if (it != m_cmd2callback.end())
    {
        return it->second;
    }
    return NULL;
}
ffslot_t::callback_t* ffslot_t::get_callback(const string& cmd_)
{
    map<string, callback_t*>::iterator it = m_name2callback.find(cmd_);
    if (it != m_name2callback.end())
    {
        return it->second;
    }
    return NULL;
}

}
#endif

