# ESPmDNS Query Result Cleanup Validation

Exercises the real `MDNSResponder` through its public query and cleanup methods.
The test's `build_opt.h` renames `mdns_query_ptr`, `mdns_query_results_free`, and
`mdns_free` so the test can supply deterministic query results and track cleanup
without starting the SDK mDNS service or requiring network access.

Each successful fake query allocates a linked list of two result nodes. The
cleanup substitute tracks freed lists and detects repeated or invalid frees.

| Test Function | Description |
|---|---|
| `test_mdns_end_without_query` | Repeated shutdown and destruction without a query do not free results |
| `test_mdns_end_releases_query_results` | Shutdown frees both nodes and clears cached results |
| `test_mdns_repeated_end_and_destruction` | Two shutdown calls followed by destruction free the list exactly once |
| `test_mdns_repeated_responder_lifetimes` | Sixteen responder lifetimes leave no outstanding result nodes |
| `test_mdns_query_replaces_previous_results` | A new query frees the previous list; destruction frees the final list |
| `test_mdns_empty_query_clears_previous_results` | An empty query frees old results without a later double free |

Requires an SDK configuration with `CONFIG_MDNS_MAX_INTERFACES` enabled. These
tests cover wrapper ownership; they do not exercise the SDK service shutdown or
live mDNS discovery.
