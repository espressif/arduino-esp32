/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2015 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef ASIO_HTTP2_CLIENT_H
#define ASIO_HTTP2_CLIENT_H

#include <nghttp2/asio_http2.h>

namespace nghttp2 {

namespace asio_http2 {

namespace client {

class response_impl;

class response {
public:
  // Application must not call this directly.
  response();
  ~response();

  // Sets callback which is invoked when chunk of response body is
  // received.
  void on_data(data_cb cb) const;

  // Returns status code.
  int status_code() const;

  // Returns content-length.  -1 if it is unknown.
  int64_t content_length() const;

  // Returns the response header fields.  The pusedo header fields,
  // which start with colon (:), are exluced from this list.
  const header_map &header() const;

  // Application must not call this directly.
  response_impl &impl() const;

private:
  std::unique_ptr<response_impl> impl_;
};

class request;

using response_cb = std::function<void(const response &)>;
using request_cb = std::function<void(const request &)>;
using connect_cb =
    std::function<void(boost::asio::ip::tcp::resolver::iterator)>;

class request_impl;

class request {
public:
  // Application must not call this directly.
  request();
  ~request();

  // Sets callback which is invoked when response header is received.
  void on_response(response_cb cb) const;

  // Sets callback which is invoked when push request header is
  // received.
  void on_push(request_cb cb) const;

  // Sets callback which is invoked when this request and response are
  // finished.  After the invocation of this callback, the application
  // must not access request and response object.
  void on_close(close_cb cb) const;

  // Write trailer part.  This must be called after setting both
  // NGHTTP2_DATA_FLAG_EOF and NGHTTP2_DATA_FLAG_NO_END_STREAM set in
  // *data_flag parameter in generator_cb passed to session::submit()
  // function.
  void write_trailer(header_map h) const;

  // Cancels this request and response with given error code.
  void cancel(uint32_t error_code = NGHTTP2_INTERNAL_ERROR) const;

  // Resumes deferred uploading.
  void resume() const;

  // Returns method (e.g., GET).
  const std::string &method() const;

  // Returns request URI, split into components.
  const uri_ref &uri() const;

  // Returns request header fields.  The pusedo header fields, which
  // start with colon (:), are exluced from this list.
  const header_map &header() const;

  // Application must not call this directly.
  request_impl &impl() const;

private:
  std::unique_ptr<request_impl> impl_;
};

// Wrapper around an nghttp2_priority_spec.
class priority_spec {
public:
  // The default ctor is used only by sentinel values.
  priority_spec() = default;

  // Create a priority spec with the given priority settings.
  explicit priority_spec(const int32_t stream_id, const int32_t weight,
                         const bool exclusive = false);

  // Return a pointer to a valid nghttp2 priority spec, or null.
  const nghttp2_priority_spec *get() const;

  // Indicates whether or not this spec is valid (i.e. was constructed with
  // values).
  const bool valid() const;

private:
  nghttp2_priority_spec spec_;
  bool valid_ = false;
};

class session_impl;

class session {
public:
  // Starts HTTP/2 session by connecting to |host| and |service|
  // (e.g., "80") using clear text TCP connection with connect timeout
  // 60 seconds.
  session(boost::asio::io_service &io_service, const std::string &host,
          const std::string &service);

  // Starts HTTP/2 session by connecting to |host| and |service|
  // (e.g., "80") using clear text TCP connection with given connect
  // timeout.
  session(boost::asio::io_service &io_service, const std::string &host,
          const std::string &service,
          const boost::posix_time::time_duration &connect_timeout);

  // Starts HTTP/2 session by connecting to |host| and |service|
  // (e.g., "443") using encrypted SSL/TLS connection with connect
  // timeout 60 seconds.
  session(boost::asio::io_service &io_service,
          boost::asio::ssl::context &tls_context, const std::string &host,
          const std::string &service);

  // Starts HTTP/2 session by connecting to |host| and |service|
  // (e.g., "443") using encrypted SSL/TLS connection with given
  // connect timeout.
  session(boost::asio::io_service &io_service,
          boost::asio::ssl::context &tls_context, const std::string &host,
          const std::string &service,
          const boost::posix_time::time_duration &connect_timeout);

  ~session();

  session(session &&other) noexcept;
  session &operator=(session &&other) noexcept;

  // Sets callback which is invoked after connection is established.
  void on_connect(connect_cb cb) const;

  // Sets callback which is invoked there is connection level error
  // and session is terminated.
  void on_error(error_cb cb) const;

  // Sets read timeout, which defaults to 60 seconds.
  void read_timeout(const boost::posix_time::time_duration &t);

  // Shutdowns connection.
  void shutdown() const;

  // Returns underlying io_service object.
  boost::asio::io_service &io_service() const;

  // Submits request to server using |method| (e.g., "GET"), |uri|
  // (e.g., "http://localhost/") and optionally additional header
  // fields.  This function returns pointer to request object if it
  // succeeds, or nullptr and |ec| contains error message.
  const request *submit(boost::system::error_code &ec,
                        const std::string &method, const std::string &uri,
                        header_map h = header_map{},
                        priority_spec prio = priority_spec()) const;

  // Submits request to server using |method| (e.g., "GET"), |uri|
  // (e.g., "http://localhost/") and optionally additional header
  // fields.  The |data| is request body.  This function returns
  // pointer to request object if it succeeds, or nullptr and |ec|
  // contains error message.
  const request *submit(boost::system::error_code &ec,
                        const std::string &method, const std::string &uri,
                        std::string data, header_map h = header_map{},
                        priority_spec prio = priority_spec()) const;

  // Submits request to server using |method| (e.g., "GET"), |uri|
  // (e.g., "http://localhost/") and optionally additional header
  // fields.  The |cb| is used to generate request body.  This
  // function returns pointer to request object if it succeeds, or
  // nullptr and |ec| contains error message.
  const request *submit(boost::system::error_code &ec,
                        const std::string &method, const std::string &uri,
                        generator_cb cb, header_map h = header_map{},
                        priority_spec prio = priority_spec()) const;

private:
  std::shared_ptr<session_impl> impl_;
};

// configure |tls_ctx| for client use.  Currently, we just set NPN
// callback for HTTP/2.
boost::system::error_code
configure_tls_context(boost::system::error_code &ec,
                      boost::asio::ssl::context &tls_ctx);

} // namespace client

} // namespace asio_http2

} // namespace nghttp2

#endif // ASIO_HTTP2_CLIENT_H
