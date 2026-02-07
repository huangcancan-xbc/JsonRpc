/*
    核心的消息结构实现
*/
#pragma once
#include "detail.hpp"
#include "fields.hpp"
#include "abstract.hpp"

namespace rpc
{
    typedef std::pair<std::string, int> Address;    // 主机地址：iP + port

    // 直接使用jsoncpp实现序列化和反序列化
    class JsonMessage : public BaseMessage
    {
    public:
        using ptr = std::shared_ptr<JsonMessage>;

        virtual std::string serialize() override
        {
            std::string body;
            bool ret = JSON::serialize(_body, body);
            if(ret==false)
            {
                return std::string();
            }

            return body;
        }

        virtual bool unserialize(const std::string &msg) override
        {
            return JSON::unserialize(msg, _body);
        }

    protected:
        Json::Value _body;  // 消息的内容（json格式）
    };



    class JsonRequest : public JsonMessage
    {   
    public:
        using ptr = std::shared_ptr<JsonRequest>;
    };



    class JsonResponse : public JsonMessage
    {
    public:
        using ptr = std::shared_ptr<JsonResponse>;

        virtual bool check() override
        {
            if(_body[KEY_RCODE].isNull() == true)
            {
                ELOG("响应中但是没有响应状态码！");
                return false;
            }

            if(_body[KEY_RCODE].isIntegral() == false)
            {
                ELOG("响应状态码类型错误！");
                return false;
            }

            return true;
        }

        virtual RCode rcode()
        {
            return (RCode)_body[KEY_RCODE].asInt();
        }

        virtual void setRCode(RCode rcode)
        {
            _body[KEY_RCODE] = (int)rcode;
        }
    };



    // rpc的请求，需要检查函数名和参数，包含4个方法：分别获取和设置函数名、参数
    class RpcRequest : public JsonRequest
    {
    public:
        using ptr = std::shared_ptr<RpcRequest>;

        virtual bool check() override
        {
            // Rpc的请求中包含：请求方法名称（字符串），参数字段（对象）
            if(_body[KEY_METHOD].isNull() == true || _body[KEY_METHOD].isString() == false)
            {
                ELOG("RPC请求中没有方法名称或者方法名称类型错误！");
                return false;
            }

            if(_body[KEY_PARAMS].isNull() == true || _body[KEY_PARAMS].isObject() == false)
            {
                ELOG("RPC请求中没有参数信息或者参数信息类型错误！");
                return false;
            }

            return true;
        }

        std::string method()
        {
            return _body[KEY_METHOD].asString();
        }

        void setMethod(const std::string &method_name)
        {
            _body[KEY_METHOD] = method_name;
        }

        Json::Value params()
        {
            return _body[KEY_PARAMS];
        }

        void setParams(const Json::Value &params)
        {
            _body[KEY_PARAMS] = params;
        }
    };



    // 主题的请求，需要检查主题的名称、操作、消息，包含6个操作：设置、获取主题的名字、操作、消息
    // 其中主题的操作包含创建、删除、订阅、取消订阅、发布5个操作
    class TopicRequest : public JsonRequest
    {
    public:
        using ptr = std::shared_ptr<TopicRequest>;

        virtual bool check() override
        {
            if(_body[KEY_TOPIC_KEY].isNull() == true || _body[KEY_TOPIC_KEY].isString() == false)
            {
                ELOG("主题请求中没有主题名称或者主题名称类型错误！");
                return false;
            }

            if(_body[KEY_OPTYPE].isNull() == true || _body[KEY_OPTYPE].isIntegral() == false)
            {
                ELOG("主题请求中没有操作类型或者操作类型类型错误！");
                return false;
            }

            if(_body[KEY_OPTYPE].asInt() == (int)TopicOptype::TOPIC_PUBLISH && 
                (_body[KEY_TOPIC_MSG].isNull() == true || _body[KEY_TOPIC_MSG].isString() == false))
            {
                ELOG("主题消息发布请求中没有消息内容字段或消息内容类型错误！");
                return false;
            }

            return true;
        }

