//! 二进制序列化 
#ifndef _CODEC_H_
#define _CODEC_H_

#include <string>
#include <stdexcept>
#include <vector>
#include <iostream>
#include <map>
#include <set>
using namespace std;

#include "net/message.h"
#include "base/singleton.h"
#include "base/atomic_op.h"
#include "base/lock.h"
#include "base/fftype.h"
#include "base/smart_ptr.h"

namespace ff {

#define GEN_CODE_DECODE(X) \
    bin_decoder_t& operator >> (X& dest_)       \
    {                                           \
        return copy_value(&dest_, sizeof(X));   \
    }


#define GEN_CODE_ENCODE(X)                                      \
    bin_encoder_t& operator << (const X& var_)                  \
    {                                                           \
        return copy_value((const char*)(&var_), sizeof(var_));  \
    }


struct codec_i
{
    virtual ~codec_i(){}
    virtual string encode_data()                      = 0;
    virtual void decode_data(const string& src_buff_) = 0;
};

class bin_encoder_t;
class bin_decoder_t;
struct codec_helper_i
{
    virtual ~codec_helper_i(){}
    virtual void encode(bin_encoder_t&) const = 0;
    virtual void decode(bin_decoder_t&)       = 0;
};


template<typename T>
struct option_t: public shared_ptr_t<T>
{
    option_t():
        data(NULL)
    {}
    option_t(const option_t& src_):
        data(NULL)
    {
        if (src_->data)
        {
            data = new T(src_->data);
        }
    }
    ~option_t()
    {
        release();
    }
    void release()
    {
        if (data)
        {
            delete data;
            data = NULL;
        }
    }
    void reset(T* p = NULL)
    {
        release();
        data = p;
    }
    T* get()        { return data; }
    T* operator->() { return data; }
    T* operator*()  { return data; }
    option_t& operator=(const option_t& src_)
    {
        release();
        if (src_->data)
        {
            data = new T(src_->data);
        }
        return *this;
    }
    T* data;
};

class bin_encoder_t
{
public:
    bin_encoder_t(){}
    bin_encoder_t& init()
    {
        return *this;
    }

    const string& get_buff() const { return m_dest_buff; }

    GEN_CODE_ENCODE(bool)
    GEN_CODE_ENCODE(int8_t)
    GEN_CODE_ENCODE(uint8_t)
    GEN_CODE_ENCODE(int16_t)
    GEN_CODE_ENCODE(uint16_t)
    GEN_CODE_ENCODE(int32_t)
    GEN_CODE_ENCODE(uint32_t)
    GEN_CODE_ENCODE(int64_t)
    GEN_CODE_ENCODE(uint64_t)
    
    bin_encoder_t& operator << (const string& str_)
    {
        return copy_value(str_);
    }
    
    template<typename T>
    bin_encoder_t& operator <<(const vector<T>& src_vt_)
    {
        uint32_t vt_size = (uint32_t)src_vt_.size();
        copy_value((const char*)(&vt_size), sizeof(vt_size));

        for (uint32_t i = 0; i < vt_size; ++i)
        {
            (*this) << src_vt_[i];
        }
        return *this;
    }
    template<typename T>
    bin_encoder_t& operator <<(const list<T>& src_vt_)
    {
        uint32_t vt_size = (uint32_t)src_vt_.size();
        copy_value((const char*)(&vt_size), sizeof(vt_size));
        typename list<T>::iterator it = src_vt_.begin();
        for (; it != src_vt_.end(); ++it)
        {
            (*this) << *it;
        }
        return *this;
    }
    template<typename T>
    bin_encoder_t& operator <<(const set<T>& src_vt_)
    {
        uint32_t vt_size = (uint32_t)src_vt_.size();
        copy_value((const char*)(&vt_size), sizeof(vt_size));
        typename set<T>::iterator it = src_vt_.begin();
        for (; it != src_vt_.end(); ++it)
        {
            (*this) << *it;
        }
        return *this;
    }
    template<typename T, typename R>
    bin_encoder_t& operator <<(const map<T, R>& src_)
    {
        uint32_t size = (uint32_t)src_.size();
        copy_value((const char*)(&size), sizeof(size));
        
        typename map<T, R>::const_iterator it = src_.begin();
        for (; it != src_.end(); ++it)
        {
            (*this) << it->first << it->second;
        }
        return *this;
    }


