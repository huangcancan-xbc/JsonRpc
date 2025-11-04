#include <iostream>
#include <future>
#include <thread>

int add(int a, int b)
{
    std::cout << "加法函数被调用！" << std::endl;
    return a + b;
}

// promise 是一个装结果的盒子，主线程创建，future 是一个取结果的钥匙，主线程也拿着。
// 子线程计算完后调用 pro.set_value()，相当于往盒子里放值，主线程调用 res.get()，相当于打开盒子取出值。
// 如果子线程没放好（还没 set_value()），主线程的 get() 就会阻塞等待。

// get_future():获取与该 promise 绑定的 future 对象
// set_value(val):在某个时刻设置 promise 的值
// get():等待 promise 提供值后返回结果（阻塞）

int main()
{
    // 1. 在使用的时候，先实例化一个指定结果的promise对象
    std::promise<int> pro;

    // 2. 通过promise对象，获取关联的future对象
    std::future<int> res = pro.get_future();

    // 3. 在任意位置给promise设置数据，就可以通过关联的future获取到这个设置的数据了
    std::thread thr([&pro]()
                    { int sum = add(11, 22);
                    pro.set_value(sum); });

    // 4. 获取future对象的值，阻塞等待，直到值被设置
    std::cout << res.get() << std::endl;

    thr.join(); // 注意：显式处理线程的执行状态，避免资源泄露

    return 0;
}