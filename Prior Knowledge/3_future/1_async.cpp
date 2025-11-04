#include <iostream>
#include <future>
#include <chrono>
#include <thread>

int add(int a,int b)
{
    std::cout << "加法函数被调用！" << std::endl;
    return a + b;
}

// std::async 用来异步执行一个函数任务，返回一个 std::future<T> 对象，T 是函数返回类型
// 简单理解就是异步版的函数调用，异步执行一个函数，返回对象用于获取函数的结果

int main()
{
    // 1. deferred 策略：函数不会立即执行，只有在 get() 时才执行
    // 参数：执行策略（同步/异步），要执行的函数，传递给函数的参数
    std::future<int> res1 = std::async(std::launch::deferred, add, 11, 22);
    // 2. async 策略：函数会立即在新线程中异步执行
    std::future<int> res2 = std::async(std::launch::async, add, 33, 44);

    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "-------------------" << std::endl;
    std::cout << res1.get() << std::endl;
    std::cout << res2.get() << std::endl;

    return 0;
}