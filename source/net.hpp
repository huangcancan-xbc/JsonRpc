/*
    网络通信相关的操作
*/
#pragma once
#include <muduo/net/TcpServer.h>       // TCP 服务器类（封装监听、accept、连接管理）
#include <muduo/net/EventLoop.h>       // 事件循环类（Reactor，负责事件分发）
#include <muduo/net/TcpConnection.h>   // TCP 连接类（封装一次 TCP 连接）
#include <muduo/net/Buffer.h>          // 缓冲区类（封装读写缓冲区）
#include <muduo/base/CountDownLatch.h> // 倒计时锁（用于线程同步）
#include <muduo/net/EventLoopThread.h> // 专用线程封装类，用于在独立线程中运行 EventLoop
#include <muduo/net/TcpClient.h>       // TCP 客户端类（封装 connect、连接管理）
#include "detail.hpp"
#include "fields.hpp"
#include "abstract.hpp"
#include "message.hpp"
#include <mutex>
#include <unordered_map>


namespace rpc
{
    class MuduoBuffer : public BaseBuffer
    {
    public:
        using ptr = std::shared_ptr<BaseBuffer>;

        MuduoBuffer(muduo::net::Buffer *buf) : _buf(buf) {}

        virtual size_t readableSize() override
        {
            return _buf->readableBytes();
        }

        virtual int32_t peekInt32() override
        {
            // 注意：muduo库是一个网络库，从缓冲区取出一个4字节的整形，会进行网络字节序的转换
            return _buf->peekInt32();
        }

        virtual int32_t readInt32() override
        {
            return _buf->readInt32();
        }

        virtual void retrievenInt32() override
        {
            return _buf->retrieveInt32();
        }

        virtual std::string retrieveAsString(size_t len) override
        {
            return _buf->retrieveAsString(len);
        }

    private:
        muduo::net::Buffer *_buf;
    };

    class BufferFactory
    {
    public:
        template <typename... Args>
        static BaseBuffer::ptr create(Args &&...args)
        {
            return std::make_shared<MuduoBuffer>(std::forward<Args>(args)...);
        }
    };



    class LVProtocol : public BaseProtocol
    {
    public:
        using ptr = std::shared_ptr<BaseProtocol>;
        virtual bool onMessage(const BaseBuffer::ptr &buf, BaseMessage::ptr &msg) override
        {
            int32_t total_len = buf->readInt32();  // 读取总长度
            MType mtype = (MType)buf->readInt32(); // 读取数据类型
            int32_t idlen = buf->readInt32();      // 读取id长度
            int32_t body_len = total_len - idlen - idlenFieldsLength - mtypeFieldsLength;
            std::string id = buf->retrieveAsString(idlen);
            std::string body = buf->retrieveAsString(body_len);
            msg = MessageFactory::create(mtype);
            if (msg.get() == nullptr)
            {
                ELOG("消息类型错误，构造消息对象失败！");
                return false;
            }

            bool ret = msg->unserialize(body);
            if (ret == false)
            {
                ELOG("消息正文反序列化失败！");
                return false;
            }

            msg->setId(id);
            msg->setMType(mtype);

            return true;
        }

        virtual std::string serialize(const BaseMessage::ptr &msg) override
        {
            std::string body = msg->serialize();
            std::string id = msg->rid();
            auto mtype = htonl((int32_t)msg->mtype());
            int32_t idlen = htonl(id.size());
            int32_t total_len = htonl(mtypeFieldsLength + idlenFieldsLength + idlen + body.size());
            std::string result;
            result.reserve(total_len);
            result.append((char *)&total_len, lernFieldsLength);
            result.append((char *)&mtype, mtypeFieldsLength);
            result.append((char *)&idlen, idlenFieldsLength);
            result.append(id);
            result.append(body);
            return result;
        }

        // 判断缓冲区中的数据量是否足够一条消息的处理
        virtual bool canProcessed(const BaseBuffer::ptr &buf) override
        {
            int32_t total_len = buf->peekInt32();
            if (buf->readableSize() < (total_len + lernFieldsLength))
            {
                return false;
            }

            return true;
        }

