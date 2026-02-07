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
#include <thread>
#include <chrono>


namespace rpc
{
    // 网络缓冲区的具体实现
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

        virtual void retrieveInt32() override
        {
            return _buf->retrieveInt32();
        }

        virtual std::string retrieveAsString(size_t len) override
        {
            return _buf->retrieveAsString(len);
        }

    private:
        muduo::net::Buffer *_buf; // 指向muduo库Buffer对象的指针（真正的网络缓冲区）
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



    // 消息格式：4字节总长度 4字节消息类型 4字节ID长度 不定长ID 不定长消息体 
    class LVProtocol : public BaseProtocol
    {
    public:
        using ptr = std::shared_ptr<BaseProtocol>;
        virtual bool onMessage(const BaseBuffer::ptr &buf, BaseMessage::ptr &msg) override
        {
            // 防御性检查：至少要有 total_len + mtype + idlen 三个字段
            if (buf->readableSize() < (lenFieldsLength + mtypeFieldsLength + idlenFieldsLength))
            {
                ELOG("消息头长度不足！");
                return false;
            }

            int32_t total_len = buf->readInt32();  // 读取总长度
            MType mtype = (MType)buf->readInt32(); // 读取数据类型
            int32_t idlen = buf->readInt32();      // 读取id长度

            const int32_t min_total_len = static_cast<int32_t>(mtypeFieldsLength + idlenFieldsLength);
            // 关键边界检查：防止 idlen/body_len 越界导致崩溃
            if (total_len < min_total_len || total_len > maxTotalLen)
            {
                ELOG("消息总长度非法: %d", total_len);
                return false;
            }

            if (idlen < 0 || idlen > (total_len - min_total_len))
            {
                ELOG("消息ID长度非法: %d, total_len: %d", idlen, total_len);
                return false;
            }

            int32_t body_len = total_len - idlen - idlenFieldsLength - mtypeFieldsLength;
            if (body_len < 0)
            {
                ELOG("消息正文长度非法: %d", body_len);
                return false;
            }

            size_t need_read_len = static_cast<size_t>(idlen) + static_cast<size_t>(body_len);
            if (buf->readableSize() < need_read_len)
            {
                ELOG("消息数据不完整，需读取: %zu, 当前可读: %zu", need_read_len, buf->readableSize());
                return false;
            }

            std::string id = buf->retrieveAsString(static_cast<size_t>(idlen));
            std::string body = buf->retrieveAsString(static_cast<size_t>(body_len));
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

            // 语义校验：字段缺失/类型错误的消息不进入业务层
            if (msg->check() == false)
            {
                ELOG("消息语义校验失败！");
                return false;
            }

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

        // 判断缓冲区中的数据量是否足够一条消息的处理，不够就等
        virtual bool canProcessed(const BaseBuffer::ptr &buf) override
        {
            if(buf->readableSize() < lenFieldsLength)
            {
                return false;
            }

            // 先确保消息头字段齐全，再判断是否可处理，避免非法帧导致读越界
            if (buf->readableSize() < (lenFieldsLength + mtypeFieldsLength + idlenFieldsLength))
            {
                return false;
            }
            
            int32_t total_len = buf->peekInt32();
            DLOG("total_len: %d", total_len);
            const int32_t min_total_len = static_cast<int32_t>(mtypeFieldsLength + idlenFieldsLength);
            if (total_len < min_total_len || total_len > maxTotalLen)
            {
                // 非法长度，交给 onMessage 返回 false 并由上层断开连接
                return true;
            }

            size_t packet_len = static_cast<size_t>(total_len) + lenFieldsLength;
            if (buf->readableSize() < packet_len)
            {
                return false;
            }

            return true;
        }

    private:
        const size_t lenFieldsLength = 4;       // 总长度
        const size_t mtypeFieldsLength = 4;     // 消息类型
        const size_t idlenFieldsLength = 4;     // ID长度
        const int32_t maxTotalLen = (1 << 16);  // 与服务端缓冲上限保持一致，避免超大帧
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



    // 对muduo::net::TcpConnection做简单的封装
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

        // 关闭连接
        virtual void shutdown() override
        {
            _conn->shutdown();
        }

        // 检查连接状态
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



    // rpc服务器
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
        const size_t maxDataSize = (1 << 16);                                         // 缓冲区的最大值
        BaseProtocol::ptr _protocol;                                                  // 负责消息的打包和解包
        muduo::net::EventLoop _baseloop;                                              // Muduo循环，服务器持续运行
        muduo::net::TcpServer _server;                                                // 网络通信（muduo的TCP）
        std::mutex _mutex;                                                            // 互斥锁
        std::unordered_map<muduo::net::TcpConnectionPtr, BaseConnection::ptr> _conns; // key：tcp,val：连接对象
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



    // rpc客户端
    class MuduoClient : public BaseClient
    {
    public:
        using ptr = std::shared_ptr<MuduoClient>;

        MuduoClient(const std::string &sip, int sport)
            : _loopthread(),
              _baseloop(_loopthread.startLoop()),
              _downlatch(1),
              _protocol(ProtocolFactory::create()),
              _conn(),
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
            // 之前这里是无限等待，服务不可达时可能会卡死；改成有限等待
            const int max_wait_ms = 3000;
            int waited_ms = 0;
            while (_downlatch.getCount() > 0 && waited_ms < max_wait_ms)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                waited_ms += 10;
            }

            if (_downlatch.getCount() > 0)
            {
                ELOG("连接服务器超时！");
                return;
            }

            if (connected() == false)
            {
                ELOG("连接服务器失败！");
                return;
            }

            DLOG("连接服务器成功！");
        }

