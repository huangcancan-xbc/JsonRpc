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
#include "detail.hpp"
#include "fields.hpp"
#include "abstract.hpp"

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
}