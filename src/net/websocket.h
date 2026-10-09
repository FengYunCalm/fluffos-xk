#ifndef NET_WEBSOCKET_H
#define NET_WEBSOCKET_H

#include <event2/util.h>

#include "base/internal/external_port.h"

#include <string>
#include <vector>

struct evbuffer;
struct lws;
struct lws_context;
struct interactive_t;

// Shared per-connection state for the ASCII and telnet websocket protocols.
// The protocol callbacks intentionally keep their receive paths separate, but
// session creation and close-after-flush handling are identical.
struct WebSocketSession {
  struct lws *wsi;
  struct interactive_t *user;
  struct evbuffer *buffer;
  bool close_after_flush;
};

using websocket_transport_setup_fn = void (*)(struct interactive_t *);

// Create and schedule the interactive user for an established websocket.
// `transport_setup`, when supplied, initializes protocol-specific state before
// the user logon callback is scheduled.
bool websocket_establish_session(struct lws *wsi, WebSocketSession *session,
                                 websocket_transport_setup_fn transport_setup);

// Close a driver-initiated websocket after the application buffer and lws pipe
// have drained. Returns true when the normal close frame was requested.
bool websocket_maybe_close_after_flush(struct lws *wsi, WebSocketSession *session);

// Parse the startup-only trusted proxy CIDR list.  The list is compiled into
// each websocket port before the listener is created.
bool websocket_parse_trusted_proxy_cidrs(
    const char *value, std::vector<websocket_trusted_proxy_cidr_t> *out, std::string *error);

// Resolve the peer address used for the websocket session.  X-Real-IP is
// applied only when the actual peer matches the port's trusted proxy list.
bool websocket_get_client_address(struct lws *wsi, const port_def_t *port,
                                  sockaddr_storage *address, socklen_t *address_length);

struct event_base;

// Initialize websocket context
struct lws_context* init_websocket_context(struct event_base* base, struct port_def_t* port);

void close_websocket_context(struct lws_context* context);

struct lws* init_user_websocket(struct lws_context*, evutil_socket_t);
void close_user_websocket(struct lws* wsi);
void websocket_session_teardown(struct lws* wsi, struct interactive_t **user,
                                struct evbuffer **buffer);

void websocket_send_text(struct lws*, const char*, size_t);

#endif /* NET_WEBSOCKET_H */
