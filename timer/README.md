# 定时器模块

## 介绍

内部使用了 hnc_thread_pool, 使用一个线程作为监听线程， 其余线程负责执行 定时器回调任务

### timer fd

---



### hnc_timer

---
封装了 timer_fd 的read函数， 并维持一个回调用以在定时器可读时执行


### hnc_timer_thread

---

1. 后台执行的 定时器 监听线程， 
2. 使用 `eventfd` 通知 该定时器线程退出
3. 主线程通过 `std::latch` 等待该线程退出后才能析构，因为子线程持有 `manager` 的指针

### hnc_timer_manager

---

> 待完善......