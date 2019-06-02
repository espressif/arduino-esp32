#ifndef ESP_SCHEDULE_H
#define ESP_SCHEDULE_H

// This API is stabilizing
// Function signatures may change, internal queue will remain FIFO.
//
// * Add the given lambda to a fifo list of lambdas, which is run when
//   - `loop` function returns,
//   - or `yield` is called,
//   - or `run_scheduled_functions` is called.
//
// * Use lambdas to pass arguments to a function, or call a class/static
//   member function.
//
// * Please ensure variables or instances used from inside lambda will exist
//   when lambda is later called
//
// * There is no mechanism for cancelling scheduled functions.
//
// * `yield` can be called from inside lambdas
//
// * Returns false if the number of scheduled functions exceeds
//   SCHEDULED_FN_MAX_COUNT.

#ifdef __cplusplus
#include <functional>
extern "C" {
#endif

#define SCHEDULED_FN_MAX_COUNT 32

typedef enum schedule_e_t
{
    SCHEDULE_FUNCTION_FROM_LOOP,
    SCHEDULE_FUNCTION_WITHOUT_YIELDELAYCALLS
} schedule_e;

#ifdef __cplusplus
}

// * Run the lambda only once next time
bool schedule_function(std::function<void(void)>&& fn,
                       schedule_e policy = SCHEDULE_FUNCTION_FROM_LOOP);
bool schedule_function(const std::function<void(void)>& fn,
                       schedule_e policy = SCHEDULE_FUNCTION_FROM_LOOP);

// * Run the lambda periodically about every <repeat_us> microseconds until
//   it returns false.
// * Note that it may be more than <repeat_us> microseconds between calls if
//   `yield` is not called frequently, and therefore should not be used for
//   timing critical operations.
bool schedule_function_us(std::function<bool(void)>&& fn,
                          uint32_t repeat_us,
                          schedule_e policy = SCHEDULE_FUNCTION_FROM_LOOP);
bool schedule_function_us(const std::function<bool(void)>& fn,
                          uint32_t repeat_us,
                          schedule_e policy = SCHEDULE_FUNCTION_FROM_LOOP);

extern "C" {
#endif /* __cplusplus */

// Run all scheduled functions.
// Use this function if your are not using `loop`, or `loop` does not return
// on a regular basis.

#ifndef __cplusplus
    void run_scheduled_functions(schedule_e policy);
#else
    void run_scheduled_functions(schedule_e policy = SCHEDULE_FUNCTION_FROM_LOOP);
}
#endif

#endif //ESP_SCHEDULE_H
