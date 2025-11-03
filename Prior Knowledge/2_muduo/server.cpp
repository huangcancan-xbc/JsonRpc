/*
实现一个翻译服务器，将客户端发来的英语单词翻译成中文。
*/
#include <muduo/net/TcpServer.h>     // TCP 服务器类（封装监听、accept、连接管理）
#include <muduo/net/EventLoop.h>     // 事件循环类（Reactor，负责事件分发）
#include <muduo/net/TcpConnection.h> // TCP 连接类（封装一次 TCP 连接）
#include <muduo/net/Buffer.h>        // 缓冲区类（封装读写缓冲区）
#include <iostream>
#include <string>
#include <unordered_map>

class DictServer
{
public:
    DictServer(int port)
        // & _baseloop ：主事件循环对象（负责监听和分发事件）
        // muduo::net::InetAddress("0.0.0.0", port)：服务器监听地址（IP+端口），0.0.0.0表示本机所有网卡都可连接
        // DictServer：服务器名称，仅用于标识和日志
        // muduo::net::TcpServer::kReusePort ：端口复用选项，允许快速重启服务
        : _server(&_baseloop, muduo::net::InetAddress("0.0.0.0", port), "DictServer", muduo::net::TcpServer::kReusePort)
    {
        // 设置连接事件的回调函数（连接建立/管理），形参 _1 表示 muduo::net::TcpConnectionPtr 智能指针（指向当前连接对象）
        _server.setConnectionCallback(std::bind(&DictServer::onConnection, this, std::placeholders::_1));
        // 设置消息事件的回调函数，TCP 连接后有可读数据时触发
        // _1:当前连接对象指针 TcpConnectionPtr，_2:接收缓冲区 Buffer*，_3:时间戳 Timestamp（消息到达的时间）
        _server.setMessageCallback(std::bind(&DictServer::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));
    }

    void start()
    {
        _server.start();  // 先启动服务器监听（内部调用 listen() 并注册 accept 事件）
        _baseloop.loop(); // 再开始死循环事件监控
    }

private:
    // 连接建立或断开时触发的回调函数
    void onConnection(const muduo::net::TcpConnectionPtr &conn)
    {
        // conn->connected() 返回 true 表示连接建立，false 表示连接断开
        if (conn->connected())
        {
            std::cout << "连接建立！\n";
        }
        else
        {
            std::cout << "连接断开！\n";
        }
    }

    // 接收到客户端数据时触发的回调函数
    // conn:当前连接对象指针 TcpConnectionPtr，buf:接收缓冲区，time:时间戳（消息到达的时间）
    void onMessage(const muduo::net::TcpConnectionPtr &conn,
                   muduo::net::Buffer *buf,
                   muduo::Timestamp time)
    {
        static std::unordered_map<std::string, std::string> dict_map = {
            {"hello", "你好"},
            {"world", "世界"},
            {"dictionary", "字典"},
            {"connection", "连接"},
            {"disconnection", "断开"}};

        // Buffer 对象封装了读缓冲区
        // retrieveAllAsString() 从缓冲区中取出全部数据并清空缓冲区
        std::string msg = buf->retrieveAllAsString();
        std::string res;
        auto it = dict_map.find(msg);
        if (it != dict_map.end())
        {
            res = it->second;
        }
        else
        {
            res = "未知单词！";
        }

        conn->send(res); // 向客户端发送字符串数据
    }

private:
    muduo::net::EventLoop _baseloop; // 需要先定义！
    muduo::net::TcpServer _server;   // TCP 服务器对象
};

int main()
{
    DictServer server(8080); // 创建服务器对象并监听 8080 端口
    server.start();          // 启动服务器（进入事件循环）

    return 0;
}