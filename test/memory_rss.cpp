#include "Hashtable.h"
#include <stdio.h>
#include <malloc.h>
#include <sys/resource.h>
#include <unistd.h>
#include <fstream>
#include <string>

// 读取当前实时 RSS（VmRSS），能反映 shrink_to_fit 后内存的实时下降
// 注意：getrusage 的 ru_maxrss 是历史峰值，不会下降，无法体现缩容效果
static void printRSS(const char *label) {
    long vmrss = 0;
    std::ifstream f("/proc/self/status");
    std::string line;
    while (std::getline(f, line)) {
        if (line.substr(0, 6) == "VmRSS:") {
            sscanf(line.c_str(), "VmRSS: %ld kB", &vmrss);
            break;
        }
    }
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    printf("  [RSS] %-30s VmRSS=%ld KB, ru_maxrss(峰值)=%ld KB\n", label, vmrss, usage.ru_maxrss);
}

int main() {
    printRSS("初始");
    {
        dt::Hashtable<unsigned long, unsigned long> t(1024, 0.75f);
        for (unsigned long i = 0; i < 100000; ++i) t.put(i, i*2);
        printRSS("写入100000项后");
        for (unsigned long i = 0; i < 99000; ++i) t.remove(i);
        printRSS("删除99000项后");
        t.shrink_to_fit();
        printRSS("shrink_to_fit后");
        malloc_trim(0);
        printRSS("malloc_trim后");
    }
    printRSS("析构后");
    malloc_trim(0);
    printRSS("析构+malloc_trim后");
    return 0;
}