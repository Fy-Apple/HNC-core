# **线程池**

## **介绍**


支持 **固定线程数模式** 和 **动态线程数模式**。

---

### 高性能任务队列
- **原始实现**：`std::queue<std::function<void()>> + std::mutex`
- **后续饼**：修改，使用 **无锁环形队列（Lock-Free Ring Buffer）**，避免 `std::mutex` 的性能瓶颈，提高任务吞吐量

### ** 任务提交**
- **支持 `std::function<void()>` 类型任务**
- `submit_task()` 方法支持 **任意参数的任务提交**，返回 `std::future<T>` 以获取异步结果
- 任务提交时检查队列是否已满，避免无限制提交导致线程阻塞



## 使用方法

```c++
// 创建一个固定线程池
auto thread_pool = hnc::core::thread_pool::get_fixed_pool(4);
```

```c++
// 创建一个动态线程池，线程数根据任务负载调整
auto thread_pool = hnc::core::thread_pool::get_cached_pool(4);
```

```c++
// 提交一个无返回值的任务
threadPool.submit_task([] {
    std::cout << "Hello from thread pool!" << std::endl;
});
```

```c++
// 提交一个有返回值的任务
auto futureResult = threadPool.submit_task([](int a, int b) -> int {
    return a + b;
}, 3, 5);
```

```c++
int result = futureResult.get(); // 获取任务执行结果
std::cout << "Result: " << result << std::endl;
```

```c++
// 查看线程池状态
threadPool.print_status();
```


## TODO

---
1. 为任务添加优先级，使用优先级队列存储急迫任务 
2. 添加 无锁环形队列（Lock-Free Ring Buffer）提供普通任务 `folly::MPMCQueue`  `boost::lockfree::queue`
3. 预热线程池（避免任务突然增加时所有线程同时创建） 
4. 增加任务超时管理 , 允许任务设置超时时间，超时则取消任务
5. ......