        std::string topicKey()
        {
            return _body[KEY_TOPIC_KEY].asString();
        }

        void setTopicKey(const std::string &topic_key)
        {
            _body[KEY_TOPIC_KEY] = topic_key;
        }

        TopicOptype optype()
        {
            return (TopicOptype)_body[KEY_OPTYPE].asInt();
        }

        void setOptype(TopicOptype optype)
        {
            _body[KEY_OPTYPE] = (int)optype;
        }

        std::string topicMsg()
        {
            return _body[KEY_TOPIC_MSG].asString();
        }

        void setTopicMsg(const std::string &msg)
        {
            _body[KEY_TOPIC_MSG] = msg;
        }
    };



    // 向注册中心发送的请求，需要进行检查服务的函数名/方法名、操作类型、主机地址
    // 6个方法分别是获取和设置函数名、操作类型、主机，其中操作类型有：服务的发现、注册、上线、下线、未知
    class ServiceRequest : public JsonRequest
    {
    public:
        using ptr = std::shared_ptr<ServiceRequest>;

        virtual bool check() override
        {
            if(_body[KEY_METHOD].isNull() == true || _body[KEY_METHOD].isString() == false)
            {
                ELOG("服务请求中没有方法名称或者方法名称类型错误！");
                return false;
            }

            if(_body[KEY_OPTYPE].isNull() == true || _body[KEY_OPTYPE].isIntegral() == false)
            {
                ELOG("服务请求中没有操作类型或者操作类型类型错误！");
                return false;
            }

            if (_body[KEY_OPTYPE].asInt() != (int)ServiceOptype::SERVICE_DISCOVERY &&
                (_body[KEY_HOST].isNull() == true ||
                 _body[KEY_HOST].isObject() == false ||
                 _body[KEY_HOST][KEY_HOST_IP].isNull() == true ||
                 _body[KEY_HOST][KEY_HOST_IP].isString() == false ||
                 _body[KEY_HOST][KEY_HOST_PORT].isNull() == true ||
                 _body[KEY_HOST][KEY_HOST_PORT].isIntegral() == false))
            {
                ELOG("服务请求中主机地址信息错误！");
                return false;
            }

            return true;
        }

        std::string method()
        {
            return _body[KEY_METHOD].asString();
        }

        void setMethod(const std::string &name)
        {
            _body[KEY_METHOD] = name;
        }

        ServiceOptype optype()
        {
            return (ServiceOptype)_body[KEY_OPTYPE].asInt();
        }

        void setOptype(ServiceOptype optype)
        {
            _body[KEY_OPTYPE] = (int)optype;
        }

        Address host()
        {
            Address addr;
            addr.first = _body[KEY_HOST][KEY_HOST_IP].asString();
            addr.second = _body[KEY_HOST][KEY_HOST_PORT].asInt();
            return addr;
        }

        void setHost(const Address &host)
        {
            Json::Value val;
            val[KEY_HOST_IP] = host.first;
            val[KEY_HOST_PORT] = host.second;
            _body[KEY_HOST] = val;
        }
    };



    // rpc给客户端的响应/答复，需要检查响应状态码和结果（必须要有结果，结果类型随便），2个方法：设置和获取结果
    class RpcResponse : public JsonResponse
    {
    public:
        using ptr = std::shared_ptr<RpcResponse>;

        virtual bool check() override
        {
            if (_body[KEY_RCODE].isNull() == true || _body[KEY_RCODE].isIntegral() == false)
            {
                ELOG("RPC响应中没有响应状态码或者响应状态码类型错误！");
                return false;
            }

            // 仅在成功场景要求 result，错误场景允许无 result（避免误判合法错误响应）
            if ((RCode)_body[KEY_RCODE].asInt() == RCode::RCODE_OK &&
                _body[KEY_RESULT].isNull() == true)
            {
                ELOG("RPC响应中没有调用结果或者返回结果类型错误！");
                return false;
            }

            return true;
        }

