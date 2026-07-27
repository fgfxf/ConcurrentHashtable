# 并发哈希表（ConcurrentHashtable）

ConcurrentHashtable 是一个在 Linux 平台下测试通过的哈希表，提供以下特性：

1. 这是一个 C++ 哈希表，可在多线程模式下工作。
2. 哈希表中的每个元素都带有时间戳，支持过期机制。同时支持通过回调函数处理过期元素。
3. 支持C/C++98 标准，我们公司在2026年的今天还不让用c++11，大无语。

该项目已在 C98 标准和 Linux 下的 g++ 4.8 编译器中测试通过。

编译步骤：
1.1 cmake .
1.2 make

使用前需要在服务器上安装 CMake。
## 使用案例
```c++
//创建
dt::Hashtable<unsigned long, unsigned long> g_table(1024, 0.75f);
```
创建1024个元素的哈希表，负载因子为0.75。
注意：超过1024会重哈希然后增大空间，不是到1024就无法插入了。
```c++
//增加
g_table.put(key, val);
//删除
g_table.remove(key);
//修改
g_table.put(1000000UL, 222222UL);
// 注意，放入同一个key就会覆盖原有的值，和stl不一样（存在则插入失败）
//查找
unsigned long newValue = 0;
g_table.get(1000000UL, newValue);
```
带过期时间的哈希表
```c++
// 初始化 Timer 单例
dt::Timer::getInstance();
// 创建带过期机制的哈希表：每 2 秒检查一次过期 key
dt::Hashtable<unsigned long, unsigned long> expiredTable(100, 0.75f, 2, onExpired);
// 过期回调函数（用于带过期时间的哈希表）
static void onExpired(unsigned long &key)
{
    printf("[过期清理] key=%lu 已过期\n", key);
}
```



祝您使用愉快！

## 修复bug
- 修复了原版大量printf调试信息
- 修复原版过时回调问题
- 添加大量使用案例