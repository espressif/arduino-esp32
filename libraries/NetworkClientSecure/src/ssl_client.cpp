/* Provide SSL/TLS functions to ESP32 with Arduino IDE
*
* Adapted from the ssl_client1 example of mbedtls.
*
* Original Copyright (C) 2006-2015, ARM Limited, All Rights Reserved, Apache 2.0 License.
* Additions Copyright (C) 2017 Evandro Luis Copercini, Apache 2.0 License.
*/

#include "Arduino.h"
#include <esp32-hal-log.h>
#include <lwip/err.h>
#include <lwip/sockets.h>
#include <lwip/sys.h>
#include <lwip/netdb.h>
#include <mbedtls/sha256.h>
#include <mbedtls/oid.h>
#include <algorithm>
#include <string>
#include "ssl_client.h"
#include "esp_crt_bundle.h"

#if !defined(MBEDTLS_KEY_EXCHANGE__SOME__PSK_ENABLED) && !defined(MBEDTLS_KEY_EXCHANGE_SOME_PSK_ENABLED)
#warning "Please call `idf.py menuconfig` then go to Component config -> mbedTLS -> TLS Key Exchange Methods -> Enable pre-shared-key ciphersuites and then check `Enable PSK based ciphersuite modes`. Save and Quit."
#else

const char *pers = "esp32-tls";

static int _handle_error(int err, const char *function, int line) {
  if (err == -30848) {
    return err;
  }
#ifdef MBEDTLS_ERROR_C
  char error_buf[100];
  mbedtls_strerror(err, error_buf, 100);
  log_e("[%s():%d]: (%d) %s", function, line, err, error_buf);
#else
  log_e("[%s():%d]: code %d", function, line, err);
#endif
  return err;
}

#define handle_error(e) _handle_error(e, __FUNCTION__, __LINE__)


void ssl_init(sslclient_context *ssl_client) {
  // reset embedded pointers to zero
  memset(ssl_client, 0, sizeof(sslclient_context));
  mbedtls_ssl_init(&ssl_client->ssl_ctx);
  mbedtls_ssl_config_init(&ssl_client->ssl_conf);
  mbedtls_ctr_drbg_init(&ssl_client->drbg_ctx);
}

