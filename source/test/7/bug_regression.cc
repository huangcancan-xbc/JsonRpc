#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <jsoncpp/json/json.h>
#include <chrono>
#include <cstring>
#include <functional>
#include <iostream>
#include <map>
#include <string>
#include <thread>
#include <vector>

namespace
{
    const int REGISTRY_PORT = 9090;
    const int REQ_SERVICE = 4;
    const int RSP_SERVICE = 5;

    enum class CaseStatus
    {
        PASS = 0,
        FAIL = 1,
        SKIP = 2
    };

    struct CaseResult
    {
        std::string name;
        CaseStatus status;
        std::string detail;
    };

    class ChildProcess
    {
    public:
        ChildProcess() : _pid(-1) {}

        bool start(const std::string &bin, const std::vector<std::string> &args = std::vector<std::string>())
        {
            if (_pid > 0)
            {
                return false;
            }

            _pid = fork();
            if (_pid < 0)
            {
                return false;
            }

            if (_pid == 0)
            {
                std::vector<char *> argv;
                argv.reserve(args.size() + 2);
                argv.push_back(const_cast<char *>(bin.c_str()));
                for (auto &arg : args)
                {
                    argv.push_back(const_cast<char *>(arg.c_str()));
                }
                argv.push_back(nullptr);

                execv(bin.c_str(), argv.data());
                _exit(127);
            }

            return true;
        }

        bool alive() const
        {
            if (_pid <= 0)
            {
                return false;
            }
            return kill(_pid, 0) == 0;
        }

        void stop()
        {
            if (_pid <= 0)
            {
                return;
            }

            int status = 0;
            if (waitpid(_pid, &status, WNOHANG) == _pid)
            {
                _pid = -1;
                return;
            }

            kill(_pid, SIGTERM);
            for (int i = 0; i < 20; i++)
            {
                if (waitpid(_pid, &status, WNOHANG) == _pid)
                {
                    _pid = -1;
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }

            kill(_pid, SIGKILL);
            waitpid(_pid, &status, 0);
            _pid = -1;
        }

        ~ChildProcess()
        {
            stop();
        }

    private:
        pid_t _pid;
    };

    class FakeRegistry
    {
    public:
        FakeRegistry() : _listen_fd(-1), _conn_fd(-1), _stop(false) {}

        bool start()
        {
            _listen_fd = socket(AF_INET, SOCK_STREAM, 0);
            if (_listen_fd < 0)
            {
                return false;
            }

            int opt = 1;
            setsockopt(_listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

            sockaddr_in addr;
            std::memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(REGISTRY_PORT);
            inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
            if (bind(_listen_fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0)
            {
                close(_listen_fd);
                _listen_fd = -1;
                return false;
            }

            if (listen(_listen_fd, 1) != 0)
            {
                close(_listen_fd);
                _listen_fd = -1;
                return false;
            }

            int flags = fcntl(_listen_fd, F_GETFL, 0);
            fcntl(_listen_fd, F_SETFL, flags | O_NONBLOCK);

            _stop = false;
            _worker = std::thread(&FakeRegistry::workerLoop, this);
            return true;
        }

        void stop()
        {
            _stop = true;
            if (_conn_fd >= 0)
            {
                shutdown(_conn_fd, SHUT_RDWR);
                close(_conn_fd);
                _conn_fd = -1;
            }

            if (_listen_fd >= 0)
            {
                shutdown(_listen_fd, SHUT_RDWR);
                close(_listen_fd);
                _listen_fd = -1;
            }

            if (_worker.joinable())
            {
                _worker.join();
            }
        }

        ~FakeRegistry()
        {
            stop();
        }

    private:
        void workerLoop()
        {
            while (!_stop)
            {
                sockaddr_in caddr;
                socklen_t clen = sizeof(caddr);
                int cfd = accept(_listen_fd, reinterpret_cast<sockaddr *>(&caddr), &clen);
                if (cfd >= 0)
                {
                    _conn_fd = cfd;
                    break;
                }

                if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                {
                    return;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }

            if (_conn_fd < 0)
            {
                return;
            }

            char buf[4096];
            while (!_stop)
            {
                ssize_t n = recv(_conn_fd, buf, sizeof(buf), MSG_DONTWAIT);
                if (n == 0)
                {
                    break;
                }

                if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)
                {
                    break;
                }

                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
        }

    private:
        int _listen_fd;
        int _conn_fd;
        bool _stop;
        std::thread _worker;
    };

    bool isPortFree(int port)
    {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
        {
            return false;
        }

        int opt = 1;
        setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(port);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        bool free = (bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) == 0);
        close(fd);
        return free;
    }

    int runWithTimeout(const std::string &bin, int timeout_sec)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            return -2;
        }

        if (pid == 0)
        {
            execl(bin.c_str(), bin.c_str(), (char *)nullptr);
            _exit(127);
        }

