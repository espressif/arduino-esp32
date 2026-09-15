/*
 * HID Report descriptor parser — extracted from ESP-IDF components/esp_hid/src/esp_hid_common.c
 * SPDX-FileCopyrightText: 2017-2024 Espressif Systems (Shanghai) CO LTD
 * SPDX-License-Identifier: Apache-2.0
 *
 * Standalone: no esp_log, no Bluetooth. Optional debug: compile with -DUSBHID_REPORT_MAP_PARSE_DEBUG=1
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "USBHIDReportMapParse.h"

#if defined(USBHID_REPORT_MAP_PARSE_DEBUG) && USBHID_REPORT_MAP_PARSE_DEBUG
#define USBHID_PARSE_LOGE(fmt, ...) fprintf(stderr, "[usbhid_parse] " fmt "\n", ##__VA_ARGS__)
#else
#define USBHID_PARSE_LOGE(...) ((void)0)
#endif

typedef struct {
    uint16_t appearance;
    uint8_t usage_mask;
    uint8_t reports_len;
    usbhid_report_item_t reports[64];
} temp_hid_report_map_t;

typedef struct {
    uint8_t cmd;
    uint8_t len;
    union {
        uint32_t value;
        uint8_t data[4];
    };
} hid_report_cmd_t;

typedef struct {
    uint16_t usage_page;
    uint16_t usage;
    uint16_t inner_usage_page;
    uint16_t inner_usage;
    uint8_t report_id;
    uint16_t input_len;
    uint16_t output_len;
    uint16_t feature_len;
} hid_report_params_t;

typedef enum {
    PARSE_WAIT_USAGE_PAGE, PARSE_WAIT_USAGE, PARSE_WAIT_COLLECTION_APPLICATION, PARSE_WAIT_END_COLLECTION
} s_parse_step_t;

static s_parse_step_t s_parse_step = PARSE_WAIT_USAGE_PAGE;
static uint8_t s_collection_depth = 0;
static hid_report_params_t s_report_params = {0,};
static uint16_t s_report_size = 0;
static uint16_t s_report_count = 0;

static bool s_new_map = false;
static temp_hid_report_map_t *s_temp_hid_report_map;

static int add_report(temp_hid_report_map_t *map, usbhid_report_item_t *item)
{
    if (map->reports_len >= 64) {
        USBHID_PARSE_LOGE("reports overflow");
        return -1;
    }
    memcpy(&(map->reports[map->reports_len]), item, sizeof(usbhid_report_item_t));
    map->reports_len++;
    return 0;
}

static int handle_report(hid_report_params_t *report, bool first)
{
    if (s_temp_hid_report_map == NULL) {
        s_temp_hid_report_map = (temp_hid_report_map_t *)calloc(1, sizeof(temp_hid_report_map_t));
        if (s_temp_hid_report_map == NULL) {
            USBHID_PARSE_LOGE("malloc failed");
            return -1;
        }
    }
    temp_hid_report_map_t *map = s_temp_hid_report_map;
    if (first) {
        memset(map, 0, sizeof(temp_hid_report_map_t));
    }

    if (report->usage_page == USBHID_RD_USAGE_PAGE_GENERIC_DESKTOP && report->usage == USBHID_RD_USAGE_KEYBOARD) {
        //Keyboard
        map->usage_mask |= USBHID_USAGE_KEYBOARD;
        if (report->input_len > 0) {
            usbhid_report_item_t item = {
                .usage = USBHID_USAGE_KEYBOARD,
                .report_id = report->report_id,
                .report_type = USBHID_REPORT_TYPE_INPUT,
                .protocol_mode = USBHID_PROTOCOL_MODE_REPORT,
                .value_len = report->input_len / 8,
            };
            if (add_report(map, &item) != 0) {
                return -1;
            }

            item.protocol_mode = USBHID_PROTOCOL_MODE_BOOT;
            item.value_len = 8;
            if (add_report(map, &item) != 0) {
                return -1;
            }
        }
        if (report->output_len > 0) {
            usbhid_report_item_t item = {
                .usage = USBHID_USAGE_KEYBOARD,
                .report_id = report->report_id,
                .report_type = USBHID_REPORT_TYPE_OUTPUT,
                .protocol_mode = USBHID_PROTOCOL_MODE_REPORT,
                .value_len = report->output_len / 8,
            };
            if (add_report(map, &item) != 0) {
                return -1;
            }

            item.protocol_mode = USBHID_PROTOCOL_MODE_BOOT;
            item.value_len = 1;
            if (add_report(map, &item) != 0) {
                return -1;
            }
        }
    } else if (report->usage_page == USBHID_RD_USAGE_PAGE_GENERIC_DESKTOP && report->usage == USBHID_RD_USAGE_MOUSE) {
        //Mouse
        map->usage_mask |= USBHID_USAGE_MOUSE;
        if (report->input_len > 0) {
            usbhid_report_item_t item = {
                .usage = USBHID_USAGE_MOUSE,
                .report_id = report->report_id,
                .report_type = USBHID_REPORT_TYPE_INPUT,
                .protocol_mode = USBHID_PROTOCOL_MODE_REPORT,
                .value_len = report->input_len / 8,
            };
            if (add_report(map, &item) != 0) {
                return -1;
            }

            item.protocol_mode = USBHID_PROTOCOL_MODE_BOOT;
            item.value_len = 3;
            if (add_report(map, &item) != 0) {
                return -1;
            }
        }
    } else {
        usbhid_report_usage_t cusage = USBHID_USAGE_GENERIC;
        if (report->usage_page == USBHID_RD_USAGE_PAGE_GENERIC_DESKTOP) {
            if (report->usage == USBHID_RD_USAGE_JOYSTICK) {
                //Joystick
                map->usage_mask |= USBHID_USAGE_JOYSTICK;
                cusage = USBHID_USAGE_JOYSTICK;
            } else if (report->usage == USBHID_RD_USAGE_GAMEPAD) {
                //Gamepad
                map->usage_mask |= USBHID_USAGE_GAMEPAD;
                cusage = USBHID_USAGE_GAMEPAD;
            }
        } else if (report->usage_page == USBHID_RD_USAGE_PAGE_CONSUMER && report->usage == USBHID_RD_USAGE_CONSUMER_CONTROL) {
            //Consumer Control
            map->usage_mask |= USBHID_USAGE_CCONTROL;
            cusage = USBHID_USAGE_CCONTROL;
        } else if (report->usage_page >= 0xFF) {
            //Vendor
            map->usage_mask |= USBHID_USAGE_VENDOR;
            cusage = USBHID_USAGE_VENDOR;
        }
        //Generic
        usbhid_report_item_t item = {
            .usage = cusage,
            .report_id = report->report_id,
            .report_type = USBHID_REPORT_TYPE_INPUT,
            .protocol_mode = USBHID_PROTOCOL_MODE_REPORT,
            .value_len = report->input_len / 8,
        };
        if (report->input_len > 0) {
            if (add_report(map, &item) != 0) {
                return -1;
            }
        }
        if (report->output_len > 0) {
            item.report_type = USBHID_REPORT_TYPE_OUTPUT;
            item.value_len = report->output_len / 8;
            if (add_report(map, &item) != 0) {
                return -1;
            }
        }
        if (report->feature_len > 0) {
            item.report_type = USBHID_REPORT_TYPE_FEATURE;
            item.value_len = report->feature_len / 8;
            if (add_report(map, &item) != 0) {
                return -1;
            }
        }
    }
    return 0;
}

static int parse_cmd(const uint8_t *data, size_t len, size_t index, hid_report_cmd_t **out)
{
    if (index == len) {
        return 0;
    }
    hid_report_cmd_t *cmd = (hid_report_cmd_t *)malloc(sizeof(hid_report_cmd_t));
    if (cmd == NULL) {
        return -1;
    }
    const uint8_t *dp = data + index;
    cmd->cmd = *dp & 0xFC;
    cmd->len = *dp & 0x03;
    cmd->value = 0;
    if (cmd->len == 3) {
        cmd->len = 4;
    }
    if ((len - index - 1) < cmd->len) {
        USBHID_PARSE_LOGE("not enough bytes! cmd: 0x%02x, len: %u, index: %u", cmd->cmd, cmd->len, index);
        free(cmd);
        return -1;
    }
    memcpy(cmd->data, dp + 1, cmd->len);
    *out = cmd;
    return cmd->len + 1;
}

static int handle_cmd(hid_report_cmd_t *cmd)
{
    switch (s_parse_step) {
    case PARSE_WAIT_USAGE_PAGE: {
        if (cmd->cmd != USBHID_RM_USAGE_PAGE) {
            USBHID_PARSE_LOGE("expected USAGE_PAGE, but got 0x%02x", cmd->cmd);
            return -1;
        }
        s_report_size = 0;
        s_report_count = 0;
        memset(&s_report_params, 0, sizeof(hid_report_params_t));
        s_report_params.usage_page = cmd->value;
        s_parse_step = PARSE_WAIT_USAGE;
        break;
    }
    case PARSE_WAIT_USAGE: {
        if (cmd->cmd != USBHID_RM_USAGE) {
            USBHID_PARSE_LOGE("expected USAGE, but got 0x%02x", cmd->cmd);
            s_parse_step = PARSE_WAIT_USAGE_PAGE;
            return -1;
        }
        s_report_params.usage = cmd->value;
        s_parse_step = PARSE_WAIT_COLLECTION_APPLICATION;
        break;
    }
    case PARSE_WAIT_COLLECTION_APPLICATION: {
        if (cmd->cmd != USBHID_RM_COLLECTION) {
            USBHID_PARSE_LOGE("expected COLLECTION, but got 0x%02x", cmd->cmd);
            s_parse_step = PARSE_WAIT_USAGE_PAGE;
            return -1;
        }
        if (cmd->value != 1) {
            USBHID_PARSE_LOGE("expected APPLICATION, but got 0x%02x", cmd->value);
            s_parse_step = PARSE_WAIT_USAGE_PAGE;
            return -1;
        }
        s_report_params.report_id = 0;
        s_collection_depth = 1;
        s_parse_step = PARSE_WAIT_END_COLLECTION;
        break;
    }
    case PARSE_WAIT_END_COLLECTION: {
        if (cmd->cmd == USBHID_RM_REPORT_ID) {
            if (s_report_params.report_id && s_report_params.report_id != cmd->value) {
                //report id changed mid collection
                if (s_report_params.input_len & 0x7) {
                    USBHID_PARSE_LOGE("ERROR: INPUT report does not amount to full bytes! %d (%d)", s_report_params.input_len, s_report_params.input_len & 0x7);
                } else if (s_report_params.output_len & 0x7) {
                    USBHID_PARSE_LOGE("ERROR: OUTPUT report does not amount to full bytes! %d (%d)", s_report_params.output_len, s_report_params.output_len & 0x7);
                } else if (s_report_params.feature_len & 0x7) {
                    USBHID_PARSE_LOGE("ERROR: FEATURE report does not amount to full bytes! %d (%d)", s_report_params.feature_len, s_report_params.feature_len & 0x7);
                } else {
                    //SUCCESS!!!
                    int res = handle_report(&s_report_params, s_new_map);
                    if (res != 0) {
                        s_parse_step = PARSE_WAIT_USAGE_PAGE;
                        return -1;
                    }
                    s_new_map = false;

                    s_report_params.input_len = 0;
                    s_report_params.output_len = 0;
                    s_report_params.feature_len = 0;
                    s_report_params.usage = s_report_params.inner_usage;
                    s_report_params.usage_page = s_report_params.inner_usage_page;
                }
            }
            s_report_params.report_id = cmd->value;
        } else if (cmd->cmd == USBHID_RM_USAGE_PAGE) {
            s_report_params.inner_usage_page = cmd->value;
        } else if (cmd->cmd == USBHID_RM_USAGE) {
            s_report_params.inner_usage = cmd->value;
        } else if (cmd->cmd == USBHID_RM_REPORT_SIZE) {
            s_report_size = cmd->value;
        } else if (cmd->cmd == USBHID_RM_REPORT_COUNT) {
            s_report_count = cmd->value;
        } else if (cmd->cmd == USBHID_RM_INPUT) {
            s_report_params.input_len += (s_report_size * s_report_count);
        } else if (cmd->cmd == USBHID_RM_OUTPUT) {
            s_report_params.output_len += (s_report_size * s_report_count);
        } else if (cmd->cmd == USBHID_RM_FEATURE) {
            s_report_params.feature_len += (s_report_size * s_report_count);
        } else if (cmd->cmd == USBHID_RM_COLLECTION) {
            s_collection_depth += 1;
        } else if (cmd->cmd == USBHID_RM_END_COLLECTION) {
            s_collection_depth -= 1;
            if (s_collection_depth == 0) {
                if (s_report_params.input_len & 0x7) {
                    USBHID_PARSE_LOGE("ERROR: INPUT report does not amount to full bytes! %d (%d)", s_report_params.input_len, s_report_params.input_len & 0x7);
                } else if (s_report_params.output_len & 0x7) {
                    USBHID_PARSE_LOGE("ERROR: OUTPUT report does not amount to full bytes! %d (%d)", s_report_params.output_len, s_report_params.output_len & 0x7);
                } else if (s_report_params.feature_len & 0x7) {
                    USBHID_PARSE_LOGE("ERROR: FEATURE report does not amount to full bytes! %d (%d)", s_report_params.feature_len, s_report_params.feature_len & 0x7);
                } else {
                    //SUCCESS!!!
                    int res = handle_report(&s_report_params, s_new_map);
                    if (res != 0) {
                        s_parse_step = PARSE_WAIT_USAGE_PAGE;
                        return -1;
                    }
                    s_new_map = false;
                }
                s_parse_step = PARSE_WAIT_USAGE_PAGE;
            }
        }

        break;
    }
    default:
        s_parse_step = PARSE_WAIT_USAGE_PAGE;
        break;
    }
    return 0;
}

usbhid_report_map_t *usbhid_parse_report_map(const uint8_t *hid_rm, size_t hid_rm_len)
{
  s_parse_step = PARSE_WAIT_USAGE_PAGE;
  s_collection_depth = 0;
  memset(&s_report_params, 0, sizeof(s_report_params));
  s_report_size = 0;
  s_report_count = 0;
  if (s_temp_hid_report_map != NULL) {
    free(s_temp_hid_report_map);
    s_temp_hid_report_map = NULL;
  }

    size_t index = 0;
    int res;
    s_new_map = true;

    while (index < hid_rm_len) {
        hid_report_cmd_t *cmd;
        res = parse_cmd(hid_rm, hid_rm_len, index, &cmd);
        if (res < 0) {
            USBHID_PARSE_LOGE("Failed parsing the descriptor at index: %u", index);
            return NULL;
        }
        index += res;
        res = handle_cmd(cmd);
        free(cmd);
        if (res != 0) {
            return NULL;
        }
    }

    temp_hid_report_map_t *map = s_temp_hid_report_map;
    if (map == NULL) {
        return NULL;
    }

    usbhid_report_map_t *out = (usbhid_report_map_t *)calloc(1, sizeof(usbhid_report_map_t));
    if (out == NULL) {
        USBHID_PARSE_LOGE("hid_report_map malloc failed");
        free(s_temp_hid_report_map);
        s_temp_hid_report_map = NULL;
        return NULL;
    }

    usbhid_report_item_t *reports = (usbhid_report_item_t *)calloc(1, map->reports_len * sizeof(usbhid_report_item_t));
    if (reports == NULL) {
        USBHID_PARSE_LOGE("hid_report_items malloc failed! %u maps", map->reports_len);
        free(out);
        free(s_temp_hid_report_map);
        s_temp_hid_report_map = NULL;
        return NULL;
    }

    if (map->usage_mask & USBHID_USAGE_KEYBOARD) {
        out->usage = USBHID_USAGE_KEYBOARD;
        out->appearance = USBHID_APPEARANCE_KEYBOARD;
    } else if (map->usage_mask & USBHID_USAGE_MOUSE) {
        out->usage = USBHID_USAGE_MOUSE;
        out->appearance = USBHID_APPEARANCE_MOUSE;
    } else if (map->usage_mask & USBHID_USAGE_JOYSTICK) {
        out->usage = USBHID_USAGE_JOYSTICK;
        out->appearance = USBHID_APPEARANCE_JOYSTICK;
    } else if (map->usage_mask & USBHID_USAGE_GAMEPAD) {
        out->usage = USBHID_USAGE_GAMEPAD;
        out->appearance = USBHID_APPEARANCE_GAMEPAD;
    } else if (map->usage_mask & USBHID_USAGE_CCONTROL) {
        out->usage = USBHID_USAGE_CCONTROL;
        out->appearance = USBHID_APPEARANCE_KEYBOARD;
    } else {
        out->usage = USBHID_USAGE_GENERIC;
        out->appearance = USBHID_APPEARANCE_GENERIC;
    }
    out->reports_len = map->reports_len;
    memcpy(reports, map->reports, map->reports_len * sizeof(usbhid_report_item_t));
    out->reports = reports;
    free(s_temp_hid_report_map);
    s_temp_hid_report_map = NULL;

    return out;
}

void usbhid_free_report_map(usbhid_report_map_t *map)
{
    if (map != NULL) {
        free(map->reports);
        free(map);
    }
}

static const char *s_unknown_str = "UNKNOWN";
static const char *s_hid_protocol_names[] = {"BOOT", "REPORT"};
static const char *s_hid_report_type_names[] = {"NULL", "INPUT", "OUTPUT", "FEATURE"};

const char *usbhid_usage_str(usbhid_report_usage_t usage) {
  switch (usage) {
    case USBHID_USAGE_GENERIC: return "GENERIC";
    case USBHID_USAGE_KEYBOARD: return "KEYBOARD";
    case USBHID_USAGE_MOUSE: return "MOUSE";
    case USBHID_USAGE_JOYSTICK: return "JOYSTICK";
    case USBHID_USAGE_GAMEPAD: return "GAMEPAD";
    case USBHID_USAGE_CCONTROL: return "CCONTROL";
    case USBHID_USAGE_VENDOR: return "VENDOR";
    default: break;
  }
  return s_unknown_str;
}

const char *usbhid_protocol_mode_str(uint8_t protocol) {
  if (protocol >= (sizeof(s_hid_protocol_names) / sizeof(s_hid_protocol_names[0]))) {
    return s_unknown_str;
  }
  return s_hid_protocol_names[protocol];
}

const char *usbhid_report_type_str(uint8_t report_type) {
  if (report_type >= (sizeof(s_hid_report_type_names) / sizeof(s_hid_report_type_names[0]))) {
    return s_unknown_str;
  }
  return s_hid_report_type_names[report_type];
}