int start_ssl_client(sslclient_context *ssl_client, const IPAddress &ip, uint32_t port, const char *hostname, int timeout, const char *rootCABuff, bool useRootCABundle, const char *cli_cert, const char *cli_key, const char *pskIdent, const char *psKey, bool insecure, const char **alpn_protos) {
  int ret;
  int enable = 1;
  log_v("Free internal heap before TLS %u", ESP.getFreeHeap());

  if (rootCABuff == NULL && pskIdent == NULL && psKey == NULL && !insecure && !useRootCABundle) {
    return -1;
  }

  int domain = ip.type() == IPv6 ? AF_INET6 : AF_INET;
  log_v("Starting socket (domain %d)", domain);
  ssl_client->socket = -1;

  ssl_client->socket = lwip_socket(domain, SOCK_STREAM, IPPROTO_TCP);
  if (ssl_client->socket < 0) {
    log_e("ERROR opening socket");
    return ssl_client->socket;
  }

  fcntl(ssl_client->socket, F_SETFL, fcntl(ssl_client->socket, F_GETFL, 0) | O_NONBLOCK);
  struct sockaddr_storage serv_addr = {};
  if (domain == AF_INET6) {
    struct sockaddr_in6 *tmpaddr = (struct sockaddr_in6 *)&serv_addr;
    tmpaddr->sin6_family = AF_INET6;
    for (int index = 0; index < 16; index++) {
      tmpaddr->sin6_addr.s6_addr[index] = ip[index];
    }
    tmpaddr->sin6_port = htons(port);
    tmpaddr->sin6_scope_id = ip.zone();
  } else {
    struct sockaddr_in *tmpaddr = (struct sockaddr_in *)&serv_addr;
    tmpaddr->sin_family = AF_INET;
    tmpaddr->sin_addr.s_addr = ip;
    tmpaddr->sin_port = htons(port);
  }

  if (timeout <= 0) {
    timeout = 30000;  // Milli seconds.
  }

  ssl_client->socket_timeout = timeout;

  fd_set fdset;
  struct timeval tv;
  FD_ZERO(&fdset);
  FD_SET(ssl_client->socket, &fdset);
  tv.tv_sec = timeout / 1000;
  tv.tv_usec = (timeout % 1000) * 1000;

  int res = lwip_connect(ssl_client->socket, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
  if (res < 0 && errno != EINPROGRESS) {
    log_e("connect on fd %d, errno: %d, \"%s\"", ssl_client->socket, errno, strerror(errno));
    lwip_close(ssl_client->socket);
    ssl_client->socket = -1;
    return -1;
  }

  res = select(ssl_client->socket + 1, nullptr, &fdset, nullptr, timeout < 0 ? nullptr : &tv);
  if (res < 0) {
    log_e("select on fd %d, errno: %d, \"%s\"", ssl_client->socket, errno, strerror(errno));
    lwip_close(ssl_client->socket);
    ssl_client->socket = -1;
    return -1;
  } else if (res == 0) {
    log_i("select returned due to timeout %d ms for fd %d", timeout, ssl_client->socket);
    lwip_close(ssl_client->socket);
    ssl_client->socket = -1;
    return -1;
  } else {
    int sockerr;
    socklen_t len = (socklen_t)sizeof(int);
    res = getsockopt(ssl_client->socket, SOL_SOCKET, SO_ERROR, &sockerr, &len);

    if (res < 0) {
      log_e("getsockopt on fd %d, errno: %d, \"%s\"", ssl_client->socket, errno, strerror(errno));
      lwip_close(ssl_client->socket);
      ssl_client->socket = -1;
      return -1;
    }

    if (sockerr != 0) {
      log_e("socket error on fd %d, errno: %d, \"%s\"", ssl_client->socket, sockerr, strerror(sockerr));
      lwip_close(ssl_client->socket);
      ssl_client->socket = -1;
      return -1;
    }
  }


#define ROE(x, msg) \
  { \
    if (((x) < 0)) { \
      log_e("LWIP Socket config of " msg " failed."); \
      return -1; \
    } \
  }
  ROE(lwip_setsockopt(ssl_client->socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)), "SO_RCVTIMEO");
  ROE(lwip_setsockopt(ssl_client->socket, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)), "SO_SNDTIMEO");

  ROE(lwip_setsockopt(ssl_client->socket, IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(enable)), "TCP_NODELAY");
  ROE(lwip_setsockopt(ssl_client->socket, SOL_SOCKET, SO_KEEPALIVE, &enable, sizeof(enable)), "SO_KEEPALIVE");



  log_v("Seeding the random number generator");
  mbedtls_entropy_init(&ssl_client->entropy_ctx);

  ret = mbedtls_ctr_drbg_seed(&ssl_client->drbg_ctx, mbedtls_entropy_func,
                              &ssl_client->entropy_ctx, (const unsigned char *)pers, strlen(pers));
  if (ret < 0) {
    return handle_error(ret);
  }

  log_v("Setting up the SSL/TLS structure...");

  if ((ret = mbedtls_ssl_config_defaults(&ssl_client->ssl_conf,
                                         MBEDTLS_SSL_IS_CLIENT,
                                         MBEDTLS_SSL_TRANSPORT_STREAM,
                                         MBEDTLS_SSL_PRESET_DEFAULT))
      != 0) {
    return handle_error(ret);
  }

  if (alpn_protos != NULL) {
    log_v("Setting ALPN protocols");
    if ((ret = mbedtls_ssl_conf_alpn_protocols(&ssl_client->ssl_conf, alpn_protos)) != 0) {
      return handle_error(ret);
    }
  }

  // MBEDTLS_SSL_VERIFY_REQUIRED if a CA certificate is defined on Arduino IDE and
  // MBEDTLS_SSL_VERIFY_NONE if not.

  if (insecure) {
    mbedtls_ssl_conf_authmode(&ssl_client->ssl_conf, MBEDTLS_SSL_VERIFY_NONE);
    log_d("WARNING: Skipping SSL Verification. INSECURE!");
  } else if (rootCABuff != NULL) {
    log_v("Loading CA cert");
    mbedtls_x509_crt_init(&ssl_client->ca_cert);
    mbedtls_ssl_conf_authmode(&ssl_client->ssl_conf, MBEDTLS_SSL_VERIFY_REQUIRED);
    ret = mbedtls_x509_crt_parse(&ssl_client->ca_cert, (const unsigned char *)rootCABuff, strlen(rootCABuff) + 1);
    mbedtls_ssl_conf_ca_chain(&ssl_client->ssl_conf, &ssl_client->ca_cert, NULL);
    //mbedtls_ssl_conf_verify(&ssl_client->ssl_ctx, my_verify, NULL );
    if (ret < 0) {
      // free the ca_cert in the case parse failed, otherwise, the old ca_cert still in the heap memory, that lead to "out of memory" crash.
      mbedtls_x509_crt_free(&ssl_client->ca_cert);
      return handle_error(ret);
    }
  } else if (useRootCABundle) {
    log_v("Attaching root CA cert bundle");
    ret = esp_crt_bundle_attach(&ssl_client->ssl_conf);

    if (ret < 0) {
      return handle_error(ret);
    }
  } else if (pskIdent != NULL && psKey != NULL) {
    log_v("Setting up PSK");
    // convert PSK from hex to binary
    if ((strlen(psKey) & 1) != 0 || strlen(psKey) > 2 * MBEDTLS_PSK_MAX_LEN) {
      log_e("pre-shared key not valid hex or too long");
      return -1;
    }
    unsigned char psk[MBEDTLS_PSK_MAX_LEN];
    size_t psk_len = strlen(psKey) / 2;
    for (int j = 0; j < strlen(psKey); j += 2) {
      char c = psKey[j];
      if (c >= '0' && c <= '9') c -= '0';
      else if (c >= 'A' && c <= 'F') c -= 'A' - 10;
      else if (c >= 'a' && c <= 'f') c -= 'a' - 10;
      else return -1;
      psk[j / 2] = c << 4;
      c = psKey[j + 1];
      if (c >= '0' && c <= '9') c -= '0';
      else if (c >= 'A' && c <= 'F') c -= 'A' - 10;
      else if (c >= 'a' && c <= 'f') c -= 'a' - 10;
      else return -1;
      psk[j / 2] |= c;
    }
    // set mbedtls config
    ret = mbedtls_ssl_conf_psk(&ssl_client->ssl_conf, psk, psk_len,
                               (const unsigned char *)pskIdent, strlen(pskIdent));
    if (ret != 0) {
      log_e("mbedtls_ssl_conf_psk returned %d", ret);
      return handle_error(ret);
    }
  } else {
    return -1;
  }

  // Note - this check for BOTH key and cert is relied on
  // later during cleanup.

  if (!insecure && cli_cert != NULL && cli_key != NULL) {
    mbedtls_x509_crt_init(&ssl_client->client_cert);
    mbedtls_pk_init(&ssl_client->client_key);

    log_v("Loading CRT cert");

    ret = mbedtls_x509_crt_parse(&ssl_client->client_cert, (const unsigned char *)cli_cert, strlen(cli_cert) + 1);
    if (ret < 0) {
      // free the client_cert in the case parse failed, otherwise, the old client_cert still in the heap memory, that lead to "out of memory" crash.
      mbedtls_x509_crt_free(&ssl_client->client_cert);
      return handle_error(ret);
    }

    log_v("Loading private key");
    mbedtls_ctr_drbg_context ctr_drbg;
    mbedtls_ctr_drbg_init(&ctr_drbg);
    ret = mbedtls_pk_parse_key(&ssl_client->client_key, (const unsigned char *)cli_key, strlen(cli_key) + 1, NULL, 0, mbedtls_ctr_drbg_random, &ctr_drbg);
    mbedtls_ctr_drbg_free(&ctr_drbg);

    if (ret != 0) {
      mbedtls_x509_crt_free(&ssl_client->client_cert);  // cert+key are free'd in pair
      return handle_error(ret);
    }

    mbedtls_ssl_conf_own_cert(&ssl_client->ssl_conf, &ssl_client->client_cert, &ssl_client->client_key);
  }

  log_v("Setting hostname for TLS session...");

  // Hostname set here should match CN in server certificate
  if ((ret = mbedtls_ssl_set_hostname(&ssl_client->ssl_ctx, hostname != NULL ? hostname : ip.toString().c_str())) != 0) {
    return handle_error(ret);
  }

  mbedtls_ssl_conf_rng(&ssl_client->ssl_conf, mbedtls_ctr_drbg_random, &ssl_client->drbg_ctx);

  if ((ret = mbedtls_ssl_setup(&ssl_client->ssl_ctx, &ssl_client->ssl_conf)) != 0) {
    return handle_error(ret);
  }

  mbedtls_ssl_set_bio(&ssl_client->ssl_ctx, &ssl_client->socket, mbedtls_net_send, mbedtls_net_recv, NULL);
  return ssl_client->socket;
}

