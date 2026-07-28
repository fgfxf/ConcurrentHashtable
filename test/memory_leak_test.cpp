#include "Hashtable.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <malloc.h>

// 并发哈希表内存泄漏测试
// 测试思路：
//   1. 大量 put / remove 循环，验证节点是否正确释放
//   2. 多线程并发 put / remove，验证并发场景下无泄漏
//   3. 测试 rehash（超过 threshold 触发扩容）是否泄漏
//   4. 测试 clear() 后再使用是否泄漏
//   5. 测试带过期时间的哈希表定时清理是否泄漏
//
// 使用 valgrind 运行可检测内存泄漏：
//   valgrind --leak-check=full ./memory_leak_test

static const int THREAD_COUNT = 16;
static const int OPS_PER_THREAD = 500000;

// 全局哈希表
static dt::Hashtable<unsigned long, unsigned long> g_table(1024, 0.75f);

// 工作线程：put 后立即 remove，循环执行
static void *worker(void *arg)
{
    unsigned long tid = (unsigned long)(arg);

    for (int i = 0; i < OPS_PER_THREAD; ++i) {
        unsigned long key = tid * OPS_PER_THREAD + i;
        unsigned long val = key * 2;

        g_table.put(key, val);
        g_table.remove(key);
    }

    return NULL;
}

// 工作线程2：put 后不 remove，留给主线程清理
static void *worker2(void *arg)
{
    unsigned long tid = (unsigned long)(arg);

    for (int i = 0; i < OPS_PER_THREAD; ++i) {
        unsigned long key = tid * OPS_PER_THREAD + i;
        unsigned long val = key * 2;
        g_table.put(key, val);
    }

    return NULL;
}

// 过期回调
static void onExpired(unsigned long &key)
{
    (void)key;
}

int main()
{
    printf("========== 并发哈希表内存泄漏测试 ==========\n");

    // -------------------------------------------------------
    // 测试1：大量 put/remove 循环（节点应被正确释放）
    // -------------------------------------------------------
    printf("\n[测试1] 大量 put/remove 循环 (%d 线程 x %d 次)\n",
           THREAD_COUNT, OPS_PER_THREAD);

    pthread_t threads[THREAD_COUNT];
    for (int i = 0; i < THREAD_COUNT; ++i) {
        pthread_create(&threads[i], NULL, worker, (void *)(unsigned long)i);
    }
    for (int i = 0; i < THREAD_COUNT; ++i) {
        pthread_join(threads[i], NULL);
    }
    malloc_trim(0);
    printf("  完成, 表大小: %zu (应为 0)\n", g_table.size());
    
    // -------------------------------------------------------
    // 测试2：触发 rehash（大量 put 超过 threshold）
    // -------------------------------------------------------
    printf("\n[测试2] 触发 rehash 扩容\n");

    for (int i = 0; i < THREAD_COUNT; ++i) {
        pthread_create(&threads[i], NULL, worker2, (void *)(unsigned long)i);
    }
    for (int i = 0; i < THREAD_COUNT; ++i) {
        pthread_join(threads[i], NULL);
    }
    printf("  写入 %d 项后表大小: %zu\n",
           THREAD_COUNT * OPS_PER_THREAD, g_table.size());

    // 手动 remove 所有元素
    printf("  逐个 remove 所有元素...\n");
    for (unsigned long i = 0; i < (unsigned long)THREAD_COUNT * OPS_PER_THREAD; ++i) {
        g_table.remove(i);
    }

    printf("  remove 完成后表大小: %zu (应为 0)\n", g_table.size());
    malloc_trim(0);
    // -------------------------------------------------------
    // 测试3：clear() 后重新使用
    // -------------------------------------------------------
    printf("\n[测试3] clear() 后重新使用\n");
    int mainInput = 100000;
    for (int i = 0; i < mainInput; ++i) {
        g_table.put(i, i * 10);
    }
    printf("  写入 %d 项后大小: %zu\n",mainInput, g_table.size());

    g_table.clear();
    printf("  clear() 后大小: %zu (应为 0)\n", g_table.size());

    // 重新写入验证可用性
    for (int i = 0; i < 500; i++) {
        g_table.put(i, i * 20);
    }
    printf("  重新写入 500 项后大小: %zu\n", g_table.size());

    // -------------------------------------------------------
    // 测试3b：reverse() 缩容
    // -------------------------------------------------------
    printf("\n[测试3b] reverse() 缩容\n");
    {
        dt::Hashtable<unsigned long, unsigned long> rTable(1024, 0.75f);
        // 写入大量数据触发扩容
        for (unsigned long i = 0; i < 100000; ++i) {
            rTable.put(i, i * 2);
        }
        printf("  写入 100000 项后大小: %zu\n", rTable.size());

        // 删除大部分元素，模拟内存占用过高场景
        for (unsigned long i = 0; i < 99000; ++i) {
            rTable.remove(i);
        }
        printf("  删除 99000 项后大小: %zu (剩余 1000)\n", rTable.size());

        // 调用 shrink_to_fix() 缩容
        rTable.shrink_to_fix();
        printf("  shrink_to_fix() 缩容后大小: %zu\n", rTable.size());

        // 验证缩容后数据仍正确
        bool ok = true;
        for (unsigned long i = 99000; i < 100000; ++i) {
            unsigned long val = 0;
            if (!rTable.get(i, val) || val != i * 2) {
                ok = false;
                break;
            }
        }
        printf("  缩容后数据完整性: %s\n", ok ? "通过" : "失败");

        // 验证缩容后仍可正常写入
        for (unsigned long i = 0; i < 500; ++i) {
            rTable.put(i, i * 3);
        }
        printf("  缩容后重新写入 500 项, 大小: %zu\n", rTable.size());
    } // rTable 析构，验证无泄漏
    
    // -------------------------------------------------------
    // 测试4：带过期时间的哈希表
    // -------------------------------------------------------
    printf("\n[测试4] 带过期时间的哈希表 (周期 2 秒)\n");
    fflush(stdout);

    dt::Timer::getInstance();
    {
        dt::Hashtable<unsigned long, unsigned long> expiredTable(100, 0.75f, 2, onExpired);

        for (unsigned long i = 1; i <= 100; ++i) {
            expiredTable.put(i, i * 100);
        }
        printf("  写入 100 项, 大小: %zu\n", expiredTable.size());
        fflush(stdout);

        // 等待过期清理触发
        printf("  等待 5 秒触发过期清理...\n");
        fflush(stdout);
        struct timespec req = {5, 0};
        while (req.tv_sec > 0 || req.tv_nsec > 0) {
            if (nanosleep(&req, &req) == 0) {
                break;
            }
        }
        printf("  过期清理后大小: %zu\n", expiredTable.size());
        fflush(stdout);
    } // expiredTable 析构，验证析构无泄漏
    malloc_trim(0);
    printf("\n========== 测试结束 ==========\n");
    printf("建议使用 valgrind 检测内存泄漏:\n");
    printf("  valgrind --leak-check=full ./memory_leak_test\n");

    return 0;
}
