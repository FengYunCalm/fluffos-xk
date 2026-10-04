#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/rand.h>

#include <cstddef>
#include <string>

struct sockaddr;

constexpr int kTlsMinimumProtocolVersion = TLS1_2_VERSION;

bool tls_set_minimum_protocol_version(SSL_CTX* ctx);
SSL_CTX* tls_server_init(std::string_view file_cert, std::string_view file_key);
void tls_server_close(SSL_CTX* ssl_ctx);
SSL* tls_get_client_ctx(SSL_CTX* server_ctx);
SSL_CTX* tls_client_init();
int tls_verify_callback(int preverify_ok, X509_STORE_CTX* x509_ctx);

// Configure the identity that a verifying TLS client must find in the peer
// certificate. An explicit SNI hostname is verified as a DNS name; otherwise
// the numeric peer address is verified as an IP SAN.
bool tls_configure_client_identity(SSL* ssl, const sockaddr* peer, size_t peer_len,
                                   const char* sni_hostname, bool verify_peer);
