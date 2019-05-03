#include "Schedule.h"

struct scheduled_fn_t
{
    scheduled_fn_t* mNext;
    std::function<void(void)> mFunc;
};

static scheduled_fn_t* sFirst = 0;
static scheduled_fn_t* sLast = 0;

static scheduled_fn_t* sFirstUnused = 0;
static scheduled_fn_t* sLastUnused = 0;

static int sCount = 0;

static scheduled_fn_t* get_fn()
{
    scheduled_fn_t* result = NULL;
    // try to get an item from unused items list
    if (sFirstUnused)
    {
        result = sFirstUnused;
        sFirstUnused = result->mNext;
        if (sFirstUnused == NULL)
        {
            sLastUnused = NULL;
        }
    }
    // if no unused items, and count not too high, allocate a new one
    else if (sCount != SCHEDULED_FN_MAX_COUNT)
    {
        result = new scheduled_fn_t;
        result->mNext = NULL;
        ++sCount;
    }
    return result;
}

static void recycle_fn(scheduled_fn_t* fn)
{
    if (!sLastUnused)
    {
        sFirstUnused = fn;
    }
    else
    {
        sLastUnused->mNext = fn;
    }
    fn->mNext = NULL;
    sLastUnused = fn;
}

bool schedule_function(std::function<void(void)> fn)
{
    scheduled_fn_t* item = get_fn();
    if (!item)
    {
        return false;
    }
    item->mFunc = fn;
    item->mNext = NULL;
    if (!sFirst)
    {
        sFirst = item;
    }
    else
    {
        sLast->mNext = item;
    }
    sLast = item;
    return true;
}

void run_scheduled_functions()
{
    scheduled_fn_t* rFirst = sFirst;
    sFirst = NULL;
    sLast  = NULL;
    while (rFirst)
    {
        scheduled_fn_t* item = rFirst;
        rFirst = item->mNext;
        item->mFunc();
        item->mFunc = std::function<void(void)>();
        recycle_fn(item);
    }
}
