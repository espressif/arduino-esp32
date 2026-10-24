# HTTPClient Stream Upload Validation

Validates `HTTPClient::sendRequest()` with a source stream that returns short reads
and a fake `NetworkClient` that can limit payload writes. The client captures the
request and supplies a fixed HTTP response. No sockets, Wi-Fi, certificates, or
external server are required.

| Test Function | Description |
|---|---|
| `test_http_stream_full_read` | Full reads upload the complete payload |
| `test_http_stream_short_read` | Short reads continue until the declared payload length is uploaded |
| `test_http_stream_short_read_short_write` | Retries send only the unsent portion of the current buffer after a short read |
| `test_http_stream_full_read_short_write` | A short write is retried after a full read |
| `test_http_stream_content_length_limit` | Short reads stop at the requested length and leave extra source data untouched |
| `test_http_stream_failed_retry` | A second short write returns `HTTPC_ERROR_SEND_PAYLOAD_FAILED` |

The success cases verify the HTTP status, exact payload bytes and length,
remaining source bytes, and outgoing `Content-Length` header.
