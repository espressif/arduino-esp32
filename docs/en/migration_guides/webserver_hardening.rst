###########################
WebServer Request Hardening
###########################

Introduction
------------

The ``WebServer`` library was hardened against malformed and hostile requests. Requests that were previously parsed without any bound on
length, argument count or loop iterations are now rejected, and a few code paths that could dereference a null pointer or leak state between
requests were fixed.

This guide lists every change that is visible to a sketch, so an application that relied on the previous behavior can be adjusted. Requests
produced by browsers and by common HTTP clients are unaffected: every limit below is well above what normal traffic uses, and all of them can
be raised from the build.

New Request Limits
------------------

All limits are plain macros with ``#ifndef`` guards, so they can be overridden from the build without editing the library, for example with
``-DWEBSERVER_MAX_URI_LEN=8192`` in ``build_opt.h`` or in the platform build flags.

.. list-table::
    :header-rows: 1
    :widths: 34 12 54

    * - Macro
      - Default
      - Effect when exceeded
    * - ``WEBSERVER_MAX_URI_LEN``
      - 2048
      - The request is answered with ``414 URI Too Long`` and the connection is closed. Set to ``0`` to disable the check.
    * - ``WEBSERVER_MAX_LINE_LEN``
      - 4096
      - A single protocol line (request line, header, or multipart part header) is refused. See `Request Line and Headers`_.
    * - ``WEBSERVER_MAX_POST_ARG_LEN``
      - 16384
      - One line of a non-file multipart field value is refused and the request is aborted.
    * - ``WEBSERVER_MAX_LINE_WAIT``
      - 5000 ms
      - A protocol line that has not arrived complete in time is answered with ``408 Request Timeout``. Set to ``0`` to disable.
    * - ``WEBSERVER_MAX_HEADER_WAIT``
      - 10000 ms
      - The request line and headers together took too long; answered with ``408 Request Timeout``. Set to ``0`` to disable.
    * - ``WEBSERVER_MAX_QUERY_ARGS``
      - 256
      - Further arguments are dropped, but the request is still handled. See `Query and Form Arguments`_.
    * - ``WEBSERVER_MAX_MULTIPART_SKIP_LINES``
      - 64
      - The multipart body is treated as malformed and the upload is aborted. See `Multipart Bodies`_.
    * - ``WEBSERVER_MAX_REGEX_URI_LEN``
      - 2048
      - The request-target is not matched against ``UriRegex`` routes. See `Regular Expression Routes`_.
    * - ``WEBSERVER_MAX_BACKREF_REGEX_URI_LEN``
      - 64
      - Same, for patterns that contain a back-reference. See `Regular Expression Routes`_.

Three response codes that the library never produced before can now be sent: ``408 Request Timeout``, ``414 URI Too Long`` and
``431 Request Header Fields Too Large``.

Behavior Changes
----------------

Authentication
**************

``authenticate(username, password)`` now only accepts the ``Basic`` and ``Digest`` schemes. Previously any ``Authorization`` header whose
value matched the configured user name was accepted, including a bare ``Authorization: <username>`` with no scheme and no password.

Sketches that pass their own callback to ``authenticate()`` are unaffected: the callback still receives every scheme, including
``OTHER_AUTH``, and decides on its own.

serveStatic()
*************

.. vale off

A request path containing a ``.`` or ``..`` segment is refused, both in its literal and in its percent-encoded form, and the resolved path
must stay under the configured filesystem root. Applications that depended on ``..`` being resolved by the filesystem must now request the
canonical path.

.. vale on

A related bug was fixed at the same time: ``serveStatic("/", fs, "/")`` used to answer every request with ``404``. Mapping a handler to the
filesystem root now works.

Request Line and Headers
************************

The request line and every header are read with a bounded reader instead of ``Stream::readStringUntil()``. A peer that never sends a line
terminator can no longer grow the heap until the device runs out of memory.

* A request line longer than ``WEBSERVER_MAX_LINE_LEN`` is answered with ``414 URI Too Long``.
* A header line longer than ``WEBSERVER_MAX_LINE_LEN`` is answered with ``431 Request Header Fields Too Large``.

In both cases the already-received part of the request is drained before the connection is closed, so the status reaches the client instead of
being lost to a connection reset.

Reading is also bounded in time, which fixes `issue #12788 <https://github.com/espressif/arduino-esp32/issues/12788>`_. The stream timeout
alone only requires that *some* byte arrive every few seconds, so a client sending one byte at a time could keep ``handleClient()`` from
returning indefinitely and eventually trip the task watchdog. Two deadlines now apply, and both answer ``408 Request Timeout``:

* ``WEBSERVER_MAX_LINE_WAIT`` bounds how long one line may take to arrive complete.
* ``WEBSERVER_MAX_HEADER_WAIT`` bounds the request line and all headers together. This is the one that catches a client sending an endless
  stream of complete but tiny headers, since every completed line restarts the per-line deadline.

The header deadline deliberately does not cover the request body, so uploads that legitimately take minutes are unaffected. A client on a link
so slow that it cannot deliver its headers within ``WEBSERVER_MAX_HEADER_WAIT`` is now dropped, where previously it would be served.

Query and Form Arguments
************************

The argument array is sized from the number of ``key=value`` pairs the parser will actually accept, instead of from the number of ``&``
separators. This has two visible consequences:

* A body or query string made only of separators, such as ``&&&&``, now yields zero arguments. It previously yielded one empty argument per
  separator.
* Arguments past ``WEBSERVER_MAX_QUERY_ARGS`` are dropped with a warning in the log. The request is still routed and handled, and the
  arguments below the limit are still available.

