# DNS server packet bounds validation

Sends malformed DNS questions to `DNSServer` over UDP loopback and checks that
the server drops them. The cases include a missing QName terminator, a zero byte
only in QType, and a truncated QClass. A valid root question then confirms that
the server remains responsive.

No Wi-Fi connection is required. The test is skipped when LwIP loopback support
is disabled.