        Json::Value result()
        {
            return _body[KEY_RESULT];
        }

        void setResult(const Json::Value &result)
        {
            _body[KEY_RESULT] = result;
        }
    };



    class TopicResponse : public JsonResponse
    {
    public:
        using ptr = std::shared_ptr<TopicResponse>;

    };



    // 注册中心的响应/答复，检查响应状态码、操作类型、主机的信息，6个函数分别是获取和设置状态码、操作类型、主机信息
    // 其中操作类型还是服务的注册、发现、上下线、未知
    class ServiceResponse : public JsonResponse
    {
    public:
        using ptr = std::shared_ptr<ServiceResponse>;

        virtual bool check() override
        {
            if (_body[KEY_RCODE].isNull() == true || _body[KEY_RCODE].isIntegral() == false)
            {
                ELOG("RPC响应中没有响应状态码或者响应状态码类型错误！");
                return false;
            }

            if (_body[KEY_OPTYPE].isNull() == true || _body[KEY_OPTYPE].isIntegral() == false)
            {
                ELOG("响应中没有操作类型或者操作类型类型错误！");
                return false;
            }

            // 服务发现成功时才必须包含 method + host 列表；失败响应允许只返回错误码
            if (_body[KEY_OPTYPE].asInt() == (int)ServiceOptype::SERVICE_DISCOVERY &&
                _body[KEY_RCODE].asInt() == (int)RCode::RCODE_OK &&
                (_body[KEY_METHOD].isNull() == true ||
                _body[KEY_METHOD].isString() == false ||
                _body[KEY_HOST].isNull() == true ||
                _body[KEY_HOST].isArray() == false))
            {
                ELOG("服务发现响应中响应信息字段错误！");
                return false;
            }

            return true;
        }

        ServiceOptype optype()
        {
            return (ServiceOptype)_body[KEY_OPTYPE].asInt();
        }

        void setOptype(ServiceOptype optype)
        {
            _body[KEY_OPTYPE] = (int)optype;
        }

        std::string method()
        {
            return _body[KEY_METHOD].asString();
        }

        void setMethod(const std::string &method)
        {
            _body[KEY_METHOD] = method;
        }

        void setHost(std::vector<Address> addrs)
        {
            for (auto &addr : addrs)
            {
                Json::Value val;
                val[KEY_HOST_IP] = addr.first;
                val[KEY_HOST_PORT] = addr.second;
                _body[KEY_HOST].append(val);
            }
        }

        std::vector<Address> hosts()
        {
            std::vector<Address> addrs;
            int sz = _body[KEY_HOST].size();
            for (int i = 0; i < sz; i++)
            {
                Address addr;
                addr.first = _body[KEY_HOST][i][KEY_HOST_IP].asString();
                addr.second = _body[KEY_HOST][i][KEY_HOST_PORT].asInt();
                addrs.push_back(addr);
            }

            return addrs;
        }
    };



    // 实现一个消息对象的生产工厂，会根据消息类型生成对应对象
    class MessageFactory
    {
    public:
        static BaseMessage::ptr create(MType mtype)
        {
            switch(mtype)
            {
                case MType::REQ_RPC : return std::make_shared<RpcRequest>();
                case MType::RSP_RPC : return std::make_shared<RpcResponse>();
                case MType::REQ_TOPIC : return std::make_shared<TopicRequest>();
                case MType::RSP_TOPIC : return std::make_shared<TopicResponse>();
                case MType::REQ_SERVICE : return std::make_shared<ServiceRequest>();
                case MType::RSP_SERVICE : return std::make_shared<ServiceResponse>();
            }

            return BaseMessage::ptr();
        }

        template<typename T, typename... Args>
        static std::shared_ptr<T> create(Args&&... args)
        {
            return std::make_shared<T>(std::forward<Args>(args)...);
        }
    };
}