    bin_encoder_t& operator <<(codec_i& dest_)
    {
        string data = dest_.encode_data();
        *this << data;
        return *this;
    }
    template<typename T>
    bin_encoder_t& operator <<(option_t<T>& dest_)
    {
        uint8_t flag = 0;
        if (dest_.get())
        {
            flag = 1;
            *this << flag;
            *this << *(dest_);
        }
        else
        {
            *this << flag;
        }
        return *this;
    }
    void clear()
    {
        m_dest_buff.clear();
    }
private:
    inline bin_encoder_t& copy_value(const string& str_)
    {
        uint32_t str_size = str_.size();
        copy_value((const char*)(&str_size), sizeof(str_size));
        copy_value(str_.data(), str_.size());
        return *this;
    }
    inline bin_encoder_t& copy_value(const void* src_, size_t size_)
    {
        m_dest_buff.append((const char*)(src_), size_);
        return *this;
    }

private:
    string         m_dest_buff;
};

class bin_decoder_t
{
public:
    bin_decoder_t(){}
    explicit bin_decoder_t(const string& src_buff_):
        m_index_ptr(src_buff_.data()),
        m_remaindered(src_buff_.size())
    {
    }
    bin_decoder_t& init(const string& str_buff_)
    {
        m_index_ptr = str_buff_.data();
        m_remaindered = str_buff_.size();
        return *this;
    }

    GEN_CODE_DECODE(bool)
    GEN_CODE_DECODE(int8_t)
    GEN_CODE_DECODE(uint8_t)
    GEN_CODE_DECODE(int16_t)
    GEN_CODE_DECODE(uint16_t)
    GEN_CODE_DECODE(int32_t)
    GEN_CODE_DECODE(uint32_t)
    GEN_CODE_DECODE(int64_t)
    GEN_CODE_DECODE(uint64_t)

    bin_decoder_t& operator >> (string& dest_)
    {
        return copy_value(dest_);
    }

    template<typename T>
    bin_decoder_t& operator >>(vector<T>& dest_vt_)
    {
        uint32_t vt_size = 0;
        copy_value(&vt_size, sizeof(vt_size));

        for (size_t i = 0; i < vt_size; ++i)
        {
            T tmp;
            (*this) >> tmp;
            dest_vt_.push_back(tmp);
        }
        return *this;
    }
    template<typename T>
    bin_decoder_t& operator >>(list<T>& dest_vt_)
    {
        uint32_t vt_size = 0;
        copy_value(&vt_size, sizeof(vt_size));

        for (size_t i = 0; i < vt_size; ++i)
        {
            T tmp;
            (*this) >> tmp;
            dest_vt_.push_back(tmp);
        }
        return *this;
    }
    template<typename T>
    bin_decoder_t& operator >>(set<T>& dest_vt_)
    {
        uint32_t vt_size = 0;
        copy_value(&vt_size, sizeof(vt_size));

        for (size_t i = 0; i < vt_size; ++i)
        {
            T tmp;
            (*this) >> tmp;
            dest_vt_.insert(tmp);
        }
        return *this;
    }
    
    template<typename T, typename R>
    bin_decoder_t& operator >>(map<T, R>& dest_)
    {
        uint32_t size = 0;
        copy_value(&size, sizeof(size));
        
        for (size_t i = 0; i < size; ++i)
        {
            T key;
            R value;
            (*this) >> key >> value;
            dest_[key] = value;
        }
        return *this;
    }
    
    bin_decoder_t& operator >>(codec_i& dest_)
    {
        string data;
        *this >> data;
        dest_.decode_data(data);
        return *this;
    }
    template<typename T>
    bin_decoder_t& operator >>(option_t<T>& dest_)
    {
        uint8_t flag = 0;
        try{
            *this >> flag;
        }
        catch(exception& e_)
        {
            return *this;
        }
        if (flag)
        {
            dest_.reset(new T());
            *this << *dest_;   
        }
        return *this;
    }
private:
    bin_decoder_t& copy_value(void* dest_, uint32_t var_size_)
    {
        if (m_remaindered < var_size_)
        {
            throw runtime_error("bin_decoder_t:msg size not enough");
        }
        ::memcpy(dest_, m_index_ptr, var_size_);
        m_index_ptr     += var_size_;
        m_remaindered  -= var_size_;
        return *this;
    }
    