        auto begin = std::chrono::steady_clock::now();
        while (true)
        {
            int status = 0;
            pid_t ret = waitpid(pid, &status, WNOHANG);
            if (ret == pid)
            {
                if (WIFEXITED(status))
                {
                    return WEXITSTATUS(status);
                }
                return -3;
            }

            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::steady_clock::now() - begin).count();
            if (elapsed >= timeout_sec)
            {
                kill(pid, SIGKILL);
                waitpid(pid, &status, 0);
                return -1;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    bool sendRawFrame(const std::vector<unsigned char> &frame)
    {
        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
        {
            return false;
        }

        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(REGISTRY_PORT);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

        if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0)
        {
            close(fd);
            return false;
        }

        ssize_t n = send(fd, frame.data(), frame.size(), 0);
        close(fd);
        return n == static_cast<ssize_t>(frame.size());
    }

    CaseResult caseMalformed()
    {
        if (!isPortFree(REGISTRY_PORT))
        {
            return {"malformed", CaseStatus::SKIP, "9090 端口被占用，无法隔离执行"};
        }

        ChildProcess reg;
        if (!reg.start("../5/reg_server"))
        {
            return {"malformed", CaseStatus::FAIL, "启动 reg_server 失败"};
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (!reg.alive())
        {
            return {"malformed", CaseStatus::FAIL, "reg_server 启动后已退出"};
        }

        const std::vector<unsigned char> malformed = {
            0x00, 0x00, 0x00, 0x0c, // total_len = 12
            0x00, 0x00, 0x00, 0x04, // mtype = REQ_SERVICE
            0x00, 0x00, 0x00, 0x64, // idlen = 100 (非法)
            'A', 'B', 'C', 'D'
        };

        if (!sendRawFrame(malformed))
        {
            return {"malformed", CaseStatus::FAIL, "发送畸形帧失败"};
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        bool alive = reg.alive();
        reg.stop();
        if (!alive)
        {
            return {"malformed", CaseStatus::FAIL, "服务端被畸形帧打崩"};
        }
        return {"malformed", CaseStatus::PASS, "畸形帧后服务端仍存活"};
    }

    CaseResult caseConnect()
    {
        if (!isPortFree(REGISTRY_PORT))
        {
            return {"connect", CaseStatus::SKIP, "9090 端口被占用，无法验证不可达场景"};
        }

        int rc = runWithTimeout("../5/client", 8);
        if (rc == -1)
        {
            return {"connect", CaseStatus::FAIL, "客户端连接失败后仍长时间卡住"};
        }
        if (rc < -1)
        {
            return {"connect", CaseStatus::FAIL, "启动客户端执行失败"};
        }
        return {"connect", CaseStatus::PASS, "连接不可达场景可在超时内返回"};
    }

    CaseResult caseSync()
    {
        if (!isPortFree(REGISTRY_PORT))
        {
            return {"sync", CaseStatus::SKIP, "9090 端口被占用，无法启动 fake registry"};
        }

        FakeRegistry fake;
        if (!fake.start())
        {
            return {"sync", CaseStatus::FAIL, "fake registry 启动失败"};
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        int rc = runWithTimeout("../5/client", 8);
        fake.stop();

        if (rc == -1)
        {
            return {"sync", CaseStatus::FAIL, "同步调用在无响应场景下仍会无限等待"};
        }
        if (rc < -1)
        {
            return {"sync", CaseStatus::FAIL, "客户端执行失败"};
        }
        return {"sync", CaseStatus::PASS, "同步调用在无响应场景可按时返回"};
    }

    CaseResult caseSemantic()
    {
        if (!isPortFree(REGISTRY_PORT))
        {
            return {"semantic", CaseStatus::SKIP, "9090 端口被占用，无法隔离执行"};
        }

        ChildProcess reg;
        if (!reg.start("../5/reg_server"))
        {
            return {"semantic", CaseStatus::FAIL, "启动 reg_server 失败"};
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));

        int fd = socket(AF_INET, SOCK_STREAM, 0);
        if (fd < 0)
        {
            return {"semantic", CaseStatus::FAIL, "创建 socket 失败"};
        }

        sockaddr_in addr;
        std::memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_port = htons(REGISTRY_PORT);
        inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
        if (connect(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) != 0)
        {
            close(fd);
            return {"semantic", CaseStatus::FAIL, "连接 reg_server 失败"};
        }

        std::string rid = "bad1";
        std::string body = "{}";
        int32_t total_len = 4 + 4 + static_cast<int32_t>(rid.size()) + static_cast<int32_t>(body.size());
        int32_t net_total = htonl(total_len);
        int32_t net_mtype = htonl(REQ_SERVICE);
        int32_t net_idlen = htonl(static_cast<int32_t>(rid.size()));

        std::string frame;
        frame.append(reinterpret_cast<char *>(&net_total), 4);
        frame.append(reinterpret_cast<char *>(&net_mtype), 4);
        frame.append(reinterpret_cast<char *>(&net_idlen), 4);
        frame.append(rid);
        frame.append(body);
        if (send(fd, frame.data(), frame.size(), 0) != static_cast<ssize_t>(frame.size()))
        {
            close(fd);
            return {"semantic", CaseStatus::FAIL, "发送非法请求失败"};
        }

        timeval tv;
        tv.tv_sec = 1;
        tv.tv_usec = 0;
        setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        char header[12];
        ssize_t n = recv(fd, header, sizeof(header), MSG_WAITALL);
        if (n < 12)
        {
            close(fd);
            reg.stop();
            return {"semantic", CaseStatus::PASS, "非法请求未被当作成功处理（连接已关闭或无响应）"};
        }

        int32_t recv_net_total = 0;
        int32_t recv_net_mtype = 0;
        int32_t recv_net_idlen = 0;
        std::memcpy(&recv_net_total, &header[0], 4);
        std::memcpy(&recv_net_mtype, &header[4], 4);
        std::memcpy(&recv_net_idlen, &header[8], 4);
        int32_t total = ntohl(recv_net_total);
        int32_t mtype = ntohl(recv_net_mtype);
        int32_t idlen = ntohl(recv_net_idlen);
        if (total < 8 || idlen < 0 || total < 8 + idlen)
        {
            close(fd);
            reg.stop();
            return {"semantic", CaseStatus::PASS, "非法请求响应格式异常，视为未成功"};
        }

        std::vector<char> idbuf(static_cast<size_t>(idlen));
        if (idlen > 0 && recv(fd, idbuf.data(), idbuf.size(), MSG_WAITALL) < static_cast<ssize_t>(idbuf.size()))
        {
            close(fd);
            reg.stop();
            return {"semantic", CaseStatus::PASS, "非法请求未返回完整成功响应"};
        }

        int body_len = total - 8 - idlen;
        std::string rsp_body(static_cast<size_t>(body_len), '\0');
        if (body_len > 0 && recv(fd, &rsp_body[0], static_cast<size_t>(body_len), MSG_WAITALL) < body_len)
        {
            close(fd);
            reg.stop();
            return {"semantic", CaseStatus::PASS, "非法请求未返回完整成功响应"};
        }
        close(fd);
        reg.stop();

        Json::Value json_rsp;
        Json::CharReaderBuilder crb;
        std::string errs;
        std::unique_ptr<Json::CharReader> reader(crb.newCharReader());
        bool ok = reader->parse(rsp_body.data(), rsp_body.data() + rsp_body.size(), &json_rsp, &errs);
        if (!ok)
        {
            return {"semantic", CaseStatus::PASS, "非法请求未返回可解析成功响应"};
        }

        if (mtype == RSP_SERVICE && json_rsp["rcode"].isInt() && json_rsp["rcode"].asInt() == 0)
        {
            return {"semantic", CaseStatus::FAIL, "非法请求被错误处理为 rcode=0"};
        }
        return {"semantic", CaseStatus::PASS, "非法请求被正确拒绝"};
    }

    void printCaseResult(const CaseResult &res)
    {
        const char *status = (res.status == CaseStatus::PASS ? "PASS" :
                             (res.status == CaseStatus::FAIL ? "FAIL" : "SKIP"));
        std::cout << "[" << status << "] " << res.name << " - " << res.detail << std::endl;
    }
}

int main(int argc, char *argv[])
{
    std::map<std::string, std::function<CaseResult()>> cases;
    cases["malformed"] = caseMalformed;
    cases["connect"] = caseConnect;
    cases["sync"] = caseSync;
    cases["semantic"] = caseSemantic;

    std::vector<std::string> selected;
    if (argc == 3 && std::string(argv[1]) == "--case")
    {
        selected.push_back(argv[2]);
    }
    else
    {
        selected.push_back("malformed");
        selected.push_back("connect");
        selected.push_back("sync");
        selected.push_back("semantic");
    }

    int pass = 0;
    int fail = 0;
    int skip = 0;

    for (auto &name : selected)
    {
        auto it = cases.find(name);
        if (it == cases.end())
        {
            std::cerr << "[FAIL] 未知 case: " << name << std::endl;
            return 1;
        }

        CaseResult res = it->second();
        printCaseResult(res);
        if (res.status == CaseStatus::PASS)
        {
            pass++;
        }
        else if (res.status == CaseStatus::FAIL)
        {
            fail++;
        }
        else
        {
            skip++;
        }
    }

    std::cout << "[SUMMARY] pass=" << pass
              << " fail=" << fail
              << " skip=" << skip << std::endl;

    return fail == 0 ? 0 : 1;
}
