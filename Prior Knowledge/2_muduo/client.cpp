#include <muduo/net/TcpClient.h>       // TCP 客户端类（封装 connect、连接管理）
#include <muduo/net/EventLoop.h>       // 事件循环类（Reactor 模型核心）
#include <muduo/net/EventLoopThread.h> // 专用线程封装类，用于在独立线程中运行 EventLoop
#include <muduo/net/TcpConnection.h>   // TCP 连接类
#include <muduo/net/Buffer.h>          // 缓冲区类（封装 I/O 读写缓存）
#include <muduo/base/CountDownLatch.h> // 倒计时锁（用于线程同步）
#include <iostream>
#include <string>

class DictClient
{
public:
    DictClient(const std::string &sip, int sport)
        // 启动一个独立线程，并在该线程内运行 EventLoop 事件循环,startLoop() 返回该线程内的 EventLoop* 指针。
        : _baseloop(_loopthread.startLoop()),
          _downlatch(1), // 初始化倒计时锁，必须初始化为 1，用于阻塞主线程，等待连接建立成功
          // EventLoop* loop ：事件循环指针，InetAddress(sip, sport) ：服务器 IP + 端口，DictClient：客户端名称，仅用于日志输出
          _client(_baseloop, muduo::net::InetAddress(sip, sport), "DictClient")
    {
        // 设置连接事件回调函数（连接建立 / 断开都会触发），参数：当前连接对象 TcpConnectionPtr
        _client.setConnectionCallback(std::bind(&DictClient::onConnection, this, std::placeholders::_1));
        // 设置消息事件回调函数（服务器有数据发送到客户端时触发），_1 -> TcpConnectionPtr 当前连接， _2 -> Buffer* 接收缓冲区， _3 -> Timestamp 消息到达时间
        _client.setMessageCallback(std::bind(&DictClient::onMessage, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3));

        _client.connect(); // 主动发起连接：内部调用 connect() → 非阻塞连接过程，回调在事件循环线程触发
        _downlatch.wait(); // 等待连接建立成功（阻塞当前线程），onConnection() 回调中会执行 _downlatch.countDown() 唤醒
    }

    bool send(const std::string &msg)
    {
        if (_conn->connected() == false) // 检查连接是否有效
        {
            std::cout << "连接已断开，发送数据失败！" << std::endl;
            return false;
        }
        _conn->send(msg); // 调用 TcpConnection::send() 发送数据
        return true;
    }

private:
    // 连接建立 / 断开时触发
    void onConnection(const muduo::net::TcpConnectionPtr &conn)
    {
        if (conn->connected())
        {
            std::cout << "连接建立成功！" << std::endl;
            _downlatch.countDown(); // 计数--，为0时唤醒阻塞

            _conn = conn; // 保存连接指针，供 send() 使用
        }
        else
        {
            std::cout << "连接断开！" << std::endl;
            _conn.reset(); // 清空连接对象
        }
    }

    // 接收到服务器消息时触发，conn:当前连接对象指针 TcpConnectionPtr，buf:接收缓冲区，time:时间戳（消息到达的时间）
    void onMessage(const muduo::net::TcpConnectionPtr &conn, muduo::net::Buffer *buf, muduo::Timestamp time)
    {
        std::string res = buf->retrieveAllAsString(); // retrieveAllAsString() 从缓冲区取出全部数据并清空缓冲区
        std::cout << res << std::endl;                // 输出翻译结果
    }

private:
    muduo::net::TcpConnectionPtr _conn;      // 保存当前连接（智能指针，线程安全）
    muduo::CountDownLatch _downlatch;        // 倒计时锁：用于等待连接建立完成
    muduo::net::EventLoopThread _loopthread; // 独立的事件循环线程
    muduo::net::EventLoop *_baseloop;        // 指向 _loopthread 中的事件循环对象
    muduo::net::TcpClient _client;           // TCP 客户端对象
};

int main()
{
    DictClient client("127.0.0.1", 8080);
    while (1)
    {
        std::string msg;
        // std::cout << "请输入要翻译的英文单词：" << std::endl;
        std::cin >> msg;
        client.send(msg);
    }

    return 0;
}