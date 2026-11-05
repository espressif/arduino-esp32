# String concatenation validation

Checks aliased String appends across inline-storage and heap-buffer growth,
including interior pointers and explicit-length binary data. No external
components or network connection are required.

| Test | Coverage |
|---|---|
| `test_string_concat_cstr_inline_growth` | Appending the String's own C string across inline-to-heap growth |
| `test_string_concat_cstr_inline_substring` | Appending an interior C-string pointer across inline-to-heap growth |
| `test_string_concat_cstr_heap_growth` | Appending the entire heap-backed String while growing its buffer |
| `test_string_concat_buffer_heap_substring` | Appending an interior byte pointer with an explicit length while growing the heap buffer |
| `test_string_concat_cstr_reserved` | Reserved capacity, interior slices without growth, and small inline appends |
| `test_string_concat_self_overloads` | Existing String-object self-concatenation and C-string `+=` |
| `test_string_concat_external_and_empty` | External RAM/flash strings, empty appends, and null-source rejection |
| `test_string_concat_binary_alias` | Explicit-length aliased append preserves embedded NUL bytes |

The hardware allocator may extend heap allocations in place. To deterministically
exercise relocation, the same sketch can also be run on a host with an allocator
that moves existing String buffers on every successful reallocation.

```sh
bash .github/scripts/tests_build.sh -s string -t esp32
bash .github/scripts/tests_run.sh -s string -t esp32
```
