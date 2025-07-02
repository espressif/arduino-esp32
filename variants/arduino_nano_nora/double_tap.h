#ifndef __DOUBLE_TAP_H__
#define __DOUBLE_TAP_H__

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void double_tap_init(void);
void double_tap_mark(void);
void double_tap_invalidate(void);
bool double_tap_check_match(void);

#ifdef __cplusplus
}
#endif

#endif /* __DOUBLE_TAP_H__ */