    bin_decoder_t& copy_value(string& dest_)
    {
        uint32_t str_len = 0;
        copy_value(&str_len, sizeof(str_len));

        if (m_remaindered < str_len)
        {
            throw runtime_error("bin_decoder_t:msg size not enough");
        }
        dest_.assign((const char*)m_index_ptr, str_len);
        m_index_ptr     += str_len;
        m_remaindered  -= str_len;
        return *this;
    }
    
private:
    const char*  m_index_ptr;
    size_t       m_remaindered;
};


class msg_i : public codec_i
{
public:
    virtual ~msg_i(){}
    msg_i(const char* msg_name_):
        m_msg_name(msg_name_)
    {}
    const char* get_type_name()  const
    {
        return m_msg_name;
    }
    bin_encoder_t& encoder()
    {
        return m_encoder;
    }
    bin_decoder_t& decoder()
    {
        return m_decoder;
    }
    virtual string encode_data()
    {
        m_encoder.clear();
        encode();
        return m_encoder.get_buff();
    }
    virtual void decode_data(const string& src_buff_)
    {
        m_decoder.init(src_buff_);
        decoder();
    }
    virtual void encode()                      = 0;
    virtual void decode()                      = 0;
    const char*   m_msg_name;
    bin_encoder_t m_encoder;
    bin_decoder_t m_decoder;
};


template<typename T>
class ffmsg_t: public msg_i
{
public:
    ffmsg_t():
        msg_i(TYPE_NAME(T).c_str())
    {}
    virtual ~ffmsg_t(){}
};

//! 向broker master 注册slave
struct register_slave_broker_t
{
    struct in_t: public ffmsg_t<in_t>
    {
        void encode()
        {
            encoder() << host;
        }
        void decode()
        {
            decoder()>> host;
        }
        string          host;
    };
};

//! 向broker master 注册client
struct register_broker_client_t
{
    struct in_t: public ffmsg_t<in_t>
    {
        void encode()
        {
            encoder() << service_name << bind_broker_id << msg_names;
        }
        void decode()
        {
            decoder() >> service_name >> bind_broker_id >> msg_names;
        }
        string                      service_name;
        uint32_t                    bind_broker_id;//! 是否需要绑定到特定的broker上
        std::set<string>            msg_names;
    };
};
//! 向broker slave 注册client
struct register_client_to_slave_broker_t
{
    struct in_t: public ffmsg_t<in_t>
    {
        void encode()
        {
            encoder() << node_id;
        }
        void decode()
        {
            decoder() >> node_id;
        }
        uint32_t                    node_id;//! master分配client的node id
    };
};

struct broker_sync_all_registered_data_t
{
    //! 记录每个broker slave 的接口信息
    struct slave_broker_info_t: public ffmsg_t<slave_broker_info_t>
    {
        void encode()
        {
            encoder() << host;
        }
        void decode()
        {
            decoder() >> host;
        }
        string          host;
    };

    struct broker_client_info_t: public ffmsg_t<broker_client_info_t>
    {
        void encode()
        {
            encoder() << bind_broker_id << service_name;
        }
        void decode()
        {
            decoder() >> bind_broker_id >> service_name;
        }
        //! 被绑定的节点broker node id
        uint32_t bind_broker_id;
        string   service_name;
    };
    struct out_t: public ffmsg_t<out_t>
    {
        out_t():
            node_id(0)
        {}
        void encode()
        {
            encoder() << node_id << msg2id ;//TODO<< slave_broker_info << broker_client_info;
        }
        void decode()
        {
            decoder() >> node_id >> msg2id ;//!TODO >> slave_broker_info >> broker_client_info;
        }
        uint32_t                                node_id;//! 被分配的node id
        map<string, uint32_t>                   msg2id; //! 消息名称对应的消息id 值
        //!记录所有的broker slave 信息
        map<uint32_t, slave_broker_info_t>      slave_broker_info;//! node id -> broker slave
        //! 记录所有服务/接口信息
        map<uint32_t, broker_client_info_t>     broker_client_info;//! node id -> service
    };
};

struct broker_route_t//!broker 转发消息
{
    struct in_t: public ffmsg_t<in_t>
    {
        void encode()
        {
            encoder() << node_id << msg_id << callback_id << body;
        }
        void decode()
        {
            decoder() >> node_id >> msg_id >> callback_id >> body;
        }
        uint32_t                    node_id;//! 需要转发到哪个节点上
        uint32_t                    msg_id;//! 调用的是哪个接口
        uint32_t                    callback_id;
        string                      body;
    };
};

}



#endif