int ssl_starttls_handshake(sslclient_context *ssl_client) {
  char buf[512];
  int ret, flags;

  log_v("Performing the SSL/TLS handshake...");
  unsigned long handshake_start_time = millis();
  while ((ret = mbedtls_ssl_handshake(&ssl_client->ssl_ctx)) != 0) {
    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
      return handle_error(ret);
    }
    if ((millis() - handshake_start_time) > ssl_client->handshake_timeout)
      return -1;
    vTaskDelay(2);  //2 ticks
  }


  if (ssl_client->client_cert.version) {
    log_d("Protocol is %s Ciphersuite is %s", mbedtls_ssl_get_version(&ssl_client->ssl_ctx), mbedtls_ssl_get_ciphersuite(&ssl_client->ssl_ctx));
    if ((ret = mbedtls_ssl_get_record_expansion(&ssl_client->ssl_ctx)) >= 0) {
      log_d("Record expansion is %d", ret);
    } else {
      log_w("Record expansion is unknown (compression)");
    }
  }

  log_v("Verifying peer X.509 certificate...");

  if ((flags = mbedtls_ssl_get_verify_result(&ssl_client->ssl_ctx)) != 0) {
    memset(buf, 0, sizeof(buf));
    mbedtls_x509_crt_verify_info(buf, sizeof(buf), "  ! ", flags);
    log_e("Failed to verify peer certificate! verification info: %s", buf);
    return handle_error(ret);
  } else {
    log_v("Certificate verified.");
  }

  if (ssl_client->ca_cert.version) {
    mbedtls_x509_crt_free(&ssl_client->ca_cert);
  }

  // We know that we always have a client cert/key pair -- and we
  // cannot look into the private client_key pk struct for newer
  // versions of mbedtls. So rely on a public field of the cert
  // and infer that there is a key too.
  if (ssl_client->client_cert.version) {
    mbedtls_x509_crt_free(&ssl_client->client_cert);
    mbedtls_pk_free(&ssl_client->client_key);
  }

  log_v("Free internal heap after TLS %u", ESP.getFreeHeap());

  return ssl_client->socket;
}

