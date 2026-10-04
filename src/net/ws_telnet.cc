#include "base/std.h"

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/util.h>
#include <event2/listener.h>

#include <libwebsockets.h>
#include <cstdlib>

#include "net/ws_telnet.h"
#include "comm.h"
#include "interactive.h"
#include "net/websocket.h"
#include "net/telnet.h"

// from comm.cc
interactive_t *new_user(port_def_t *port, evutil_socket_t fd, sockaddr *addr, socklen_t addrlen);
extern void on_user_logon(interactive_t *);
extern void remove_interactive(object_t *ob, int dested);
int cmd_in_buf(interactive_t *ip);

void on_user_websocket_telnet_received(interactive_t *ip, const char *data, size_t len);

namespace {

/* one of these is created for each vhost our protocol is used with */
struct per_vhost_data {
  struct lws_context *context;
  struct lws_vhost *vhost;
  const struct lws_protocols *protocol;

  ws_telnet_session *pss_list; /* linked-list of live pss*/
};

}  // namespace

int ws_telnet_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in,
                      size_t len) {
  auto *pss = (ws_telnet_session *)user;
  auto *vhd =
      (struct per_vhost_data *)lws_protocol_vh_priv_get(lws_get_vhost(wsi), lws_get_protocol(wsi));

  switch (reason) {
    case LWS_CALLBACK_PROTOCOL_INIT:
      lwsl_info("LWS_CALLBACK_PROTOCOL_INIT\n");
      // freed automatically when context is destroyed.
      vhd = reinterpret_cast<per_vhost_data *>(lws_protocol_vh_priv_zalloc(
          lws_get_vhost(wsi), lws_get_protocol(wsi), sizeof(struct per_vhost_data)));
      vhd->context = lws_get_context(wsi);
      vhd->protocol = lws_get_protocol(wsi);
      vhd->vhost = lws_get_vhost(wsi);
     break;
    case LWS_CALLBACK_PROTOCOL_DESTROY:
      lwsl_info("LWS_CALLBACK_PROTOCOL_DESTROY\n");
      break;
    case LWS_CALLBACK_ESTABLISHED: {
      /* generate a block of output before travis times us out */
      lwsl_info("LWS_CALLBACK_ESTABLISHED\n");

      auto port = (port_def_t *)lws_context_user(lws_get_context(wsi));
      auto fd = lws_get_socket_fd(lws_get_network_wsi(wsi));

      sockaddr_storage addr = {};
      socklen_t addrlen = sizeof(addr);
      if (!websocket_get_client_address(wsi, port, &addr, &addrlen)) {
        lwsl_warn("LWS_CALLBACK_ESTABLISHED: invalid peer address or trusted X-Real-IP\n");
        return -1;
      }

      auto ip = new_user(port, fd, reinterpret_cast<sockaddr *>(&addr), addrlen);

      pss->user = ip;
      pss->buffer = evbuffer_new();
      if (!pss->buffer) {
        websocket_session_teardown(wsi, &pss->user, &pss->buffer);
        return -1;
      }

      ip->iflags |= HANDSHAKE_COMPLETE;
      ip->lws = wsi;

      //handshake complete so lets setup telnet layer
      ip->telnet = net_telnet_init(ip);
      send_initial_telnet_negotiations(ip);

      auto base = evconnlistener_get_base(port->ev_conn);
      if (!schedule_user_logon(base, ip)) {
        websocket_session_teardown(wsi, &pss->user, &pss->buffer);
        return -1;
      }
      break;
    }
    case LWS_CALLBACK_CLOSED: {
      lwsl_info("LWS_CALLBACK_CLOSED: wsi %p\n", wsi);

      websocket_session_teardown(wsi, &pss->user, &pss->buffer);
      break;
    }
    case LWS_CALLBACK_SERVER_WRITEABLE: {
      lwsl_info("LWS_CALLBACK_SERVER_WRITEABLE\n");

      auto total = evbuffer_get_length(pss->buffer);

      static unsigned char buf[LWS_PRE + 2048];
      auto numbytes = evbuffer_copyout(pss->buffer, &buf[LWS_PRE], sizeof(buf) - LWS_PRE);
      if (numbytes > 0) {
        auto m = lws_write(wsi, buf + LWS_PRE, numbytes, LWS_WRITE_BINARY);
        // A short return means the connection failed. On success lws consumed
        // the whole payload (partials are buffered and flushed internally), and
        // the return can EXCEED numbytes on TLS -- never use it as a drain
        // count.
        if (m < static_cast<int>(numbytes)) {
          lwsl_warn("ERROR %d writing to ws socket.\n", m);
          return -1;
        }
        evbuffer_drain(pss->buffer, numbytes);
        total = evbuffer_get_length(pss->buffer);
        // May have more text to write.
        if (total > 0) {
          lws_callback_on_writable(wsi);
        }
      }
      break;
    }
    case LWS_CALLBACK_RECEIVE: {
      lwsl_info(
          "LWS_CALLBACK_RECEIVE: %4d (rpp %5d, first %d, "
          "last %d, bin %d, len %zd)\n",
          (int)len, (int)lws_remaining_packet_payload(wsi), lws_is_first_fragment(wsi),
          lws_is_final_fragment(wsi), lws_frame_is_binary(wsi), len);

      if (len <= 0) {
        break;
      }
      auto ip = pss->user;
      if (!ip) {  // we are already disconnected
        return -1;
      }
      on_user_websocket_telnet_received(ip, (const char *)in, len);
      break;
    }
    default:
      lwsl_info("Unknown callback: %d, \n", reason);
      break;
  }

  return 0;
}

void ws_telnet_send(struct lws *wsi, const char *data, size_t len) {
  DEBUG_CHECK(lws_get_protocol(wsi)->id != PROTOCOL_WS_TELNET, "wrong protocol!");
  auto pss = reinterpret_cast<ws_telnet_session *>(lws_wsi_user(wsi));
  DEBUG_CHECK(pss == nullptr, "no session data!");

  evbuffer_add(pss->buffer, data, len);
  lws_callback_on_writable(wsi);
}
