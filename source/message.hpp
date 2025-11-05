
#pragma once
#include "detail.hpp"
#include "fields.hpp"
#include "abstract.hpp"

namespace rpc
{
    class JsonMessage : public BaseMessage
    {
    public:
        using ptr = std::shared_ptr<BaseMessage>;

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

        // virtual bool check() override
        // {

        // }

    protected:
        Json::Value _body;
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
    };


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

    class ServiceRequest : public JsonRequest
    {
    public:
        using ptr = std::shared_ptr<ServiceRequest>;
    };

    typedef std::pair<std::string, int> Address;
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

            if (_body[KEY_HOST].isNull() == true ||
                _body[KEY_HOST].isObject() == false ||
                _body[KEY_HOST_IP].isNull() == true ||
                _body[KEY_HOST_IP].isString() == false ||
                _body[KEY_HOST_PORT].isNull() == true ||
                _body[KEY_HOST_PORT].isIntegral() == false)
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

        void setOptype(TopicOptype optype)
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
}