void stop_ssl_socket(sslclient_context *ssl_client, const char *rootCABuff, const char *cli_cert, const char *cli_key) {
  log_v("Cleaning SSL connection.");

  if (ssl_client->socket >= 0) {
    lwip_close(ssl_client->socket);
    ssl_client->socket = -1;
  }

  // avoid memory leak if ssl connection attempt failed
  // if (ssl_client->ssl_conf.ca_chain != NULL) {
  mbedtls_x509_crt_free(&ssl_client->ca_cert);
  // }
  // if (ssl_client->ssl_conf.key_cert != NULL) {
  mbedtls_x509_crt_free(&ssl_client->client_cert);
  mbedtls_pk_free(&ssl_client->client_key);
  // }
  mbedtls_ssl_free(&ssl_client->ssl_ctx);
  mbedtls_ssl_config_free(&ssl_client->ssl_conf);
  mbedtls_ctr_drbg_free(&ssl_client->drbg_ctx);
  mbedtls_entropy_free(&ssl_client->entropy_ctx);

  // save only interesting fields
  int handshake_timeout = ssl_client->handshake_timeout;
  int socket_timeout = ssl_client->socket_timeout;

  // reset embedded pointers to zero
  memset(ssl_client, 0, sizeof(sslclient_context));

  ssl_client->handshake_timeout = handshake_timeout;
  ssl_client->socket_timeout = socket_timeout;
}


int data_to_read(sslclient_context *ssl_client) {
  int ret, res;
  ret = mbedtls_ssl_read(&ssl_client->ssl_ctx, NULL, 0);
  //log_e("RET: %i",ret);   //for low level debug
  res = mbedtls_ssl_get_bytes_avail(&ssl_client->ssl_ctx);
  //log_e("RES: %i",res);    //for low level debug
  if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE && ret < 0) {
    return handle_error(ret);
  }

  return res;
}

