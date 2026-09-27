#ifndef IMP_TIMER_H_
#define IMP_TIMER_H_

#include"Define.h"

class ImpTimer
{
public:
    ImpTimer();
    ~ImpTimer();

    void start();

    int get_ticks();

private:
    int start_tick_;

    bool is_started_;
};

#endif
