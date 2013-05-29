
#ifndef _FF_SLOT_H_
#define _FF_SLOT_H_

#include <assert.h>
#include <string>
#include <map>
using namespace std;

namespace ff
{

class fflost_t //! 回调函数工具类
{
public:
    class callback_arg_t;
    class callback_t;
public:
    virtual ~fflost_t();
    int bind(int cmd_, callback_t* callback_);
    int bind(const string& cmd_, callback_t* callback_);
    callback_t* get_callback(int cmd_);
    callback_t* get_callback(const string& cmd_);
private:
    map<int, callback_t*>       m_cmd2callback;
    map<string, callback_t*>    m_name2callback;
};

class fflost_t::callback_arg_t
{
public:
    virtual ~callback_arg_t(){}
    virtual int type() = 0;
};

class fflost_t::callback_t
{
public:
    virtual ~callback_t(){}
    virtual void exe(fflost_t::callback_arg_t* args_) = 0;
    virtual callback_t* fork() { return NULL; }
};

fflost_t::~fflost_t()
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
int fflost_t::bind(int cmd_, fflost_t::callback_t* callback_)
{
    assert(callback_);
    return m_cmd2callback.insert(make_pair(cmd_, callback_)).second == true? 0: -1;
}
int fflost_t::bind(const string& cmd_, fflost_t::callback_t* callback_)
{
    assert(callback_);
    return m_name2callback.insert(make_pair(cmd_, callback_)).second == true? 0: -1;
}
fflost_t::callback_t* fflost_t::get_callback(int cmd_)
{
    map<int, callback_t*>::iterator it = m_cmd2callback.find(cmd_);
    if (it != m_cmd2callback.end())
    {
        return it->second;
    }
    return NULL;
}
fflost_t::callback_t* fflost_t::get_callback(const string& cmd_)
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