int send_ssl_data(sslclient_context *ssl_client, const uint8_t *data, size_t len) {
  unsigned long write_start_time = millis();
  int ret = -1;

  while ((ret = mbedtls_ssl_write(&ssl_client->ssl_ctx, data, len)) <= 0) {
    if ((millis() - write_start_time) > ssl_client->socket_timeout) {
      log_v("SSL write timed out.");
      return -1;
    }

    if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE && ret < 0) {
      log_v("Handling error %d", ret);  //for low level debug
      return handle_error(ret);
    }

    //wait for space to become available
    vTaskDelay(2);
  }

  return ret;
}

// Some protocols, such as SMTP, XMPP, MySQL/Posgress and various others
// do a 'in-line' upgrade from plaintext to SSL or TLS (usually with some
// sort of 'STARTTLS' textual command from client to sever). For this
// we need to have access to the 'raw' socket; i.e. without TLS/SSL state
// handling before the handshake starts; but after setting up the TLS
// connection.
//
int peek_net_receive(sslclient_context *ssl_client, int timeout) {
#if MBEDTLS_FIXED_LINKING_NET_POLL
  int ret = mbedtls_net_poll((mbedtls_net_context *)ssl_client, MBEDTLS_NET_POLL_READ, timeout);
  ret == MBEDTLS_NET_POLL_READ ? 1 : ret;
#else
  // We should be using mbedtls_net_poll(); which is part of mbedtls and
  // included in the EspressifSDK. Unfortunately - it did not make it into
  // the statically linked library file. So, for now, we replace it by
  // substancially similar code.
  //
  struct timeval tv = { .tv_sec = timeout / 1000, .tv_usec = (timeout % 1000) * 1000 };

  fd_set fdset;
  FD_SET(ssl_client->socket, &fdset);

  int ret = select(ssl_client->socket + 1, &fdset, nullptr, nullptr, timeout < 0 ? nullptr : &tv);
  if (ret < 0) {
    log_e("select on read fd %d, errno: %d, \"%s\"", ssl_client->socket, errno, strerror(errno));
    lwip_close(ssl_client->socket);
    ssl_client->socket = -1;
    return -1;
  };
#endif
  return ret;
};

int get_net_receive(sslclient_context *ssl_client, uint8_t *data, int length) {
  int ret = peek_net_receive(ssl_client, ssl_client->socket_timeout);
  if (ret > 0)
    ret = mbedtls_net_recv(ssl_client, data, length);

  // log_v( "%d bytes NET read of %d", ret, length);   //for low level debug
  return ret;
}

int send_net_data(sslclient_context *ssl_client, const uint8_t *data, size_t len) {
  int ret = mbedtls_net_send(ssl_client, data, len);
  // log_v("Net sending %d btes->ret %d", len, ret); //for low level debug
  return ret;
}


int get_ssl_receive(sslclient_context *ssl_client, uint8_t *data, int length) {
  int ret = mbedtls_ssl_read(&ssl_client->ssl_ctx, data, length);
  // log_v( "%d bytes SSL read", ret);   //for low level debug
  return ret;
}

static bool parseHexNibble(char pb, uint8_t *res) {
  if (pb >= '0' && pb <= '9') {
    *res = (uint8_t)(pb - '0');
    return true;
  } else if (pb >= 'a' && pb <= 'f') {
    *res = (uint8_t)(pb - 'a' + 10);
    return true;
  } else if (pb >= 'A' && pb <= 'F') {
    *res = (uint8_t)(pb - 'A' + 10);
    return true;
  }
  return false;
}

// Compare a name from certificate and domain name, return true if they match
static bool matchName(const std::string &name, const std::string &domainName) {
  size_t wildcardPos = name.find('*');
  if (wildcardPos == std::string::npos) {
    // Not a wildcard, expect an exact match
    return name == domainName;
  }

  size_t firstDotPos = name.find('.');
  if (wildcardPos > firstDotPos) {
    // Wildcard is not part of leftmost component of domain name
    // Do not attempt to match (rfc6125 6.4.3.1)
    return false;
  }
  if (wildcardPos != 0 || firstDotPos != 1) {
    // Matching of wildcards such as baz*.example.com and b*z.example.com
    // is optional. Maybe implement this in the future?
    return false;
  }
  size_t domainNameFirstDotPos = domainName.find('.');
  if (domainNameFirstDotPos == std::string::npos) {
    return false;
  }
  return domainName.substr(domainNameFirstDotPos) == name.substr(firstDotPos);
}

