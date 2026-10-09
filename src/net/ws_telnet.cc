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

void on_user_websocket_telnet_received(interactive_t *ip, const char *data, size_t len);

namespace {

/* one of these is created for each vhost our protocol is used with */
struct per_vhost_data {
  struct lws_context *context;
  struct lws_vhost *vhost;
  const struct lws_protocols *protocol;

  WebSocketSession *pss_list; /* linked-list of live pss*/
};

void setup_telnet(interactive_t *ip) {
  ip->telnet = net_telnet_init(ip);
  send_initial_telnet_negotiations(ip);
}

}  // namespace

int ws_telnet_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in,
                      size_t len) {
  auto *pss = reinterpret_cast<WebSocketSession *>(user);
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
    case LWS_CALLBACK_ESTABLISHED:
      if (!websocket_establish_session(wsi, pss, setup_telnet)) {
        return -1;
      }
      break;
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
      if (websocket_maybe_close_after_flush(wsi, pss)) {
        return -1;
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
      if (!ip) {
        // Driver-initiated close is draining the application buffer. Ignore
        // late input instead of aborting the drain.
        break;
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
  auto pss = reinterpret_cast<WebSocketSession *>(lws_wsi_user(wsi));
  DEBUG_CHECK(pss == nullptr, "no session data!");

  evbuffer_add(pss->buffer, data, len);
  lws_callback_on_writable(wsi);
}
