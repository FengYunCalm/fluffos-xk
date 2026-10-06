#include "base/std.h"

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/util.h>
#include <event2/listener.h>
#include <libwebsockets.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

#include "net/ws_ascii.h"
#include "net/ws_telnet.h"
#include "net/tls.h"
#include "comm.h"
#include "interactive.h"

enum PROTOCOL_ID {
  WS_HTTP = 0,
  WS_ASCII = PROTOCOL_WS_ASCII,
  WS_TELNET = PROTOCOL_WS_TELNET,
};

static struct lws_protocols protocols[] = {
    {"http", lws_callback_http_dummy, 0, 0, WS_HTTP},
    {"ascii", ws_ascii_callback, sizeof(struct ws_ascii_session), 4096, WS_ASCII},
    {"telnet", ws_telnet_callback, sizeof(struct ws_telnet_session), 4096, WS_TELNET},
    //for backward compatiblity with fluffos 2.x
    {"binary", ws_telnet_callback, sizeof(struct ws_telnet_session), 4096, WS_TELNET},
    {NULL, NULL, 0, 0} /* terminator */
};

static const struct lws_extension extensions[] = {
    {"permessage-deflate", lws_extension_callback_pm_deflate,
     "permessage-deflate"
     "; client_no_context_takeover"
     "; client_max_window_bits"},
    {NULL, NULL, NULL /* terminator */}};

// modified on create.
static struct lws_http_mount mount = {
    /* .mount_next */ nullptr, /* linked-list "next" */
    /* .mountpoint */ "/",     /* mountpoint URL */
    /* .origin */ nullptr,     /* serve from dir */
    /* .def */ "index.html",   /* default filename */
    /* .protocol */ nullptr,
    /* .cgienv */ nullptr,
    /* .extra_mimetypes */ nullptr,
    /* .interpret */ nullptr,
    /* .cgi_timeout */ 0,
    /* .cache_max_age */ 0,
    /* .auth_mask */ 0,
    /* .cache_reusable */ 0,
    /* .cache_revalidate */ 0,
    /* .cache_intermediaries */ 0,
    /* .cache_no */ 0, /* 4.5.8 新增字段 */
    /* .origin_protocol */ LWSMPRO_FILE, /* files in a dir */
    /* .mountpoint_len */ 1,             /* char count */
    /* .basic_auth_login_file */ nullptr,
};

void lws_log(int severity, const char *msg) {
  if (severity == LLL_ERR) {
    debug(all, "lws ERROR: %s", msg);
  } else {
    debug(websocket, "lws %d: %s", severity, msg);
  }
}

struct lws_context *init_websocket_context(event_base *base, port_def_t *port) {
  int logs = LLL_USER | LLL_WARN | LLL_ERR;

#ifdef DEBUG
  logs |= LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_DEBUG;
  // More debug levels
  /* | LLL_INFO */ /* | LLL_PARSER */ /* | LLL_HEADER */
  /* | LLL_EXT */ /* | LLL_CLIENT */  /* | LLL_LATENCY */
  /* | LLL_DEBUG */;
#endif
  lws_set_log_level(logs, lws_log);

  struct lws_context_creation_info info = {0};
  void *foreign_loops[1] = {base};

  info.foreign_loops = foreign_loops;

  DEBUG_CHECK(CONFIG_STR(__RC_WEBSOCKET_HTTP_DIR__) == nullptr, "Bug, no websocket http lib dir!");
  mount.origin = CONFIG_STR(__RC_WEBSOCKET_HTTP_DIR__);

  info.mounts = &mount;
  info.port = CONTEXT_PORT_NO_LISTEN_SERVER;
  info.protocols = protocols;
  info.extensions = extensions;
  info.pt_serv_buf_size = 128 * 1024;
  info.options = LWS_SERVER_OPTION_LIBEVENT | LWS_SERVER_OPTION_VALIDATE_UTF8;

