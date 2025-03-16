# 高性能异步日志模块

## 简介

---
本项目是一个core插件中的异步日志模块，采用三缓冲区机制和独立的后台线程处理日志写入。

### 核心特性

---
- 三缓冲区设计：主缓冲区（写入）、从缓冲区（交换）、备份缓冲区（文件写入）实现无锁高效写入。
- 线程安全：利用`atomic`和`memory_order`实现多生产者对同一缓冲区的无锁接口写入，使用共享锁和独占锁进行缓冲区交换。
- CPP新特性 线程屏障`latch`和`barrier`实现多生产者和消费者的同步，通过`source_location`自动记录源文件名、行号和函数名
- 异步后台线程：使用`epoll`和`eventfd`轻量级后台日志线程唤醒。


### 目录结构

---
``` text
logger
├── include
│   ├── log_buffer.h
│   ├── log_thread.h
│   ├── logger.h
│   └── log_common.h
├── src
│   ├── log_buffer.cpp
│   ├── log_thread.cpp
│   └── logger.cpp
├── test
│   ├── benchmark.cpp
│   └── test_logger.cpp
└── hnc_log.h
```

### 使用方法

```cpp
#include "hnc_log.h"
using namespace hnc::core::logger;

int main() {
    log_trace("apple");
    log_debug("orange");
    log_info("banana");
    log_warn("...");
    log_critical("xxx");
    log_error("...");
    log_fatal("yyy");
    return 0;
}

```
**可能的输出格式**
> [info] [2025-01-01 00:00:00][/home/xxx/xxx:xxx.cpp:00:void func()] apple

### 环境变量

---
通过环境变量 `HNC_LOG_LEVEL` 实现 字符串到日志等级的反射，宏实现

## 实现

---

### 全局接口

---
```cpp
#define _FUNCTION(name) inline void log_##name(const std::string& msg, std::source_location loc = std::source_location::current()) noexcept { details::log_message(Level::name, msg, loc); }
    _FOREACH_LOG_LEVEL(_FUNCTION)
#undef _FUNCTION
```


### Logger
> 提供外部调用接口。
> 自动记录日志位置（文件、函数、行号）信息。

### LogThread
> 后台线程类，负责将缓冲区日志异步写入文件。
> 提供主从缓冲区交换功能。





## 日志模块设计


### 缓冲区

#### 介绍

---
- 主缓冲区用于记录生产者线程产生的日志。
- 从缓冲区用于在主缓冲区满后快速切换，使生产者几乎无感知。
- 备份缓冲区用于切换从缓冲区，后台线程将日志内容异步写入磁盘。
- 多生产者持有前两块缓冲区，消费者持有 备份缓冲区

#### LogBuffer

---
> 使用共享锁避免生产者之间阻塞，
> 缓冲区使用 `atomic(memory_order)` 获取唯一日志元素块下标
> 缓存单元，用于高效地存储日志数据。
> 提供线程安全的缓冲区索引获取与日志写入。


### 后台日志线程

---
> 后台线程类，负责将缓冲区日志异步写入文件。
> 提供主从缓冲区交换功能。
> 
> 


## 后续问题
1. 实现从 配置文件 读取所有需要的配置信息
2. 目前每条日志长度受限， 实现 变长 缓冲区的 无锁接口
3. 单消费者， 实现多消费者模型
4. 使用 fmt库的 format 替代 C++20 的 std::format
5. 支持可变参，运行时 format
6. 将log单例修改为 支持多个实例，打印到不同文件
7. ...

```text
std::barrier 的 arrive_and_drop 是永久减少一个数， 死锁。。。
std::latch 只能用一次。。。
多个竞争独占锁的线程，只有一个会成为切换主从缓冲区的线程
避免 递归调用自己导致调用栈过深。。。。 8KB
......
```


