#include "rpc/ffbroker.h"
using namespace ff;

ffbroker_t::ffbroker_t()
{

}
ffbroker_t::~ffbroker_t()
{

}
int ffbroker_t::handle_broken(socket_ptr_t sock_)
{
    return 0;
}
int ffbroker_t::handle_msg(const message_t& msg_, socket_ptr_t sock_)
{
    return 0;
}