        virtual void shutdown() override
        {
            return _client.disconnect();
        }

        virtual bool send(const BaseMessage::ptr &msg) override
        {
            BaseConnection::ptr conn;
            {
                std::unique_lock<std::mutex> lock(_conn_mutex);
                conn = _conn;
            }

            if (!conn || conn->connected() == false)
            {
                ELOG("连接已断开！");
                return false;
            }

            conn->send(msg);
            return true;
        }

        virtual BaseConnection::ptr connection() override
        {
            std::unique_lock<std::mutex> lock(_conn_mutex);
            return _conn;
        }

        virtual bool connected() override
        {
            std::unique_lock<std::mutex> lock(_conn_mutex);
            return (_conn && _conn->connected());
        }

    private:
        void onConnection(const muduo::net::TcpConnectionPtr &conn)
        {
            if (conn->connected())
            {
                std::cout << "连接建立！" << std::endl;
                // _conn 在网络线程写入，业务线程会读取，这里加锁避免数据竞争
                {
                    std::unique_lock<std::mutex> lock(_conn_mutex);
                    _conn = ConnectionFactory::create(conn, _protocol);
                }

                // 之前的代码在这里可能发生了线程切换/竞争！

                if (_downlatch.getCount() > 0)
                {
                    _downlatch.countDown(); // 唤醒 connect() 等待
                }
            }
            else
            {
                std::cout << "连接断开！" << std::endl;
                {
                    std::unique_lock<std::mutex> lock(_conn_mutex);
                    _conn.reset();
                }

                // 连接失败也要唤醒等待线程，避免永久阻塞
                if (_downlatch.getCount() > 0)
                {
                    _downlatch.countDown();
                }
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
                    BaseConnection::ptr conn_snapshot;
                    {
                        std::unique_lock<std::mutex> lock(_conn_mutex);
                        conn_snapshot = _conn;
                    }

                    if (!conn_snapshot)
                    {
                        ELOG("消息处理时连接对象为空！");
                        return;
                    }

                    _cb_message(conn_snapshot, msg);
                }
            }
        }

    private:
        const size_t maxDataSize = (1 << 16); // 64K的最大缓冲区
        muduo::net::EventLoopThread _loopthread;
        muduo::net::EventLoop *_baseloop;
        muduo::CountDownLatch _downlatch;
        BaseProtocol::ptr _protocol;
        BaseConnection::ptr _conn;
        std::mutex _conn_mutex;
        muduo::net::TcpClient _client; // muduo的tcp客户端
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