// Verifies certificate provided by the peer to match specified SHA256 fingerprint
bool verify_ssl_fingerprint(sslclient_context *ssl_client, const char *fp, const char *domain_name) {
  // Convert hex string to byte array
  uint8_t fingerprint_local[32];
  int len = strlen(fp);
  int pos = 0;
  for (size_t i = 0; i < sizeof(fingerprint_local); ++i) {
    while (pos < len && ((fp[pos] == ' ') || (fp[pos] == ':'))) {
      ++pos;
    }
    if (pos > len - 2) {
      log_d("pos:%d len:%d fingerprint too short", pos, len);
      return false;
    }
    uint8_t high, low;
    if (!parseHexNibble(fp[pos], &high) || !parseHexNibble(fp[pos + 1], &low)) {
      log_d("pos:%d len:%d invalid hex sequence: %c%c", pos, len, fp[pos], fp[pos + 1]);
      return false;
    }
    pos += 2;
    fingerprint_local[i] = low | (high << 4);
  }

  // Calculate certificate's SHA256 fingerprint
  uint8_t fingerprint_remote[32];
  if (!get_peer_fingerprint(ssl_client, fingerprint_remote))
    return false;

  // Check if fingerprints match
  if (memcmp(fingerprint_local, fingerprint_remote, 32)) {
    log_d("fingerprint doesn't match");
    return false;
  }

  // Additionally check if certificate has domain name if provided
  if (domain_name)
    return verify_ssl_dn(ssl_client, domain_name);
  else
    return true;
}

bool get_peer_fingerprint(sslclient_context *ssl_client, uint8_t sha256[32]) {
  if (!ssl_client) {
    log_d("Invalid ssl_client pointer");
    return false;
  };

  const mbedtls_x509_crt *crt = mbedtls_ssl_get_peer_cert(&ssl_client->ssl_ctx);
  if (!crt) {
    log_d("Failed to get peer cert.");
    return false;
  };

  mbedtls_sha256_context sha256_ctx;
  mbedtls_sha256_init(&sha256_ctx);
  mbedtls_sha256_starts(&sha256_ctx, false);
  mbedtls_sha256_update(&sha256_ctx, crt->raw.p, crt->raw.len);
  mbedtls_sha256_finish(&sha256_ctx, sha256);

  return true;
}

// Checks if peer certificate has specified domain in CN or SANs
bool verify_ssl_dn(sslclient_context *ssl_client, const char *domain_name) {
  log_d("domain name: '%s'", (domain_name) ? domain_name : "(null)");
  std::string domain_name_str(domain_name);
  std::transform(domain_name_str.begin(), domain_name_str.end(), domain_name_str.begin(), ::tolower);

  // Get certificate provided by the peer
  const mbedtls_x509_crt *crt = mbedtls_ssl_get_peer_cert(&ssl_client->ssl_ctx);

  // Check for domain name in SANs
  const mbedtls_x509_sequence *san = &crt->subject_alt_names;
  while (san != nullptr) {
    std::string san_str((const char *)san->buf.p, san->buf.len);
    std::transform(san_str.begin(), san_str.end(), san_str.begin(), ::tolower);

    if (matchName(san_str, domain_name_str))
      return true;

    log_d("SAN '%s': no match", san_str.c_str());

    // Fetch next SAN
    san = san->next;
  }

  // Check for domain name in CN
  const mbedtls_asn1_named_data *common_name = &crt->subject;
  while (common_name != nullptr) {
    // While iterating through DN objects, check for CN object
    if (!MBEDTLS_OID_CMP(MBEDTLS_OID_AT_CN, &common_name->oid)) {
      std::string common_name_str((const char *)common_name->val.p, common_name->val.len);

      if (matchName(common_name_str, domain_name_str))
        return true;

      log_d("CN '%s': not match", common_name_str.c_str());
    }

    // Fetch next DN object
    common_name = common_name->next;
  }

  return false;
}
#endif
