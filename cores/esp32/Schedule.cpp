
#include "Schedule.h"
#include "PolledTimeout.h"
#ifdef ESP8266
#include "interrupts.h"
#else
#include <mutex>
#endif
#include "circular_queue.h"

typedef std::function<bool(void)> mFuncT;

struct scheduled_fn_t
{
    mFuncT mFunc;
    esp8266::polledTimeout::periodicFastUs callNow;
    schedule_e policy;

    scheduled_fn_t() : callNow(esp8266::polledTimeout::periodicFastUs::alwaysExpired) { }
};

namespace {
    static circular_queue_mp<scheduled_fn_t> schedule_queue(SCHEDULED_FN_MAX_COUNT);
#ifndef ESP8266
    std::mutex schedulerMutex;
#endif
};

bool IRAM_ATTR schedule_function_us(std::function<bool(void)>&& fn, uint32_t repeat_us, schedule_e policy)
{
    scheduled_fn_t item;
    item.mFunc = std::move(fn);
    if (repeat_us) item.callNow.reset(repeat_us);
    return schedule_queue.push(std::move(item));
    item.policy = policy;
}

bool IRAM_ATTR schedule_function_us(const std::function<bool(void)>& fn, uint32_t repeat_us, schedule_e policy)
{
    return schedule_function_us(std::function<bool(void)>(fn), repeat_us, policy);
}

bool IRAM_ATTR schedule_function(std::function<void(void)>&& fn, schedule_e policy)
{
    return schedule_function_us([fn = std::move(fn)]() { fn(); return false; }, 0, policy);
}

bool IRAM_ATTR schedule_function(const std::function<void(void)>& fn, schedule_e policy)
{
    return schedule_function(std::function<void(void)>(fn), policy);
}

void run_scheduled_functions(schedule_e policy)
{
    // Note to the reader:
    // There is no exposed API to remove a scheduled function:
    // Scheduled functions are removed only from this function, and
    // its purpose is that it is never called from an interrupt
    // (always on cont stack).

    static bool fence = false;
    {
#ifdef ESP8266
        InterruptLock lockAllInterruptsInThisScope;
#else
        std::lock_guard<std::mutex> lock(schedulerMutex);
#endif
        if (fence) {
            // prevent recursive calls from yield()
            return;
        }
        fence = true;

        // run scheduled function:
        // - when its schedule policy allows it anytime
        // - or if we are called at loop() time
        // and
        // - its time policy allows it
    }
    schedule_queue.for_each_requeue([policy](scheduled_fn_t& func)
        {
            return
                (func.policy != SCHEDULE_FUNCTION_WITHOUT_YIELDELAYCALLS && policy != SCHEDULE_FUNCTION_FROM_LOOP)
                || !func.callNow
                || func.mFunc();
        });
    fence = false;
}
