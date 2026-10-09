#include "base/std.h"

#include <event2/buffer.h>
#include <event2/event.h>
#include <event2/util.h>
#include <event2/listener.h>

#include <libwebsockets.h>
#include <cstdlib>

#include "net/ws_ascii.h"
#include "comm.h"
#include "interactive.h"
#include "net/websocket.h"

void on_user_websocket_received(interactive_t *ip, const char *data, size_t len);

namespace {

/* one of these is created for each vhost our protocol is used with */
struct per_vhost_data {
  struct lws_context *context;
  struct lws_vhost *vhost;
  const struct lws_protocols *protocol;

  WebSocketSession *pss_list; /* linked-list of live pss*/
};

}  // namespace

int ws_ascii_callback(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in,
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
      if (!websocket_establish_session(wsi, pss, nullptr)) {
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

      static unsigned char buf[LWS_PRE + 2048];
      auto numbytes = evbuffer_copyout(pss->buffer, &buf[LWS_PRE], sizeof(buf) - LWS_PRE);
      if (numbytes <= 0) {
        if (websocket_maybe_close_after_flush(wsi, pss)) {
          return -1;
        }
        break;
      }
      // Hold back a trailing incomplete UTF-8 sequence so a codepoint is
      // never split across ws messages. evbuffer_copyout() does not drain, so
      // the held-back bytes stay at the front of pss->buffer and go out with
      // the next write; re-prepending them here (the old code did, with a
      // negative length) both duplicated and overflowed. During a
      // close-after-flush drain the user is gone, so no future bytes can
      // complete a partial codepoint; send the bytes that remain.
      if (pss->user) {
        auto new_numbytes = static_cast<ev_ssize_t>(u8_truncate(&buf[LWS_PRE], numbytes));
        if (new_numbytes == 0) {
          // Only an incomplete codepoint is buffered; the exit below keeps a
          // writeable callback requested until the rest of it arrives.
          break;
        }
        numbytes = new_numbytes;
      }
#ifdef DEBUG
      if (!u8_validate(&buf[LWS_PRE], numbytes)) {
        char buf1[sizeof(buf) + 1] = {};
        strncpy(buf1, reinterpret_cast<const char *>(&buf[LWS_PRE]), numbytes);
        debug_message("Illegal UTF8 Websocket output string: %s.", buf1);
      }
#endif
      // TODO: we could use LWS_WRITE_TEXT , however it is much safer to use binary mode, its
      // better to let client deal with incorrect encoding.
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
      // May have more text to write.
      if (evbuffer_get_length(pss->buffer) > 0) {
        lws_callback_on_writable(wsi);
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
      // don't accept binary frame, we want client to always send valid utf8.
      // lws handles the utf8 check for us.
      if (lws_frame_is_binary(wsi)) {
        return -1;
      }
      auto ip = pss->user;
      if (!ip) {
        // Driver-initiated close is draining the application buffer. Ignore
        // late input instead of aborting the drain.
        break;
      }
      on_user_websocket_received(ip, (const char *)in, len);
      break;
    }
    default:
      lwsl_info("Unknown callback: %d, \n", reason);
      break;
  }

  return 0;
}

void ws_ascii_send(struct lws *wsi, const char *data, size_t len) {
  DEBUG_CHECK(lws_get_protocol(wsi)->id != PROTOCOL_WS_ASCII, "wrong protocol!");
  auto pss = reinterpret_cast<WebSocketSession *>(lws_wsi_user(wsi));
  DEBUG_CHECK(pss == nullptr, "no session data!");

  evbuffer_add(pss->buffer, data, len);
  lws_callback_on_writable(wsi);
}
