#include "Threads.h"
#include <vector>

std::vector<dt::TimerCall> vec;

void dt::Timer::timerHandler(int sig, siginfo_t *si, void *uc)
{
    timer_t *tidp = (timer_t *)si->si_value.sival_ptr;
    // printf("time handler is triggered\n");
    for ( std::vector<TimerCall >::iterator itr = vec.begin(); itr != vec.end(); ++itr) {
        if (*((*itr)._timer_t) == *tidp)  {
            // printf("execute command\n");
            (*itr).func((*itr).para);
        }
    }
}


timer_t dt::Timer::create(long expireMS, int intervalMS, void (* callbackFunc) (void *), void * para)
{
    //unsigned int id = getTimerId();
    // 动态分配 timer_t，使其地址在 create 返回后仍然有效（sival_ptr 需要指向持久内存）
    timer_t * id = new timer_t;
    
    make(id, expireMS, intervalMS);
    TimerCall call;
    call._timer_t = id;
    call.func = callbackFunc;
    call.para = para;
    vec.push_back(call);
    
    return *id;
}


void dt::Timer::remove(timer_t& timerID)
{
    size_t j = 0;
    bool found = false;
    for ( std::vector<TimerCall >::iterator itr = vec.begin(); itr != vec.end(); ++itr) {
        if (*((*itr)._timer_t) == timerID) {
            found = true;
            break;
        }
        j++;
    }
    
    if (found)
    {
        timer_t * id = vec[j]._timer_t;
        vec.erase(vec.begin() + j);
        int ret = timer_delete(timerID);
        // if (ret != 0)
            // printf ("delete timer failed , ret = %d", ret);
        delete id;
    }
}