Argument arrays are also allocated with ``std::nothrow``. When the allocation fails, ``args()`` reports ``0`` and ``arg()`` returns an empty
string, where the previous code aborted.

Multipart Bodies
****************

* A preamble or epilogue around the parts is skipped, as required by :rfc:`2046`, but only up to ``WEBSERVER_MAX_MULTIPART_SKIP_LINES``
  lines. Beyond that, and when no data arrives for ``HTTP_MAX_POST_WAIT``, the upload is aborted instead of looping.
* A body truncated right after the boundary aborts the upload instead of spinning the server task forever.
* Exceeding ``WEBSERVER_MAX_POST_ARGS`` now calls the upload callback with ``UPLOAD_FILE_ABORTED`` before failing. It previously failed
  without notifying the callback, so a handler could be left holding an open file.
* A non-file field value is read with the same bounded reader as the rest of the protocol, capped per line by ``WEBSERVER_MAX_POST_ARG_LEN``
  and by ``WEBSERVER_MAX_LINE_WAIT``. A field value line longer than the cap, or a client that stalls part-way through one, aborts the
  request. The cap is separate from ``WEBSERVER_MAX_LINE_LEN`` and much larger, because a long field value is legitimate.

Per-Request State
*****************

Upload state, raw-body state and collected form fields are reset at the start of every request. On a keep-alive connection, a field from an
earlier request can no longer be returned by ``arg()`` or ``hasArg()``, and an aborted multipart request can no longer leave fields visible to
the request that follows.

Sketches that read ``upload()`` or ``arg()`` outside the handler for the request they belong to will now see empty state.

upload() and raw()
******************

A handler registered with an upload callback also receives raw, non-multipart bodies. ``upload()`` and ``raw()`` could therefore be called
with no corresponding object allocated, dereferencing a null pointer.

Both now return a reference to an inactive object whose ``status`` is ``UPLOAD_FILE_ABORTED`` or ``RAW_ABORTED`` instead. Two accessors were
added to tell the two contexts apart:

.. code-block:: arduino

    server.on(
      "/data", HTTP_POST, []() { server.send(200); },
      []() {
        if (server.hasUpload()) {
          HTTPUpload &upload = server.upload();
          // multipart upload
        } else if (server.hasRaw()) {
          HTTPRaw &raw = server.raw();
          // raw body
        }
      }
    );

Regular Expression Routes
*************************

``UriRegex`` now compiles its pattern with the bounded matcher provided by the standard library instead of the default recursive one. The
recursive matcher consumed task stack in proportion to the length of the request-target, and took exponential time on patterns with nested
quantifiers, both of which an unauthenticated request could trigger. Measured on an ESP32:

.. list-table::
    :header-rows: 1
    :widths: 46 27 27

    * - Case
      - Recursive matcher
      - Bounded matcher
    * - Stack, 160-character target
      - 21.9 KB
      - 2.5 KB
    * - Stack, 2000-character target
      - Overflows the task stack
      - 2.5 KB
    * - ``^(a+)+$`` against 22 characters
      - 13.8 s
      - 1.4 ms
    * - Matching a 160-character target
      - 0.3 ms
      - 4.7 ms (about 26 us per character)

Stack use no longer depends on the length of the request-target, which is why ``WEBSERVER_MAX_REGEX_URI_LEN`` defaults to 2048 rather than to
a value derived from the stack of the task that calls ``handleClient()``.

The trade-offs of the bounded matcher are:

* **Back-references are not supported.** A pattern containing ``\1`` to ``\9`` keeps the recursive matcher, and with it the much smaller
  ``WEBSERVER_MAX_BACKREF_REGEX_URI_LEN`` limit. Route patterns essentially never use back-references, since a back-reference in a route
  means one path segment has to repeat another.
* **Matching is slower.** A short path costs roughly 0.6 ms instead of 0.1 ms, and a long one grows linearly at about 26 us per character. The
  cost is paid once per registered ``UriRegex`` route per request, so an application with many regex routes and long request-targets may want
  to lower ``WEBSERVER_MAX_REGEX_URI_LEN``.
* **Pending states are held on the heap** rather than on the stack, at roughly 3 bytes per character of request-target. A failure to allocate
  is reported as "route does not match", so the client receives a ``404`` instead of the device restarting.

Everything else behaves as before. Anchors, character classes, greedy and lazy quantifiers, counted repetition, alternation, capture groups,
lookahead, word boundaries and the ``icase`` flag were all compared between the two matchers and produce identical captures.

.. note::

    The bounded matcher is a libstdc++ extension. It is selected only when the standard library provides it, and the library falls back to the
    previous matcher otherwise, so no configuration is required.

A request-target longer than the applicable limit is not matched against regex routes, and an error is logged naming the limit that was
exceeded. Plain string routes registered for the same path are unaffected, since they are matched first.

New APIs
--------

* ``bool WebServer::hasUpload() const`` - true when a multipart upload is in progress.
* ``bool WebServer::hasRaw() const`` - true when a raw request body is in progress.

Restoring the Previous Limits
-----------------------------

Every limit can be raised, and the request-target check can be disabled entirely, from the build. For example, in ``build_opt.h``:

.. code-block:: none

    -DWEBSERVER_MAX_URI_LEN=0
    -DWEBSERVER_MAX_LINE_LEN=16384
    -DWEBSERVER_MAX_QUERY_ARGS=1024
    -DWEBSERVER_MAX_HEADER_WAIT=30000

.. warning::

    Raising these limits restores the memory-exhaustion behavior they were added to prevent. Prefer raising a single limit to the value the
    application actually needs.

The dot-segment rejection in ``serveStatic()``, the authentication scheme check and the per-request state reset are not configurable, because
each of them fixes a security issue rather than imposing a limit.
