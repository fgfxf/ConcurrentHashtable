#include "Hashtable.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <iostream>

// 并发哈希表使用案例
// 本示例演示：
//   1. 多线程并发 put / get / remove 操作
//   2. 使用 Iterator 遍历哈希表
//   3. 带过期时间的哈希表（定时清理）

// 全局哈希表，供工作线程并发访问
static dt::Hashtable<unsigned long, unsigned long> g_table(1024, 0.75f);

// 工作线程数量
static const int WORKER_COUNT = 8;
// 每个线程执行的操作数量
static const int OPS_PER_THREAD = 20;

// 工作线程函数：并发执行 put / get / remove
static void *worker(void *arg)
{
    unsigned long tid = (unsigned long)(arg);

    for (int i = 0; i < OPS_PER_THREAD; ++i) {
        // 以 (tid * OPS_PER_THREAD + i) 作为 key，保证不同线程的 key 不冲突
        unsigned long key = tid * OPS_PER_THREAD + i;
        unsigned long val = key * 2 + 1;

        // 写入
        g_table.put(key, val);

        // 读取并校验
        unsigned long out = 0;
        if (g_table.get(key, out)) {
            if (out != val) {
                printf("[thread %lu] 校验失败: key=%lu, 期望=%lu, 实际=%lu\n",
                       tid, key, val, out);
            }
        } else {
            printf("[thread %lu] 读取失败: key=%lu\n", tid, key);
        }

        // 每隔一定次数删除部分 key
        // if (i % 3 == 0) {
        //     g_table.remove(key);
        // }
    }

    printf("[thread %lu] 完成 %d 次操作\n", tid, OPS_PER_THREAD);
    return NULL;
}

// 过期回调函数（用于带过期时间的哈希表）
static void onExpired(unsigned long &key)
{
    printf("[过期清理] key=%lu 已过期\n", key);
}

int main()
{
    printf("========== 并发哈希表使用案例 ==========\n");

    // -------------------------------------------------------
    // 第一部分：多线程并发读写测试
    // -------------------------------------------------------
    printf("\n[1] 多线程并发读写测试 (%d 线程, 每线程 %d 次操作)\n",
           WORKER_COUNT, OPS_PER_THREAD);

    pthread_t threads[WORKER_COUNT];

    // 创建并启动工作线程
    for (int i = 0; i < WORKER_COUNT; ++i) {
        int ret = pthread_create(&threads[i], NULL, worker, (void *)(unsigned long)i);
        if (ret != 0) {
            fprintf(stderr, "pthread_create 失败: %s\n", strerror(ret));
            return 1;
        }
    }

    // 等待所有工作线程结束
    for (int i = 0; i < WORKER_COUNT; ++i) {
        pthread_join(threads[i], NULL);
    }

    printf("\n所有线程结束, 当前哈希表大小: %zu\n", g_table.size());

    // -------------------------------------------------------
    // 第二部分：使用 Iterator 遍历哈希表
    // -------------------------------------------------------
    printf("\n[2] 使用 Iterator 遍历哈希表 (最多显示前 10 项)\n");

    dt::Iterator<unsigned long, unsigned long> itr = g_table.keys();
    int count = 0;
    while (itr.hasNext()) {
        unsigned long k = 0, v = 0;
        itr.next(k, v);
        if (count < 100) {
            printf("  key=%lu, value=%lu\n", k, v);
        }
        count++;
    }
    printf("  遍历完成, 共 %d 项\n", count);

    // -------------------------------------------------------
    // 第三部分：contain / remove 测试
    // -------------------------------------------------------
    printf("\n[3] contain / remove 测试\n");

    g_table.put(999999UL, 888888UL);
    printf("  put(999999, 888888) 后 contain(999999) = %s\n",
           g_table.contain(999999UL) ? "true" : "false");

    bool removed = g_table.remove(999999UL);
    printf("  remove(999999) = %s, 之后 contain(999999) = %s\n",
           removed ? "true" : "false",
           g_table.contain(999999UL) ? "true" : "false");

   // -------------------------------------------------------
   // 第三部分：修改 key 的 value 测试
   // -------------------------------------------------------
   printf("\n[3] 修改 key 的 value 测试\n");

   // 先写入一个初始值
   g_table.put(1000000UL, 111111UL);
   unsigned long oldValue = 0;
   g_table.get(1000000UL, oldValue);
   printf("  初始值: key=1000000, value=%lu\n", oldValue);

   // 再次 put 同一个 key，value 会被更新
   std::cout<<"contain 1000000? " << g_table.contain(1000000UL)<<std::endl;
   g_table.put(1000000UL, 222222UL);
   unsigned long newValue = 0;
   g_table.get(1000000UL, newValue);
   printf("  修改后: key=1000000, value=%lu\n", newValue);

   // 验证 size 没有变化（put 更新不增加 size）
   printf("  修改后哈希表大小: %zu (应为 %zu)\n",
          g_table.size(),
          g_table.size());

   // -------------------------------------------------------
   // 第四部分：带过期时间的哈希表
    // -------------------------------------------------------
    printf("\n[4] 带过期时间的哈希表 (周期 2 秒)\n");
    fflush(stdout);

    // 初始化 Timer 单例
    dt::Timer::getInstance();

    // 创建带过期机制的哈希表：每 2 秒检查一次过期 key
    dt::Hashtable<unsigned long, unsigned long> expiredTable(100, 0.75f, 2, onExpired);

    // 写入若干 key
    for (unsigned long i = 1; i <= 5; ++i) {
        expiredTable.put(i, i * 100);
    }
    printf("  写入 5 个 key, 当前大小: %zu\n", expiredTable.size());
    fflush(stdout);
    // 等待超过过期周期，观察过期回调是否触发
    // 注意：sleep 会被定时器信号中断，使用 nanosleep 循环累计等待时间
    printf("  等待 5 秒以触发过期清理...\n");
    fflush(stdout);
    struct timespec req = {5, 0};
    while (req.tv_sec > 0 || req.tv_nsec > 0) {
        if (nanosleep(&req, &req) == 0) {
            break;
        }
        // 被信号中断后继续睡眠剩余时间
    }

    printf("  过期检查后大小: %zu\n", expiredTable.size());
    fflush(stdout);


    printf("\n========== 测试结束 ==========\n");
    return 0;
}