    private:
        const size_t lernFieldsLength = 4;
        const size_t mtypeFieldsLength = 4;
        const size_t idlenFieldsLength = 4;
    };

    class ProtocolFactory
    {
    public:
        template <typename... Args>
        static BaseProtocol::ptr create(Args &&...args)
        {
            return std::make_shared<LVProtocol>(std::forward<Args>(args)...);
        }
    };



    class MuduoConnection : public BaseConnection
    {
    public:
        using ptr = std::shared_ptr<BaseConnection>;

        MuduoConnection(const muduo::net::TcpConnectionPtr conn, const BaseProtocol::ptr &protocol)
            :  _protocol(protocol),
            _conn(conn)
        {

        }

        virtual void send(const BaseMessage::ptr &msg) override
        {
            std::string body = _protocol->serialize(msg);
            _conn->send(body);
        }

        virtual void shutdown() override
        {
            _conn->shutdown();
        }

        virtual bool connected() override
        {
            _conn->connected();
        }

    private:
        BaseProtocol::ptr _protocol;
        muduo::net::TcpConnectionPtr _conn;
    };

    class ConnetionFactor
    {
    public:
        template <typename... Args>
        static BaseConnection::ptr create(Args &&...args)
        {
            return std::make_shared<MuduoConnection>(std::forward<Args>(args)...);
        }
    };



    class MuduoServer : public BaseServer
    {
    public:
        using ptr = std::shared_ptr<MuduoServer>;

        MuduoServer(int port)
            : _server(&_baseloop, muduo::net::InetAddress("0.0.0.0", port), "MuduoServer", muduo::net::TcpServer::kReusePort),
            _protocol(ProtocolFactory::create())
        {

        }

        virtual void start()
        {
            _server.setConnectionCallback(std::bind(&MuduoServer::onConnection, this, std::placeholders::_1));
            _server.setMessageCallback(std::bind(&MuduoServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
            _server.start();
            _baseloop.loop();
        }
    private:
        void onConnection(const muduo::net::TcpConnectionPtr& conn)
        {
            if (conn->connected())
            {
                std::cout << "连接建立！" << std::endl;
                auto muduo_conn = ConnetionFactor::create(conn, _protocol);
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    _conns.insert(std::make_pair(conn, muduo_conn));
                }

                if (_cb_connection)
                {
                    _cb_connection(muduo_conn);
                }
            }
            else
            {
                std::cout << "连接断开！" << std::endl;
                BaseConnection::ptr muduo_conn;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _conns.find(conn);
                    if (it == _conns.end())
                    {
                        return;
                    }

                    muduo_conn = it->second();
                    _conns.erase(it);
                }

                if (_cb_close)
                {
                    _cb_close(muduo_conn);
                }
            }
        }

        void onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp time)
        {
            auto base_buf = BufferFactory::create(buf);

            while (1)
            {
                if (_protocol->canProcessed(base_buf) == false)
                {
                    if (base_buf->readableSize() > maxDataSize)
                    {
                        conn->shutdown();
                        ELOG("缓冲区中的数据过大！");
                        return;
                    }
                    break;
                }

                BaseMessage::ptr msg;
                bool ret = _protocol->onMessage(base_buf, msg);
                if (ret == false)
                {
                    conn->shutdown();
                    ELOG("缓冲区中的数据错误！");
                    return;
                }

                BaseConnection::ptr base_conn;
                {
                    std::unique_lock<std::mutex> lock(_mutex);
                    auto it = _conns.find(conn);
                    if (it == _conns.end())
                    {
                        return;
                    }

                    base_conn = it->second;
                }

                if (_cb_message)
                {
                    _cb_message(base_conn, msg);
                }
            }
        }

    private:
        const size_t maxDataSize = (1 << 16);
        BaseProtocol::ptr _protocol;
        muduo::net::EventLoop _baseloop;
        muduo::net::TcpServer _server;
        std::mutex _mutex;
        std::unordered_map<muduo::net::TcpConnectionPtr, BaseConnection::ptr> _conns;
    };

    class ServerFactor
    {
    public:
        template <typename... Args>
        static BaseServer::ptr create(Args &&...args)
        {
            return std::make_shared<MuduoServer>(std::forward<Args>(args)...);
        }
    };
}