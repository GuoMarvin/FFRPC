
#ifndef _FF_BROKER_H_
#define _FF_BROKER_H_

#include <assert.h>
#include <string>
#include <map>
using namespace std;

#include "net/msg_handler_i.h"
//#include "net/net_factory.h"

namespace ff
{

class ffbroker_t: public msg_handler_i
{
public:
    ffbroker_t();
    virtual ~ffbroker_t();

    int handle_broken(socket_ptr_t sock_);
    int handle_msg(const message_t& msg_, socket_ptr_t sock_);
};

}

#endif
