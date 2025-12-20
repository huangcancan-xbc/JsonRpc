/*
    抽象层的实现
    * BaseMessage：消息的基本抽象
    * BaseBuffer：数据缓冲区抽象
    * BaseProtocol：协议解析抽象
    * BaseConnection：连接抽象
    * BaseServer/BaseClient：服务器与客户端的抽象
*/
#pragma once
#include <memory>
#include "fields.hpp"
#include <functional>

namespace rpc
{
    // 基础消息类型，所有的消息都继承于此
    class BaseMessage
    {
    public:
        using ptr = std::shared_ptr<BaseMessage>;
        virtual ~BaseMessage() {}
        virtual void setId(const std::string &id)
        {
            _rid = id;
        }

        // 获取消息 ID
        virtual std::string rid()
        {
            return _rid;
        }

        virtual void setMType(MType mtype)
        {
            _mtype = mtype;
        }

        // 获取消息类型
        virtual MType mtype()
        {
            return _mtype;
        }

        virtual std::string serialize() = 0;

        virtual bool unserialize(const std::string &msg) = 0;

        // 校验消息内容是否有效
        virtual bool check() = 0;

    private:
        MType _mtype;       // 消息类型
        std::string _rid;   // 消息的ID
    };


    // 网络缓冲区的抽象
    class BaseBuffer
    {
    public:
        using ptr = std::shared_ptr<BaseBuffer>;
        virtual size_t readableSize() = 0;                    // 缓冲区还剩多少字节可读
        virtual int32_t peekInt32() = 0;                      // 只读前4字节数据
        virtual int32_t readInt32() = 0;                      // 读取并清空前4字节数据
        virtual void retrieveInt32() = 0;                     // 直接扔掉/跳过（没用的字段）前4字节数据
        virtual std::string retrieveAsString(size_t len) = 0; // 读取并清空指定长度的数据（字符串）
    };


    // 解析和序列化的抽象
    class BaseProtocol
    {
    public:
        using ptr = std::shared_ptr<BaseProtocol>;

        // 从缓冲区解析消息
        virtual bool onMessage(const BaseBuffer::ptr &buf, BaseMessage::ptr &msg) = 0;
        virtual std::string serialize(const BaseMessage::ptr &msg) = 0;

        // 判断一条消息是否完整
        virtual bool canProcessed(const BaseBuffer::ptr &buf) = 0;
    };


    // 网络连接的抽象
    class BaseConnection
    {
    public:
        using ptr = std::shared_ptr<BaseConnection>;
        virtual void send(const BaseMessage::ptr &msg) = 0;
        virtual void shutdown() = 0;    // 关闭连接
        virtual bool connected() = 0;   // 是否已连接
    };


    // 回调类型
    using ConnectionCallback = std::function<void(const BaseConnection::ptr &)>;
    using CloseCallback = std::function<void(const BaseConnection::ptr &)>;
    using MessageCallback = std::function<void(const BaseConnection::ptr &, BaseMessage::ptr &)>;
    
    // 服务端的抽象
    class BaseServer
    {
    public:
        using ptr = std::shared_ptr<BaseServer>;

        // 连接建立时的回调
        virtual void setConnectionCallback(const ConnectionCallback &cb)
        {
            _cb_connection = cb;
        }

        // 连接关闭时的回调
        virtual void setCloseCallback(const CloseCallback &cb)
        {
            _cb_close = cb;
        }

        // 收到消息时的回调
        virtual void setMessageCallback(const MessageCallback &cb)
        {
            _cb_message = cb;
        }

        virtual void start() = 0;   // 启动服务器

    protected:
        ConnectionCallback _cb_connection;
        CloseCallback _cb_close;
        MessageCallback _cb_message;
    };


    // 客户端的抽象
    class BaseClient
    {
    public:
        using ptr = std::shared_ptr<BaseClient>;
        virtual void setConnectionCallback(const ConnectionCallback &cb)
        {
            _cb_connection = cb;
        }

        virtual void setCloseCallback(const CloseCallback &cb)
        {
            _cb_close = cb;
        }

        virtual void setMessageCallback(const MessageCallback &cb)
        {
            _cb_message = cb;
        }

        virtual void connect() = 0;                      // 连接服务器
        virtual bool send(const BaseMessage::ptr &) = 0; // 发送消息
        virtual void shutdown() = 0;                     // 关闭连接
        virtual bool connected() = 0;                    // 是否已经连接
        virtual BaseConnection::ptr connection() = 0;    // 获取底层连接对象

    protected:
        ConnectionCallback _cb_connection;
        CloseCallback _cb_close;
        MessageCallback _cb_message;
    };
}