  if (!port->tls_cert.empty() && !port->tls_key.empty()) {
    info.options |= LWS_SERVER_OPTION_DO_SSL_GLOBAL_INIT;
    info.options |= LWS_SERVER_OPTION_ALLOW_NON_SSL_ON_SSL_PORT;
    info.options |= LWS_SERVER_OPTION_REDIRECT_HTTP_TO_HTTPS;
    info.ssl_cipher_list =
        "ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:"
        "ECDHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:DHE-"
        "RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384";
    info.ssl_cert_filepath = port->tls_cert.c_str();
    info.ssl_private_key_filepath = port->tls_key.c_str();
    info.ssl_options_clear = SSL_OP_CIPHER_SERVER_PREFERENCE;
    info.ssl_options_set = SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3;
    info.ssl_min_proto_version = kTlsMinimumProtocolVersion;
  }
  // info.options |= LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
  info.user = (void *)port;

  auto context = lws_create_context(&info);

  if (!context) {
    lwsl_err("lws init failed\n");
    return nullptr;
  }

  std::string res;
  for (auto &p : protocols) {
    if (p.name) {
      res += p.name;
      res += " ";
    }
  }
  lwsl_user("WS protocols supported: %s\n", res.c_str());

  return context;
}

struct lws *init_user_websocket(struct lws_context *context, evutil_socket_t fd) {
  // Since lws v4.3 the context's vhost list is headed by an internal "system"
  // vhost, which carries none of our ws protocols; lws_adopt_socket() adopts
  // onto the list head, so adopt explicitly onto our own vhost instead.
  // (Upstream 8fe05a5d.)
  auto *vhost = lws_get_vhost_by_name(context, "default");
  if (!vhost) {
    return nullptr;
  }
  return lws_adopt_socket_vhost(vhost, fd);
}

void websocket_send_text(struct lws *wsi, const char *data, size_t len) {
  switch (lws_get_protocol(wsi)->id) {
    case WS_TELNET:
      ws_telnet_send(wsi, data, len);
      break;
    case WS_ASCII:
      ws_ascii_send(wsi, data, len);
      break;
    default:
      // No way to send message
      return;
  }
}

