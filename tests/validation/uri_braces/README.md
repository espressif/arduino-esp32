# UriBraces route matching validation

Exercises `UriBraces::canHandle` directly, without a network connection or
external components. A placeholder must not capture `/`, whether it ends the
route or precedes a literal suffix.

| Test | Coverage |
|---|---|
| `test_uri_braces_suffixed_argument` | Valid filename capture before `.json` |
| `test_uri_braces_suffix_rejects_nested_paths` | Nested and double-slash paths rejected before a suffix |
| `test_uri_braces_multiple_arguments` | Valid captures in separate segments; nested final capture rejected |
| `test_uri_braces_arguments_in_one_segment` | Multiple captures before suffixes; slash rejected in either capture |
| `test_uri_braces_trailing_argument` | Existing trailing-placeholder behavior preserved |
| `test_uri_braces_rejects_mismatched_literals` | Wrong extension, prefix, missing suffix, and extra trailing path rejected |
| `test_uri_braces_empty_argument` | Existing empty-capture behavior preserved |
| `test_uri_braces_valid_after_rejection` | Valid capture succeeds after a rejected nested path |

```sh
bash .github/scripts/tests_build.sh -s uri_braces -t esp32
bash .github/scripts/tests_run.sh -s uri_braces -t esp32
```
