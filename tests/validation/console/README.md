# Console Library Validation Test

Validates the `Console` library in two phases: an interactive serial test (Phase 1) where pytest sends commands and verifies responses, followed by programmatic Unity tests (Phase 2) covering the full Console API.

## Test Cases

### Phase 1 — Interactive Serial (pytest-driven)

| Step | Description |
|---|---|
| `help` command | Verify registered commands appear in help output |
| `ping` command | Send `ping`, verify `pong` response |
| REPL task | Switch to REPL task via `test_repl`, verify it dispatches `ping` |
| `run_tests` | Trigger Phase 2 Unity tests via REPL task |

### Phase 2 — Unity Tests

| Test Function | Description |
|---|---|
| `test_begin_end` | `begin()` / `end()` lifecycle, double-end safety |
| `test_begin_twice` | Calling `begin()` twice succeeds |
| `test_add_remove_cmd` | Add command, run it, remove it, verify it no longer exists |
| `test_add_cmd_with_hint` | Add command with hint string, run with arguments |
| `test_add_cmd_with_context` | Add command with user context pointer, verify context is passed |
| `test_run_return_codes` | Verify return codes: 0 for success, 42 for custom, -1 for unknown |
| `test_run_before_begin` | `run()` before `begin()` returns -1 |
| `test_nested_run` | Command calls `Console.run()` internally (nested dispatch) |
| `test_help_cmd` | Add/remove built-in `help` command |
| `test_split_argv` | `splitArgv` parsing: simple tokens, quoted args, empty string |
| `test_plain_mode` | Enable/disable plain mode (no ANSI escape sequences) |
| `test_configuration` | Set prompt, max history, task stack/priority/core |
| `test_multiple_commands` | Register and run multiple commands in sequence |
| `test_string_overloads` | `addCmd` / `run` / `removeCmd` with Arduino `String` objects |
| `test_history_save_to_file` | Save linenoise history to LittleFS file, verify contents |
| `test_history_load_from_file` | Load pre-populated history file, add entry, verify merged output |
| `test_attach_to_serial` | Attach/detach REPL task to Serial, verify state transitions |

## Requirements

- **Hardware**: Currently disabled on hardware (`hardware: false` in ci.yml)
- **Wokwi/QEMU**: QEMU not supported; Wokwi supported

## Serial Protocol

1. DUT prints `REPL_READY`
2. Host sends `help` → verifies `ping` appears in output
3. Host sends `ping` → DUT replies `pong`
4. Host sends `test_repl` → DUT prints `REPL_TASK_STARTED`
5. Host sends `ping` through REPL task → DUT replies `pong`
6. Host sends `run_tests` → Unity test suite runs

## Notes

- Phase 1 uses a manual serial input loop (not the REPL task) to test end-to-end serial transport before switching to the REPL task.
- History persistence tests use LittleFS; the filesystem is pre-formatted at the start of Phase 2.
- The `hardware: false` flag in `ci.yml` indicates the test currently only runs on Wokwi due to pytest-embedded interaction issues on real hardware.