namespace {

std::string_view trim_ascii(std::string_view text) {
  while (!text.empty() && (text.front() == ' ' || text.front() == '\t' || text.front() == '\r' ||
                           text.front() == '\n')) {
    text.remove_prefix(1);
  }
  while (!text.empty() && (text.back() == ' ' || text.back() == '\t' || text.back() == '\r' ||
                           text.back() == '\n')) {
    text.remove_suffix(1);
  }
  return text;
}

bool parse_unsigned(std::string_view text, unsigned int *value) {
  if (text.empty()) {
    return false;
  }
  unsigned int parsed = 0;
  const auto result = std::from_chars(text.data(), text.data() + text.size(), parsed);
  if (result.ec != std::errc{} || result.ptr != text.data() + text.size()) {
    return false;
  }
  *value = parsed;
  return true;
}

bool parse_numeric_address(std::string_view text, int *family,
                           std::array<unsigned char, 16> *bytes, bool *was_mapped = nullptr) {
  text = trim_ascii(text);
  if (was_mapped) {
    *was_mapped = false;
  }
  if (text.empty() || text.find('%') != std::string_view::npos) {
    return false;
  }
  const std::string terminated(text);
  struct in_addr ipv4{};
  if (evutil_inet_pton(AF_INET, terminated.c_str(), &ipv4) == 1) {
    std::memcpy(bytes->data(), &ipv4, sizeof(ipv4));
    *family = AF_INET;
    return true;
  }
  struct in6_addr ipv6{};
  if (evutil_inet_pton(AF_INET6, terminated.c_str(), &ipv6) != 1) {
    return false;
  }
  std::memcpy(bytes->data(), &ipv6, sizeof(ipv6));
  *family = AF_INET6;
  static constexpr unsigned char mapped_prefix[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
  if (std::equal(std::begin(mapped_prefix), std::end(mapped_prefix), bytes->begin())) {
    if (was_mapped) {
      *was_mapped = true;
    }
    std::array<unsigned char, 16> normalized{};
    std::copy(bytes->begin() + 12, bytes->begin() + 16, normalized.begin());
    *bytes = normalized;
    *family = AF_INET;
  }
  return true;
}

bool address_matches_cidr(int family, const unsigned char *address,
                          const websocket_trusted_proxy_cidr_t &cidr) {
  if (family != cidr.family) {
    return false;
  }
  const size_t full_bytes = cidr.prefix_length / 8;
  const unsigned int remaining_bits = cidr.prefix_length % 8;
  if (!std::equal(address, address + full_bytes, cidr.network.begin())) {
    return false;
  }
  if (remaining_bits != 0) {
    const unsigned char mask = static_cast<unsigned char>(0xffu << (8 - remaining_bits));
    if ((address[full_bytes] & mask) != (cidr.network[full_bytes] & mask)) {
      return false;
    }
  }
  return true;
}

bool is_trusted_proxy(const sockaddr_storage &address,
                      const std::vector<websocket_trusted_proxy_cidr_t> &cidrs) {
  std::array<unsigned char, 16> bytes{};
  int family = address.ss_family;
  if (family == AF_INET) {
    const auto *ipv4 = reinterpret_cast<const sockaddr_in *>(&address);
    std::memcpy(bytes.data(), &ipv4->sin_addr, 4);
  } else if (family == AF_INET6) {
    const auto *ipv6 = reinterpret_cast<const sockaddr_in6 *>(&address);
    std::memcpy(bytes.data(), &ipv6->sin6_addr, 16);
    static constexpr unsigned char mapped_prefix[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0xff, 0xff};
    if (std::equal(std::begin(mapped_prefix), std::end(mapped_prefix), bytes.begin())) {
      std::array<unsigned char, 16> normalized{};
      std::copy(bytes.begin() + 12, bytes.end(), normalized.begin());
      bytes = normalized;
      family = AF_INET;
    }
  } else {
    return false;
  }
  return std::any_of(cidrs.begin(), cidrs.end(), [&](const auto &cidr) {
    return address_matches_cidr(family, bytes.data(), cidr);
  });
}

}  // namespace

bool websocket_parse_trusted_proxy_cidrs(
    const char *value, std::vector<websocket_trusted_proxy_cidr_t> *out, std::string *error) {
  if (!out || !error) {
    return false;
  }
  out->clear();
  error->clear();
  if (!value || !*value) {
    return true;
  }
  std::string_view remaining(value);
  while (!remaining.empty()) {
    const size_t comma = remaining.find(',');
    const auto item = trim_ascii(
        remaining.substr(0, comma == std::string_view::npos ? remaining.size() : comma));
    if (item.empty()) {
      *error = "empty CIDR entry";
      return false;
    }
    const size_t slash = item.find('/');
    if (slash == std::string_view::npos || item.find('/', slash + 1) != std::string_view::npos) {
      *error = "CIDR entry must include one prefix length";
      return false;
    }
    websocket_trusted_proxy_cidr_t cidr;
    bool was_mapped = false;
    if (!parse_numeric_address(item.substr(0, slash), &cidr.family, &cidr.network, &was_mapped)) {
      *error = "CIDR address must be numeric IPv4 or IPv6";
      return false;
    }
    unsigned int prefix = 0;
    if (!parse_unsigned(trim_ascii(item.substr(slash + 1)), &prefix) ||
        prefix > (was_mapped ? 128u : (cidr.family == AF_INET ? 32u : 128u)) ||
        (was_mapped && prefix < 96)) {
      *error = "CIDR prefix length is out of range";
      return false;
    }
    if (was_mapped) {
      cidr.prefix_length = static_cast<uint8_t>(prefix - 96);
    } else {
      cidr.prefix_length = static_cast<uint8_t>(prefix);
    }
    const size_t bytes_to_mask = cidr.family == AF_INET ? 4 : 16;
    const size_t full_bytes = cidr.prefix_length / 8;
    const unsigned int remaining_bits = cidr.prefix_length % 8;
    if (remaining_bits != 0) {
      cidr.network[full_bytes] &= static_cast<unsigned char>(0xffu << (8 - remaining_bits));
    }
    for (size_t i = full_bytes + (remaining_bits != 0 ? 1 : 0); i < bytes_to_mask; ++i) {
      cidr.network[i] = 0;
    }
    out->push_back(cidr);
    if (comma == std::string_view::npos) {
      break;
    }
    remaining.remove_prefix(comma + 1);
    if (remaining.empty()) {
      *error = "empty CIDR entry";
      out->clear();
      return false;
    }
  }
  return true;
}

bool websocket_get_client_address(struct lws *wsi, const port_def_t *port,
                                  sockaddr_storage *address, socklen_t *address_length) {
  if (!wsi || !port || !address || !address_length) {
    return false;
  }
  const auto fd = lws_get_socket_fd(lws_get_network_wsi(wsi));
  *address = {};
  *address_length = sizeof(*address);
  if (getpeername(fd, reinterpret_cast<sockaddr *>(address), address_length) != 0) {
    return false;
  }
  if (!is_trusted_proxy(*address, port->websocket_trusted_proxy_cidrs)) {
    return true;
  }

  std::array<char, 46> header{};
  const int first_fragment =
      lws_hdr_copy_fragment(wsi, header.data(), static_cast<int>(header.size()),
                            WSI_TOKEN_HTTP_X_REAL_IP, 0);
  if (first_fragment == -1) {
    return true;
  }
  if (first_fragment < 0) {
    return false;
  }
  if (first_fragment == 0) {
    return false;
  }
  const int header_length = lws_hdr_total_length(wsi, WSI_TOKEN_HTTP_X_REAL_IP);
  if (header_length <= 0 || header_length > 45) {
    return false;
  }
  const int copied = lws_hdr_copy(wsi, header.data(), static_cast<int>(header.size()),
                                  WSI_TOKEN_HTTP_X_REAL_IP);
  if (copied != header_length) {
    return false;
  }
  int family = AF_UNSPEC;
  std::array<unsigned char, 16> bytes{};
  if (!parse_numeric_address(std::string_view(header.data(), copied), &family, &bytes)) {
    return false;
  }
  *address = {};
  if (family == AF_INET) {
    auto *ipv4 = reinterpret_cast<sockaddr_in *>(address);
    ipv4->sin_family = AF_INET;
    std::memcpy(&ipv4->sin_addr, bytes.data(), 4);
    *address_length = sizeof(*ipv4);
  } else {
    auto *ipv6 = reinterpret_cast<sockaddr_in6 *>(address);
    ipv6->sin6_family = AF_INET6;
    std::memcpy(&ipv6->sin6_addr, bytes.data(), 16);
    *address_length = sizeof(*ipv6);
  }
  return true;
}

void close_websocket_context(struct lws_context *context) { lws_context_destroy(context); }

void close_user_websocket(struct lws *wsi) {
  if (!wsi) {
    return;
  }
  lws_set_timeout(wsi, pending_timeout::PENDING_FLUSH_STORED_SEND_BEFORE_CLOSE, LWS_TO_KILL_ASYNC);
  bool close_from_writable = false;
  switch (lws_get_protocol(wsi)->id) {
    case WS_TELNET: {
      auto pss = reinterpret_cast<ws_telnet_session *>(lws_wsi_user(wsi));
      if (pss) {
        pss->close_after_flush = true;
        close_from_writable = true;
        pss->user = nullptr;
      }
      break;
    }
    case WS_ASCII: {
      auto pss = reinterpret_cast<ws_ascii_session *>(lws_wsi_user(wsi));
      if (pss) {
        pss->close_after_flush = true;
        close_from_writable = true;
        pss->user = nullptr;
      }
      break;
    }
    default:
      break;
  }

  if (close_from_writable) {
    // The application-side evbuffer is outside lws, so an async kill can win
    // before the requested writable callback moves these final bytes into lws.
    // Give the application buffer a bounded drain; the protocol callback closes
    // as soon as it is empty, while this timeout covers a permanently choked peer.
    lws_set_timeout(wsi, pending_timeout::PENDING_FLUSH_STORED_SEND_BEFORE_CLOSE, 5);
    lws_callback_on_writable(wsi);
  }
}

void websocket_session_teardown(struct lws *wsi, struct interactive_t **user,
                                struct evbuffer **buffer) {
  if (!user || !buffer) {
    return;
  }

  auto *ip = *user;
  *user = nullptr;
  if (ip) {
    if (ip->lws == wsi) {
      ip->lws = nullptr;
    }
    remove_interactive(ip->ob, 0);
  }

  if (*buffer) {
    evbuffer_free(*buffer);
    *buffer = nullptr;
  }
}
