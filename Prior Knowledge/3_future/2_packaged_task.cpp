#include <iostream>
#include <future>
#include <memory>
#include <thread>

int add(int a, int b)
{
    std::cout << "加法函数被调用！" << std::endl;
    return a + b;
}

// std::packaged_task 的核心功能：把一个普通函数（同步任务）变成异步任务，即把函数的执行结果包装成 future 可取的任务。
// std::packaged_task<返回类型(参数类型...)> task(可调用对象);
// 通过 task.get_future() 获取关联的 future。
// 在新线程或异步环境中执行 task(参数...)。
// 在主线程用 future.get() 拿到结果。

int main()
{
    // 1. 封装任务

    // std::shared_ptr<std::packaged_task<int(int, int)>> task = std::make_shared<std::packaged_task<int(int, int)>>(add);
    auto task = std::make_shared<std::packaged_task<int(int, int)>>(add);

    // 2. 获取关联的 future
    std::future<int> res = task->get_future();

    // 3. 启动一个线程执行任务（异步）
    std::thread thr([task]()
                    { (*task)(11, 22); }); // 调用 Add(11,22)，并自动把返回值存入future

    // 4. 在主线程中异步获取结果
    std::cout << res.get() << std::endl;

    thr.join(); // 注意：显式处理线程的执行状态，避免资源泄露

    return 0;
}