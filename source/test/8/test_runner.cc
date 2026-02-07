#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <cstring>
#include <chrono>
#include <functional>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

namespace
{
    class ProcessGuard
    {
    public:
        ProcessGuard(const std::string &name, const std::string &bin)
            : _name(name), _bin(bin), _pid(-1)
        {
        }

        bool start()
        {
            _pid = fork();
            if (_pid < 0)
            {
                std::cerr << "[FAIL] fork 失败: " << _name << std::endl;
                return false;
            }

            if (_pid == 0)
            {
                execl(_bin.c_str(), _bin.c_str(), (char *)nullptr);
                std::cerr << "[FAIL] exec 失败: " << _bin << " err=" << std::strerror(errno) << std::endl;
                _exit(127);
            }

            return true;
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

        ~ProcessGuard()
        {
            stop();
        }

    private:
        std::string _name;
        std::string _bin;
        pid_t _pid;
    };

    int runClientMode(const std::string &mode, int timeout_sec)
    {
        pid_t pid = fork();
        if (pid < 0)
        {
            std::cerr << "[FAIL] fork 失败: integration_client " << mode << std::endl;
            return -2;
        }

        if (pid == 0)
        {
            execl("./integration_client", "./integration_client", mode.c_str(), (char *)nullptr);
            std::cerr << "[FAIL] exec integration_client 失败, mode=" << mode
                      << " err=" << std::strerror(errno) << std::endl;
            _exit(127);
        }

        auto begin = std::chrono::steady_clock::now();
        int status = 0;
        while (true)
        {
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

    bool runCase(const std::string &name, const std::function<bool()> &fn)
    {
        std::cout << "[CASE] " << name << std::endl;
        bool ok = fn();
        std::cout << (ok ? "[PASS] " : "[FAIL] ") << name << std::endl;
        return ok;
    }
}

int main()
{
    int failed = 0;

    failed += runCase("场景1 直连RPC功能全量测试", []() {
        ProcessGuard server("rpc_server_direct", "./rpc_server_direct");
        if (!server.start())
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return runClientMode("direct", 60) == 0;
    }) ? 0 : 1;

    failed += runCase("场景2 注册中心+服务发现+RPC", []() {
        ProcessGuard registry("registry_server", "./registry_server");
        ProcessGuard server("rpc_server_registry", "./rpc_server_registry");
        if (!registry.start() || !server.start())
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        if (runClientMode("registry", 60) != 0)
        {
            return false;
        }
        return runClientMode("malformed", 60) == 0;
    }) ? 0 : 1;

    failed += runCase("场景3 Topic发布订阅全流程", []() {
        ProcessGuard topic_server("topic_server", "./topic_server");
        if (!topic_server.start())
        {
            return false;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return runClientMode("topic", 60) == 0;
    }) ? 0 : 1;

    failed += runCase("场景4 不可达注册中心超时恢复", []() {
        return runClientMode("timeout", 60) == 0;
    }) ? 0 : 1;

    if (failed > 0)
    {
        std::cerr << "[SUMMARY] 失败场景数: " << failed << std::endl;
        return 1;
    }

    std::cout << "[SUMMARY] 全部场景通过" << std::endl;
    return 0;
}
