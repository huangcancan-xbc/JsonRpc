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
            int32_t h_total_len = mtypeFieldsLength + idlenFieldsLength + id.size() + body.size();
            int32_t n_total_len = htonl(h_total_len);
            DLOG("h_total_len: %d", h_total_len);
            std::string result;
            result.reserve(h_total_len);
            result.append((char *)&n_total_len, lenFieldsLength);
            result.append((char *)&mtype, mtypeFieldsLength);
            result.append((char *)&idlen, idlenFieldsLength);
            result.append(id);
            result.append(body);
            return result;
        }

        // 判断缓冲区中的数据量是否足够一条消息的处理
        virtual bool canProcessed(const BaseBuffer::ptr &buf) override
        {
            if(buf->readableSize() < lenFieldsLength)
            {
                return false;
            }
            
            int32_t total_len = buf->peekInt32();
            DLOG("total_len: %d", total_len);
            if (buf->readableSize() < (total_len + lenFieldsLength))
            {
                return false;
            }

            return true;
        }

    private:
        const size_t lenFieldsLength = 4;
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
            return _conn->connected();
        }

    private:
        BaseProtocol::ptr _protocol;
        muduo::net::TcpConnectionPtr _conn;
    };

    class ConnectionFactory
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
                auto muduo_conn = ConnectionFactory::create(conn, _protocol);
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

                    muduo_conn = it->second;
                    _conns.erase(it);
                }

                if (_cb_close)
                {
                    _cb_close(muduo_conn);
                }
            }
        }

        void onMessage(const muduo::net::TcpConnectionPtr& conn, muduo::net::Buffer* buf, muduo::Timestamp)
        {
            DLOG("连接有新数据到来，开始处理！");
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

                    DLOG("数据量不足！");
                    break;
                }

                DLOG("缓冲区数据可处理！");
                BaseMessage::ptr msg;
                bool ret = _protocol->onMessage(base_buf, msg);
                if (ret == false)
                {
                    conn->shutdown();
                    ELOG("缓冲区中的数据错误！");
                    return;
                }

                DLOG("消息反序列化成功！");
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

                DLOG("调用回调函数进行消息处理！");
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

    class ServerFactory
    {
    public:
        template <typename... Args>
        static BaseServer::ptr create(Args &&...args)
        {
            return std::make_shared<MuduoServer>(std::forward<Args>(args)...);
        }
    };

    class MuduoClient : public BaseClient
    {
    public:
        using ptr = std::shared_ptr<MuduoClient>;

        MuduoClient(const std::string &sip, int sport)
            : _protocol(ProtocolFactory::create()),
              _baseloop(_loopthread.startLoop()),
              _downlatch(1),
              _client(_baseloop, muduo::net::InetAddress(sip, sport), "MuduoClient")
        {
        }

        virtual void connect() override
        {
            DLOG("设置回调函数，连接服务器！");
            _client.setConnectionCallback(std::bind(&MuduoClient::onConnection, this, std::placeholders::_1));
            // 设置连接消息的回调
            _client.setMessageCallback(std::bind(&MuduoClient::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

            // 连接服务器
            _client.connect();
            _downlatch.wait();
            DLOG("连接服务器成功！");
        }

        virtual void shutdown() override
        {
            return _client.disconnect();
        }

        virtual bool send(const BaseMessage::ptr &msg) override
        {
            if (connected() == false)
            {
                ELOG("连接已断开！");
                return false;
            }

            _conn->send(msg);
            return true;
        }

        virtual BaseConnection::ptr connection() override
        {
            return _conn;
        }

        virtual bool connected() override
        {
            return (_conn && _conn->connected());
        }

    private:
        void onConnection(const muduo::net::TcpConnectionPtr &conn)
        {
            if (conn->connected())
            {
                std::cout << "连接建立！" << std::endl;
                _downlatch.countDown();
                _conn = ConnectionFactory::create(conn, _protocol);
            }
            else
            {
                std::cout << "连接断开！" << std::endl;
                _conn.reset();
            }
        }

        void onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf, muduo::Timestamp)
        {
            DLOG("连接有新数据到来，开始处理！");
            auto base_buf = BufferFactory::create(buf);
            while (1)
            {
                if (_protocol->canProcessed(base_buf) == false)
                {
                    // 数据不足
                    if (base_buf->readableSize() > maxDataSize)
                    {
                        conn->shutdown();
                        ELOG("缓冲区中数据过大！");
                        return;
                    }

                    DLOG("数据量不足！");
                    break;
                }

                DLOG("缓冲区数据可处理！");
                BaseMessage::ptr msg;
                bool ret = _protocol->onMessage(base_buf, msg);
                if (ret == false)
                {
                    conn->shutdown();
                    ELOG("缓冲区中数据错误！");
                    return;
                }

                DLOG("缓冲区数据解析完毕！调用回调函数进行处理！");
                if (_cb_message)
                {
                    _cb_message(_conn, msg);
                }
            }
        }

    private:
        const size_t maxDataSize = (1 << 16);
        BaseProtocol::ptr _protocol;
        BaseConnection::ptr _conn;
        muduo::CountDownLatch _downlatch;
        muduo::net::EventLoopThread _loopthread;
        muduo::net::EventLoop *_baseloop;
        muduo::net::TcpClient _client;
    };

    class ClientFactory
    {
    public:
        template <typename... Args>
        static BaseClient::ptr create(Args... args)
        {
            return std::make_shared<MuduoClient>(std::forward<Args>(args)...);
        }
    };
}