// SPDX-FileCopyrightText: 2025 Espressif Systems (Shanghai) CO LTD
// SPDX-License-Identifier: Apache-2.0

/*
 * GCC gcov serial dump for embedded targets (ESP32).
 *
 * Problem: GCC's --coverage instrumentation expects a POSIX filesystem to
 * persist .gcda profiling data when the program exits.  ESP32 targets running
 * in Wokwi or on real hardware have no such filesystem, and no normal
 * "program exit" to trigger the dump.
 *
 * Solution: We use the GCC freestanding gcov API (__gcov_info_to_gcda from
 * <gcov.h>) to serialise each translation unit's coverage data into binary
 * .gcda format through callbacks -- zero file I/O, no fopen() interception.
 *
 * At startup the normal --coverage constructors register every TU's
 * gcov_info into the libgcov linked list rooted at __gcov_root.  At dump
 * time we walk that list and call __gcov_info_to_gcda() for each entry.
 * The filename callback emits a serial marker and the dump callback
 * base64-encodes the binary data over the ROM UART.
 *
 * A host-side script (coverage_collector.py) parses the serial output,
 * reconstructs the .gcda files, and produces an lcov report.
 *
 * ROM UART output (esp_rom_output_to_channels) is used instead of printf/
 * Serial to guarantee output even if higher-level I/O is unavailable.
 *
 * The entire mechanism is compiled in only when COVERAGE_ENABLED is defined
 * AND CORE_DEBUG_LEVEL == 0, so that no debug log messages can corrupt the
 * base64 data stream on the serial channel.
 *
 * Build flags required (injected by tests_build.sh --coverage):
 *   compiler.c.extra_flags   = --coverage -DCOVERAGE_ENABLED
 *   compiler.cpp.extra_flags = --coverage -DCOVERAGE_ENABLED
 *   compiler.libraries.ldflags = -lgcov
 *
 * Serial protocol:
 *   <<<GCOV_DUMP_START>>>        -- marks the beginning of a dump session
 *   <<<GCOV_FILE:/path/to.gcda>>>-- header for each .gcda file
 *   <base64 data lines>          -- 192-byte input chunks -> 256-char lines
 *   <<<GCOV_END>>>               -- footer for each .gcda file
 *   <<<GCOV_DUMP_END>>>          -- marks the end of the dump session
 */

#include "sdkconfig.h"

#if defined(COVERAGE_ENABLED) && CORE_DEBUG_LEVEL == 0

#include <gcov.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include "esp_rom_sys.h"
#include "libb64/cencode.h"

/*
 * libgcov internal: linked-list root of all gcov_info objects registered by
 * the --coverage constructors.  The first field is a pointer to the head of
 * the gcov_info chain.
 *
 * Layout of struct gcov_root (stable since GCC 10):
 *   void *list;            -- head of gcov_info linked list
 *   unsigned dumped : 1;
 *   unsigned run_counted : 1;
 *   void *prev, *next;    -- root chain (unused here)
 */
extern char __gcov_root[];

/*
 * Advance to the next gcov_info in the chain.
 *
 * struct gcov_info layout (first two fields, stable since GCC 4.7):
 *   gcov_unsigned_t version;  -- offset 0
 *   struct gcov_info *next;   -- offset sizeof(uint32_t)
 */
static const struct gcov_info *gcov_info_next(const struct gcov_info *info) {
  const struct gcov_info *const *pnext =
      (const struct gcov_info *const *)((const char *)info + sizeof(uint32_t));
  return *pnext;
}

/* --- Low-level ROM UART helpers (bypass all stdio) --- */

static void rom_putc(char c) {
  esp_rom_output_to_channels(c);
}

static void rom_puts(const char *s) {
  while (*s) {
    rom_putc(*s++);
  }
}

/*
 * Base64-encode and emit binary data in fixed-size chunks.
 * 192 bytes in -> 256 base64 chars out per chunk; the input size being a
 * multiple of 3 avoids mid-stream padding characters ('=').
 */
#define B64_CHUNK_IN  192
#define B64_CHUNK_OUT (B64_CHUNK_IN * 4 / 3 + 4)

static void rom_write_base64(const uint8_t *data, size_t len) {
  char buf[B64_CHUNK_OUT + 1];
  const char *src = (const char *)data;
  size_t remaining = len;

  while (remaining > 0) {
    int chunk = (remaining > B64_CHUNK_IN) ? B64_CHUNK_IN : (int)remaining;
    int enc_len = base64_encode_chars(src, chunk, buf);
    for (int i = 0; i < enc_len; i++) {
      rom_putc(buf[i]);
    }
    rom_putc('\n');
    src += chunk;
    remaining -= chunk;
  }
}

/* --- __gcov_info_to_gcda() callbacks --- */

static void gcov_filename_cb(const char *filename, void *arg) {
  (void)arg;
  rom_puts("<<<GCOV_FILE:");
  rom_puts(filename ? filename : "(null)");
  rom_puts(">>>\n");
}

static void gcov_dump_cb(const void *data, unsigned len, void *arg) {
  (void)arg;
  rom_write_base64((const uint8_t *)data, (size_t)len);
}

static void *gcov_allocate_cb(unsigned len, void *arg) {
  (void)arg;
  return malloc(len);
}

/*
 * Public entry point -- called from test sketches after UNITY_END().
 * Walks the constructor-registered gcov_info linked list and serialises
 * each entry via __gcov_info_to_gcda().  Brackets the dump session with
 * START/END markers so the host can reliably delimit gcov output from
 * other serial traffic.
 */
void gcov_dump_serial(void) {
  const struct gcov_info *info;

  /* __gcov_root is declared as char[] so we can read its first pointer-sized
     field (the list head) without including the internal struct definition. */
  const struct gcov_info *const *plist =
      (const struct gcov_info *const *)(void *)__gcov_root;
  info = *plist;

  rom_puts("\n<<<GCOV_DUMP_START>>>\n");
  while (info) {
    __gcov_info_to_gcda(info, gcov_filename_cb, gcov_dump_cb,
                        gcov_allocate_cb, NULL);
    rom_puts("<<<GCOV_END>>>\n");
    info = gcov_info_next(info);
  }
  rom_puts("<<<GCOV_DUMP_END>>>\n");
}

#else /* !COVERAGE_ENABLED || CORE_DEBUG_LEVEL != 0 */

/* No-op stub so test sketches can call gcov_dump_serial() unconditionally */
void gcov_dump_serial(void) {}

#endif
