#include "base/std.h"

#include "net/tls.h"

#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif
#include <string>

void tls_library_init() {
  static int called = 0;

  if (!called) {
    called = 1;

    /* Initialize the OpenSSL library */
#if OPENSSL_VERSION_NUMBER < 0x10100000L
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
#else
    OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS, nullptr);
#endif
  }
}

SSL_CTX* tls_server_init(std::string_view file_cert, std::string_view file_key) {
  tls_library_init();

  /* We MUST have entropy, or else there's no point to crypto. */
  if (!RAND_poll() && !RAND_status()) return nullptr;

  SSL_CTX* server_ctx = SSL_CTX_new(SSLv23_server_method());
  if (!server_ctx) {
    debug_message("Couldn't allocate a TLS server context.\n");
    return nullptr;
  }

  if (!tls_set_minimum_protocol_version(server_ctx)) {
    debug_message("Couldn't set the minimum TLS protocol version.\n");
    SSL_CTX_free(server_ctx);
    return nullptr;
  }

  if (!SSL_CTX_use_certificate_chain_file(server_ctx, file_cert.data()) ||
      !SSL_CTX_use_PrivateKey_file(server_ctx, file_key.data(), SSL_FILETYPE_PEM) ||
      !SSL_CTX_check_private_key(server_ctx)) {
    debug_message("Couldn't read '%s' or '%s' file. Please verify these are valid PEM files.",
                  file_cert.data(), file_key.data());
    SSL_CTX_free(server_ctx);
    return nullptr;
  }
  SSL_CTX_set_options(server_ctx, SSL_OP_NO_SSLv2);
  SSL_CTX_set_options(server_ctx, SSL_OP_NO_SSLv3);

  return server_ctx;
}

void tls_server_close(SSL_CTX* ssl_ctx) { SSL_CTX_free(ssl_ctx); }

SSL* tls_get_client_ctx(SSL_CTX* server_ctx) {
  auto ctx = SSL_new(server_ctx);
  return ctx;
}

int tls_verify_callback(int preverify_ok, X509_STORE_CTX* x509_ctx) {
  char  buf[256];
  X509* cert;
  int   err, depth;

  cert = X509_STORE_CTX_get_current_cert(x509_ctx);
  err = X509_STORE_CTX_get_error(x509_ctx);
  depth = X509_STORE_CTX_get_error_depth(x509_ctx);

  X509_NAME_oneline(X509_get_subject_name(cert), buf, 256);

  /* If error is not X509_V_OK, print out the error information */
  if (err != X509_V_OK) {
    debug(sockets, "tls_verify_callback: verify error:num=%d:%s:depth=%d:%s\n", err,
           X509_verify_cert_error_string(err), depth, buf);
  }

  return preverify_ok;
}

bool tls_set_minimum_protocol_version(SSL_CTX* ctx) {
  return ctx != nullptr && SSL_CTX_set_min_proto_version(ctx, kTlsMinimumProtocolVersion) == 1;
}

bool tls_configure_client_identity(SSL* ssl, const sockaddr* peer, size_t peer_len,
                                   const char* sni_hostname, bool verify_peer) {
  if (ssl == nullptr) {
    return false;
  }

  if (sni_hostname != nullptr) {
    if (*sni_hostname == '\0' || SSL_set_tlsext_host_name(ssl, sni_hostname) != 1) {
      return false;
    }
    return !verify_peer || SSL_set1_host(ssl, sni_hostname) == 1;
  }

  if (!verify_peer || peer == nullptr) {
    return !verify_peer;
  }

  char numeric_host[NI_MAXHOST] = {};
  if (getnameinfo(peer, static_cast<socklen_t>(peer_len), numeric_host,
                  sizeof(numeric_host), nullptr, 0, NI_NUMERICHOST) != 0) {
    return false;
  }
  auto* verify_param = SSL_get0_param(ssl);
  return verify_param != nullptr &&
         X509_VERIFY_PARAM_set1_ip_asc(verify_param, numeric_host) == 1;
}

SSL_CTX* tls_client_init() {
  tls_library_init();

  /* We MUST have entropy, or else there's no point to crypto. */
  if (!RAND_poll() && !RAND_status()) return nullptr;

  SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
  if (!ctx) {
    debug_message("Couldn't allocate a TLS client context.\n");
    return nullptr;
  }

  if (!tls_set_minimum_protocol_version(ctx)) {
    debug_message("Couldn't set the minimum TLS protocol version.\n");
    SSL_CTX_free(ctx);
    return nullptr;
  }

  SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
  SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv3);

  // Load system default CA certificates.
  if (SSL_CTX_set_default_verify_paths(ctx) != 1) {
    debug_message("Couldn't load system default CA certificates.\n");
    SSL_CTX_free(ctx);
    return nullptr;
  }

  // setup certificate verification
  SSL_CTX_set_verify(ctx, SSL_VERIFY_PEER, tls_verify_callback);

  return ctx;
}
