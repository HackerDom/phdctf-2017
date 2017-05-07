/*
 * Copyright (c) 2014 Cesanta Software Limited
 * All rights reserved
 */

#include <stdarg.h>
#include <stdio.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
#include <sys/time.h>

#include "mongoose.h"

#define DOCUMENT_ROOT "."
#define INDEX_FILE "coffeepot.cgi"
#define CGI_TIMEOUT 10

static struct mg_http_proto_data *mg_http_get_proto_data(
    struct mg_connection *c);
void mg_http_handler(struct mg_connection *nc, int ev, void *ev_data);

static void mg_handle_cgi(struct mg_connection *nc, const char *prog,
                               const struct mg_str *path_info,
                               const struct http_message *hm);
static void mg_http_free_proto_data_cgi(struct mg_http_proto_data *d);
static int mg_get_errno(void);
static double mg_time(void);

static const char *mg_skip(const char *s, const char *end_string,
                    const char *delimiters, struct mg_str *v);
static int mg_stat(const char *path, cs_stat_t *st);
static void mg_set_close_on_exec(sock_t);
static int mg_socket_if_listen_tcp(struct mg_connection *nc,
                            union socket_address *sa);
static void mg_socket_if_tcp_send(struct mg_connection *nc, const void *buf,
                           size_t len);
static void mg_socket_if_destroy_conn(struct mg_connection *nc);
static void mg_send(struct mg_connection *, const void *buf, int len);
static void mg_conn_addr_to_str(struct mg_connection *nc, char *buf, size_t len,
                         int flags);
static void mg_sock_addr_to_str(const union socket_address *sa, char *buf, size_t len,
                         int flags);
static void mbuf_resize(struct mbuf *, size_t new_size);


#define MG_VPRINTF_BUFFER_SIZE 100
#define MBUF_SIZE_MULTIPLIER 1.5

static void cs_log_print_prefix(const char *func) {
  fprintf(stderr, "%-20s ", func);
}

static void cs_log_printf(const char *fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  fputc('\n', stderr);
  fflush(stderr);
}

#define LOG(x)                    \
  do {                               \
    cs_log_print_prefix(__func__); \
    cs_log_printf x;               \
  } while (0)


static double cs_time(void) {
  double now;
  struct timeval tv;
  if (gettimeofday(&tv, NULL /* tz */) != 0) return 0;
  now = (double) tv.tv_sec + (((double) tv.tv_usec) / 1000000.0);
  return now;
}

static void mbuf_init(struct mbuf *mbuf, size_t initial_size) {
  mbuf->len = mbuf->size = 0;
  mbuf->buf = NULL;
  mbuf_resize(mbuf, initial_size);
}

static void mbuf_free(struct mbuf *mbuf) {
  if (mbuf->buf != NULL) {
    free(mbuf->buf);
    mbuf_init(mbuf, 0);
  }
}

static void mbuf_resize(struct mbuf *a, size_t new_size) {
  if (new_size > a->size || (new_size < a->size && new_size >= a->len)) {
    char *buf = (char *) realloc(a->buf, new_size);
    /*
     * In case realloc fails, there's not much we can do, except keep things as
     * they are. Note that NULL is a valid return value from realloc when
     * size == 0, but that is covered too.
     */
    if (buf == NULL && new_size != 0) return;
    a->buf = buf;
    a->size = new_size;
  }
}

static size_t mbuf_insert(struct mbuf *a, size_t off, const void *buf, size_t len) {
  char *p = NULL;

  assert(a != NULL);
  assert(a->len <= a->size);
  assert(off <= a->len);

  /* check overflow */
  if (~(size_t) 0 - (size_t) a->buf < len) return 0;

  if (a->len + len <= a->size) {
    memmove(a->buf + off + len, a->buf + off, a->len - off);
    if (buf != NULL) {
      memcpy(a->buf + off, buf, len);
    }
    a->len += len;
  } else {
    size_t new_size = (size_t)((a->len + len) * MBUF_SIZE_MULTIPLIER);
    if ((p = (char *) realloc(a->buf, new_size)) != NULL) {
      a->buf = p;
      memmove(a->buf + off + len, a->buf + off, a->len - off);
      if (buf != NULL) memcpy(a->buf + off, buf, len);
      a->len += len;
      a->size = new_size;
    } else {
      len = 0;
    }
  }

  return len;
}

static int mg_ncasecmp(const char *s1, const char *s2, size_t len);

static size_t mbuf_append(struct mbuf *a, const void *buf, size_t len) {
  return mbuf_insert(a, a->len, buf, len);
}

static void mbuf_remove(struct mbuf *mb, size_t n) {
  if (n > 0 && n <= mb->len) {
    memmove(mb->buf, mb->buf + n, mb->len - n);
    mb->len -= n;
  }
}

static struct mg_str mg_mk_str(const char *s) {
  struct mg_str ret = {s, 0};
  if (s != NULL) ret.len = strlen(s);
  return ret;
}

static int mg_vcmp(const struct mg_str *str1, const char *str2) {
  size_t n2 = strlen(str2), n1 = str1->len;
  int r = memcmp(str1->p, str2, (n1 < n2) ? n1 : n2);
  if (r == 0) {
    return n1 - n2;
  }
  return r;
}

static int mg_vcasecmp(const struct mg_str *str1, const char *str2) {
  size_t n2 = strlen(str2), n1 = str1->len;
  int r = mg_ncasecmp(str1->p, str2, (n1 < n2) ? n1 : n2);
  if (r == 0) {
    return n1 - n2;
  }
  return r;
}

static int str_util_lowercase(const char *s) {
  return tolower(*(const unsigned char *) s);
}


static int mg_ncasecmp(const char *s1, const char *s2, size_t len) {
  int diff = 0;

  if (len > 0) do {
      diff = str_util_lowercase(s1++) - str_util_lowercase(s2++);
    } while (diff == 0 && s1[-1] != '\0' && --len > 0);

  return diff;
}

static int mg_avprintf(char **buf, size_t size, const char *fmt, va_list ap) {
  va_list ap_copy;
  int len;

  va_copy(ap_copy, ap);
  len = vsnprintf(*buf, size, fmt, ap_copy);
  va_end(ap_copy);

  if (len < 0) {
    *buf = NULL; /* LCOV_EXCL_START */
    while (len < 0) {
      free(*buf);
      size *= 2;
      if ((*buf = (char *) malloc(size)) == NULL) break;
      va_copy(ap_copy, ap);
      len = vsnprintf(*buf, size, fmt, ap_copy);
      va_end(ap_copy);
    }
    /* LCOV_EXCL_STOP */
  } else if (len >= (int) size) {
    /* Standard-compliant code path. Allocate a buffer that is large enough. */
    if ((*buf = (char *) malloc(len + 1)) == NULL) {
      len = -1; /* LCOV_EXCL_LINE */
    } else {    /* LCOV_EXCL_LINE */
      va_copy(ap_copy, ap);
      len = vsnprintf(*buf, len + 1, fmt, ap_copy);
      va_end(ap_copy);
    }
  }

  return len;
}

static void mg_add_conn(struct mg_mgr *mgr, struct mg_connection *c) {
  c->mgr = mgr;
  c->next = mgr->active_connections;
  mgr->active_connections = c;
  c->prev = NULL;
  if (c->next != NULL) c->next->prev = c;
}

static void mg_remove_conn(struct mg_connection *conn) {
  if (conn->prev == NULL) conn->mgr->active_connections = conn->next;
  if (conn->prev) conn->prev->next = conn->next;
  if (conn->next) conn->next->prev = conn->prev;
}

static void mg_call(struct mg_connection *nc,
                         mg_event_handler_t ev_handler, int ev, void *ev_data) {
  if (ev_handler == NULL) {
    ev_handler = nc->proto_handler ? nc->proto_handler : nc->handler;
  }

  if (ev_handler != NULL) {
    ev_handler(nc, ev, ev_data);
  }
}

static void mg_if_timer(struct mg_connection *c, double now) {
  if (c->ev_timer_time > 0 && now >= c->ev_timer_time) {
    double old_value = c->ev_timer_time;
    mg_call(c, NULL, MG_EV_TIMER, &now);
    /*
     * To prevent timer firing all the time, reset the timer after delivery.
     * However, in case user sets it to new value, do not reset.
     */
    if (c->ev_timer_time == old_value) {
      c->ev_timer_time = 0;
    }
  }
}

static void mg_destroy_conn(struct mg_connection *conn) {
  mg_socket_if_destroy_conn(conn);
  if (conn->proto_data != NULL && conn->proto_data_destructor != NULL) {
    conn->proto_data_destructor(conn->proto_data);
  }
  mbuf_free(&conn->recv_mbuf);
  mbuf_free(&conn->send_mbuf);

  memset(conn, 0, sizeof(*conn));

  free(conn);
}

void mg_mgr_init(struct mg_mgr *m) {
  m->active_connections = NULL;

  signal(SIGPIPE, SIG_IGN);
  signal(SIGCHLD, SIG_IGN);
}

void mg_mgr_free(struct mg_mgr *m) {
  struct mg_connection *conn, *tmp_conn;

  if (m == NULL) return;
  /* Do one last poll, see https://github.com/cesanta/mongoose/issues/286 */
  mg_mgr_poll(m, 0);

  for (conn = m->active_connections; conn != NULL; conn = tmp_conn) {
    tmp_conn = conn->next;
    mg_call(conn, NULL, MG_EV_CLOSE, NULL);
    mg_remove_conn(conn);
    mg_destroy_conn(conn);
  }
}

static int mg_vprintf(struct mg_connection *nc, const char *fmt, va_list ap) {
  char mem[MG_VPRINTF_BUFFER_SIZE], *buf = mem;
  int len;

  if ((len = mg_avprintf(&buf, sizeof(mem), fmt, ap)) > 0) {
    mg_send(nc, buf, len);
  }
  if (buf != mem && buf != NULL) {
    free(buf); /* LCOV_EXCL_LINE */
  }               /* LCOV_EXCL_LINE */

  return len;
}

static int mg_printf(struct mg_connection *conn, const char *fmt, ...) {
  int len;
  va_list ap;
  va_start(ap, fmt);
  len = mg_vprintf(conn, fmt, ap);
  va_end(ap);
  return len;
}

static struct mg_connection *mg_create_connection(
  struct mg_mgr *mgr, mg_event_handler_t callback) {
  struct mg_connection *conn;

  if ((conn = (struct mg_connection *) calloc(1, sizeof(*conn))) != NULL) {
    conn->sock = INVALID_SOCKET;
    conn->handler = callback;
    conn->mgr = mgr;
    conn->last_io_time = (time_t) mg_time();
    conn->flags = 0;
    conn->user_data = NULL;
    /*
     * SIZE_MAX is defined as a long long constant in
     * system headers on some platforms and so it
     * doesn't compile with pedantic ansi flags.
     */
    conn->recv_mbuf_limit = ~0;
  } else {
    LOG(("failed to create connection"));
  }

  return conn;
}

/*
 * Address format: [PROTO://][HOST]:PORT
 *
 * HOST could be IPv4/IPv6 address or a host name.
 * `host` is a destination buffer to hold parsed HOST part. Shoud be at least
 * MG_MAX_HOST_LEN bytes long.
 * `proto` is a returned socket type, either SOCK_STREAM or SOCK_DGRAM
 *
 * Return:
 *   -1   on parse error
 *    0   if HOST needs DNS lookup
 *   >0   length of the address string
 */
static int mg_parse_address(const char *str, union socket_address *sa) {
  unsigned int a, b, c, d, port = 0;
  int ch, len = 0;
  /*
   * MacOS needs that. If we do not zero it, subsequent bind() will fail.
   * Also, all-zeroes in the socket address means binding to all addresses
   * for both IPv4 and IPv6 (INADDR_ANY and IN6ADDR_ANY_INIT).
   */
  memset(sa, 0, sizeof(*sa));
  sa->sin.sin_family = AF_INET;

  if (sscanf(str, "%u.%u.%u.%u:%u%n", &a, &b, &c, &d, &port, &len) == 5) {
    /* Bind to a specific IPv4 address, e.g. 192.168.1.5:8080 */
    sa->sin.sin_addr.s_addr =
        htonl(((uint32_t) a << 24) | ((uint32_t) b << 16) | c << 8 | d);
    sa->sin.sin_port = htons((uint16_t) port);
  } else if (sscanf(str, ":%u%n", &port, &len) == 1 ||
             sscanf(str, "%u%n", &port, &len) == 1) {
    /* If only port is specified, bind to IPv4, INADDR_ANY */
    sa->sin.sin_port = htons((uint16_t) port);
  } else {
    return -1;
  }

  ch = str[len]; /* Character that follows the address */
  return port < 0xffffUL && (ch == '\0' || ch == ',' || isspace(ch)) ? len : -1;
}

struct mg_connection *mg_if_accept_new_conn(struct mg_connection *lc) {
  struct mg_connection *nc;
  nc = mg_create_connection(lc->mgr, lc->handler);
  if (nc == NULL) return NULL;
  nc->listener = lc;
  nc->proto_handler = lc->proto_handler;
  nc->user_data = lc->user_data;
  nc->recv_mbuf_limit = lc->recv_mbuf_limit;
  mg_add_conn(nc->mgr, nc);
  return nc;
}

static void mg_if_accept_tcp_cb(struct mg_connection *nc, union socket_address *sa,
                         size_t sa_len) {
  (void) sa_len;
  nc->sa = *sa;
  mg_call(nc, NULL, MG_EV_ACCEPT, &nc->sa);
}

static void mg_send(struct mg_connection *nc, const void *buf, int len) {
  nc->last_io_time = (time_t) mg_time();
  mg_socket_if_tcp_send(nc, buf, len);
}

static void mg_if_sent_cb(struct mg_connection *nc, int num_sent) {
  if (num_sent < 0) {
    nc->flags |= MG_F_CLOSE_IMMEDIATELY;
  }
  mg_call(nc, NULL, MG_EV_SEND, &num_sent);
}

static void mg_recv_common(struct mg_connection *nc, void *buf, int len,
                                int own) {
  if (nc->flags & MG_F_CLOSE_IMMEDIATELY) {
    /*
     * This connection will not survive next poll. Do not deliver events,
     * send data to /dev/null without acking.
     */
    if (own) {
      free(buf);
    }
    return;
  }
  nc->last_io_time = (time_t) mg_time();
  if (!own) {
    mbuf_append(&nc->recv_mbuf, buf, len);
  } else if (nc->recv_mbuf.len == 0) {
    /* Adopt buf as recv_mbuf's backing store. */
    mbuf_free(&nc->recv_mbuf);
    nc->recv_mbuf.buf = (char *) buf;
    nc->recv_mbuf.size = nc->recv_mbuf.len = len;
  } else {
    mbuf_append(&nc->recv_mbuf, buf, len);
    free(buf);
  }
  mg_call(nc, NULL, MG_EV_RECV, &len);
}

struct mg_connection *mg_bind(struct mg_mgr *srv, const char *address,
                              mg_event_handler_t event_handler) {
  union socket_address sa;
  struct mg_connection *nc = NULL;
  int rc;

  if (mg_parse_address(address, &sa) <= 0) {
    LOG(("cannot parse address"));
    return NULL;
  }

  nc = mg_create_connection(srv, event_handler);
  if (nc == NULL) {
    return NULL;
  }

  nc->sa = sa;
  nc->flags |= MG_F_LISTENING;
  nc->proto_handler = mg_http_handler;

  rc = mg_socket_if_listen_tcp(nc, &nc->sa);
  if (rc != 0) {
    LOG(("Failed to open listener: %d", rc));
    mg_destroy_conn(nc);
    return NULL;
  }
  mg_add_conn(nc->mgr, nc);

  return nc;
}

/* Move data from one connection to another */
static void mg_forward(struct mg_connection *from, struct mg_connection *to) {
  mg_send(to, from->recv_mbuf.buf, from->recv_mbuf.len);
  mbuf_remove(&from->recv_mbuf, from->recv_mbuf.len);
}

static void mg_set_non_blocking_mode(sock_t sock) {
  int flags = fcntl(sock, F_GETFL, 0);
  fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

/* Associate a socket to a connection. */
static void mg_sock_set(struct mg_connection *nc, sock_t sock) {
  if (sock == INVALID_SOCKET) {
    return;
  }
  mg_set_non_blocking_mode(sock);
  mg_set_close_on_exec(sock);
  nc->sock = sock;
}

struct mg_connection *mg_add_sock(struct mg_mgr *s, sock_t sock,
                                  mg_event_handler_t callback) {
  struct mg_connection *nc = mg_create_connection(s, callback);
  if (nc != NULL) {
    mg_sock_set(nc, sock);
    mg_add_conn(nc->mgr, nc);
  }
  return nc;
}

static double mg_time(void) {
  return cs_time();
}

#define MG_TCP_RECV_BUFFER_SIZE 8192

static sock_t mg_open_listening_socket(union socket_address *sa, int type,
                                       int proto);

static int mg_is_error(int n) {
  int err = mg_get_errno();
  return (n < 0 && err != EINPROGRESS && err != EWOULDBLOCK
          && err != EAGAIN && err != EINTR);
}

static int mg_socket_if_listen_tcp(struct mg_connection *nc,
                            union socket_address *sa) {
  int proto = 0;
  sock_t sock = mg_open_listening_socket(sa, SOCK_STREAM, proto);
  if (sock == INVALID_SOCKET) {
    return (mg_get_errno() ? mg_get_errno() : 1);
  }
  mg_sock_set(nc, sock);
  return 0;
}

static void mg_socket_if_tcp_send(struct mg_connection *nc, const void *buf,
                           size_t len) {
  mbuf_append(&nc->send_mbuf, buf, len);
}

static void mg_socket_if_destroy_conn(struct mg_connection *nc) {
  if (nc->sock == INVALID_SOCKET) return;
  close(nc->sock);
  nc->sock = INVALID_SOCKET;
}

static int mg_accept_conn(struct mg_connection *lc) {
  struct mg_connection *nc;
  union socket_address sa;
  socklen_t sa_len = sizeof(sa);
  sock_t sock = accept(lc->sock, &sa.sa, &sa_len);
  if (sock == INVALID_SOCKET) {
    if (mg_is_error(-1)) LOG(("failed to accept: %d", mg_get_errno()));
    return 0;
  }
  nc = mg_if_accept_new_conn(lc);
  if (nc == NULL) {
    close(sock);
    return 0;
  }
  LOG(("conn from %s:%d", inet_ntoa(sa.sin.sin_addr),
       ntohs(sa.sin.sin_port)));
  mg_sock_set(nc, sock);
  {
    mg_if_accept_tcp_cb(nc, &sa, sa_len);
  }
  return 1;
}

/* 'sa' must be an initialized address to bind to */
static sock_t mg_open_listening_socket(union socket_address *sa, int type,
                                       int proto) {
  socklen_t sa_len =
      (sa->sa.sa_family == AF_INET) ? sizeof(sa->sin) : sizeof(sa->sin6);
  sock_t sock = INVALID_SOCKET;
  int on = 1;

  if ((sock = socket(sa->sa.sa_family, type, proto)) != INVALID_SOCKET &&
      !setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (void *) &on, sizeof(on)) &&
      !bind(sock, &sa->sa, sa_len) && listen(sock, SOMAXCONN) == 0) {
    mg_set_non_blocking_mode(sock);
    /* In case port was set to 0, get the real port number */
    (void) getsockname(sock, &sa->sa, &sa_len);
  } else if (sock != INVALID_SOCKET) {
    close(sock);
    sock = INVALID_SOCKET;
  }

  return sock;
}

static void mg_write_to_socket(struct mg_connection *nc) {
  struct mbuf *io = &nc->send_mbuf;
  int n = 0;

  assert(io->len > 0);

  {
    n = (int) send(nc->sock, io->buf, io->len, 0);
    if (n < 0 && mg_is_error(n)) {
      /* Something went wrong, drop the connection. */
      nc->flags |= MG_F_CLOSE_IMMEDIATELY;
      return;
    }
  }

  if (n > 0) {
    mbuf_remove(io, n);
    mg_if_sent_cb(nc, n);
  }
}

static size_t recv_avail_size(struct mg_connection *conn, size_t max) {
  size_t avail;
  if (conn->recv_mbuf_limit < conn->recv_mbuf.len) return 0;
  avail = conn->recv_mbuf_limit - conn->recv_mbuf.len;
  return avail > max ? max : avail;
}

static void mg_handle_tcp_read(struct mg_connection *conn) {
  int n = 0;
  char *buf = (char *) malloc(MG_TCP_RECV_BUFFER_SIZE);

  if (buf == NULL) {
    LOG(("OOM"));
    return;
  }

  {
    n = (int) recv(conn->sock, buf,
                           recv_avail_size(conn, MG_TCP_RECV_BUFFER_SIZE), 0);
    if (n > 0) {
      mg_recv_common(conn, buf, n, 1 /* own */);
    } else {
      free(buf);
    }
    if (n == 0) {
      /* Orderly shutdown of the socket, try flushing output. */
      // HACK: actually it is a half-close
      mg_call(conn, NULL, MG_EV_CLOSE, NULL); 
      conn->flags |= MG_F_SEND_AND_CLOSE;
    } else if (mg_is_error(n)) {
      conn->flags |= MG_F_CLOSE_IMMEDIATELY;
    }
  }
}

#define _MG_F_FD_CAN_READ 1
#define _MG_F_FD_CAN_WRITE 1 << 1
#define _MG_F_FD_ERROR 1 << 2

static void mg_mgr_handle_conn(struct mg_connection *nc, int fd_flags, double now) {
  if (fd_flags & _MG_F_FD_CAN_READ) {
    if (nc->flags & MG_F_LISTENING) {
      /*
       * We're not looping here, and accepting just one connection at
       * a time. The reason is that eCos does not respect non-blocking
       * flag on a listening socket and hangs in a loop.
       */
      mg_accept_conn(nc);
    } else {
      mg_handle_tcp_read(nc);
    }
  }

  if (!(nc->flags & MG_F_CLOSE_IMMEDIATELY)) {
    if ((fd_flags & _MG_F_FD_CAN_WRITE) && nc->send_mbuf.len > 0) {
      mg_write_to_socket(nc);
    }
    mg_call(nc, NULL, MG_EV_POLL, &now);
    mg_if_timer(nc, now);
  }
}

static void mg_add_to_set(sock_t sock, fd_set *set, sock_t *max_fd) {
  if (sock != INVALID_SOCKET && sock < FD_SETSIZE) {
    FD_SET(sock, set);
    if (*max_fd == INVALID_SOCKET || sock > *max_fd) {
      *max_fd = sock;
    }
  }
}

time_t mg_mgr_poll(struct mg_mgr *mgr, int timeout_ms) {
  double now = mg_time();
  double min_timer;
  struct mg_connection *nc, *tmp;
  struct timeval tv;
  fd_set read_set, write_set, err_set;
  sock_t max_fd = INVALID_SOCKET;
  int num_fds, num_ev, num_timers = 0;
  int try_dup = 1;

  FD_ZERO(&read_set);
  FD_ZERO(&write_set);
  FD_ZERO(&err_set);

  /*
   * Note: it is ok to have connections with sock == INVALID_SOCKET in the list,
   * e.g. timer-only "connections".
   */
  min_timer = 0;
  for (nc = mgr->active_connections, num_fds = 0; nc != NULL; nc = tmp) {
    tmp = nc->next;

    if (nc->sock != INVALID_SOCKET) {
      num_fds++;

      /* A hack to make sure all our file descriptos fit into FD_SETSIZE. */
      if (nc->sock >= FD_SETSIZE && try_dup) {
        int new_sock = dup(nc->sock);
        if (new_sock >= 0 && new_sock < FD_SETSIZE) {
          close(nc->sock);
          LOG(("new sock %d -> %d", nc->sock, new_sock));
          nc->sock = new_sock;
        } else {
          try_dup = 0;
        }
      }

      if (!(nc->flags & MG_F_SEND_AND_CLOSE) &&
        nc->recv_mbuf.len < nc->recv_mbuf_limit) {
        mg_add_to_set(nc->sock, &read_set, &max_fd);
      }

      if (nc->send_mbuf.len > 0) {
        mg_add_to_set(nc->sock, &write_set, &max_fd);
        mg_add_to_set(nc->sock, &err_set, &max_fd);
      }
    }

    if (nc->ev_timer_time > 0) {
      if (num_timers == 0 || nc->ev_timer_time < min_timer) {
        min_timer = nc->ev_timer_time;
      }
      num_timers++;
    }
  }

  /*
   * If there is a timer to be fired earlier than the requested timeout,
   * adjust the timeout.
   */
  if (num_timers > 0) {
    double timer_timeout_ms = (min_timer - mg_time()) * 1000 + 1 /* rounding */;
    if (timer_timeout_ms < timeout_ms) {
      timeout_ms = (int) timer_timeout_ms;
    }
  }
  if (timeout_ms < 0) timeout_ms = 0;

  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;

  num_ev = select((int) max_fd + 1, &read_set, &write_set, &err_set, &tv);
  now = mg_time();

  for (nc = mgr->active_connections; nc != NULL; nc = tmp) {
    int fd_flags = 0;
    if (nc->sock != INVALID_SOCKET) {
      if (num_ev > 0) {
        fd_flags = (FD_ISSET(nc->sock, &read_set) ? _MG_F_FD_CAN_READ : 0) |
                   (FD_ISSET(nc->sock, &write_set) ? _MG_F_FD_CAN_WRITE : 0) |
                   (FD_ISSET(nc->sock, &err_set) ? _MG_F_FD_ERROR : 0);
      }
    }
    tmp = nc->next;
    mg_mgr_handle_conn(nc, fd_flags, now);
  }

  for (nc = mgr->active_connections; nc != NULL; nc = tmp) {
    tmp = nc->next;
    if (nc->flags & MG_F_CLOSE_IMMEDIATELY) {
      mg_call(nc, NULL, MG_EV_CLOSE, NULL);
      mg_remove_conn(nc);
      mg_destroy_conn(nc);
    } else if (nc->send_mbuf.len == 0 && (nc->flags & MG_F_SEND_AND_CLOSE)) {
      struct mg_http_proto_data *pd;
      pd = mg_http_get_proto_data(nc);

      if(pd->cgi_nc == NULL) {
        mg_remove_conn(nc);
        mg_destroy_conn(nc);
      }
    }
  }

  return (time_t) now;
}

static int mg_socketpair(sock_t sp[2]) {
  union socket_address sa;
  sock_t sock;
  socklen_t len = sizeof(sa.sin);
  int ret = 0;

  sock = sp[0] = sp[1] = INVALID_SOCKET;

  (void) memset(&sa, 0, sizeof(sa));
  sa.sin.sin_family = AF_INET;
  sa.sin.sin_port = htons(0);
  sa.sin.sin_addr.s_addr = htonl(0x7f000001); /* 127.0.0.1 */

  if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
  } else if (bind(sock, &sa.sa, len) != 0) {
  } else if (listen(sock, 1) != 0) {
  } else if (getsockname(sock, &sa.sa, &len) != 0) {
  } else if ((sp[0] = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
  } else if (connect(sp[0], &sa.sa, len) != 0) {
  } else if ((sp[1] = (accept(sock, &sa.sa, &len))) == INVALID_SOCKET) {
  } else {
    mg_set_close_on_exec(sp[0]);
    mg_set_close_on_exec(sp[1]);
    close(sock);
    ret = 1;
  }

  if (!ret) {
    if (sp[0] != INVALID_SOCKET) close(sp[0]);
    if (sp[1] != INVALID_SOCKET) close(sp[1]);
    if (sock != INVALID_SOCKET) close(sock);
    sock = sp[0] = sp[1] = INVALID_SOCKET;
  }

  return ret;
}

static void mg_sock_get_addr(sock_t sock, int remote,
                             union socket_address *sa) {
  socklen_t slen = sizeof(*sa);
  memset(sa, 0, slen);
  if (remote) {
    getpeername(sock, &sa->sa, &slen);
  } else {
    getsockname(sock, &sa->sa, &slen);
  }
}

static void mg_sock_to_str(sock_t sock, char *buf, size_t len, int flags) {
  union socket_address sa;
  mg_sock_get_addr(sock, flags & MG_SOCK_STRINGIFY_REMOTE, &sa);
  mg_sock_addr_to_str(&sa, buf, len, flags);
}

static void mg_if_get_conn_addr(struct mg_connection *nc, int remote,
                                union socket_address *sa) {
  mg_sock_get_addr(nc->sock, remote, sa);
}

/*
 * scan string until `sep`, keeping track of component boundaries in `res`.
 *
 * `p` will point to the char after the separator or it will be `end`.
 */
static void parse_uri_component(const char **p, const char *end, char sep,
                                struct mg_str *res) {
  res->p = *p;
  for (; *p < end; (*p)++) {
    if (**p == sep) {
      break;
    }
  }
  res->len = (*p) - res->p;
  if (*p < end) (*p)++;
}


/* Normalize the URI path. Remove/resolve "." and "..". */
static int mg_normalize_uri_path(const struct mg_str *in, struct mg_str *out) {
  const char *s = in->p, *se = s + in->len;
  char *cp = (char *) out->p, *d;

  if (in->len == 0 || *s != '/') {
    out->len = 0;
    return 0;
  }

  d = cp;

  while (s < se) {
    const char *next = s;
    struct mg_str component;
    parse_uri_component(&next, se, '/', &component);
    if (mg_vcmp(&component, ".") == 0) {
      /* Yum. */
    } else if (mg_vcmp(&component, "..") == 0) {
      /* Backtrack to previous slash. */
      if (d > cp + 1 && *(d - 1) == '/') d--;
      while (d > cp && *(d - 1) != '/') d--;
    } else {
      memmove(d, s, next - s);
      d += next - s;
    }
    s = next;
  }
  if (d == cp) *d++ = '/';

  out->p = cp;
  out->len = d - cp;
  return 1;
}

static const char *mg_version_header = "Mongoose/" MG_VERSION;

static void mg_http_conn_destructor(void *proto_data) {
  struct mg_http_proto_data *pd = (struct mg_http_proto_data *) proto_data;
  mg_http_free_proto_data_cgi(pd);
  free(proto_data);
}

static struct mg_http_proto_data *mg_http_get_proto_data(
    struct mg_connection *c) {
  if (c->proto_data == NULL) {
    c->proto_data = calloc(1, sizeof(struct mg_http_proto_data));
    c->proto_data_destructor = mg_http_conn_destructor;
  }

  return (struct mg_http_proto_data *) c->proto_data;
}

/*
 * Check whether full request is buffered. Return:
 *   -1  if request is malformed
 *    0  if request is not yet fully buffered
 *   >0  actual request length, including last \r\n\r\n
 */
static int mg_http_get_request_len(const char *s, int buf_len) {
  const unsigned char *buf = (unsigned char *) s;
  int i;

  for (i = 0; i < buf_len; i++) {
    if (!isprint(buf[i]) && buf[i] != '\r' && buf[i] != '\n' && buf[i] < 128) {
      return -1;
    } else if (buf[i] == '\n' && i + 1 < buf_len && buf[i + 1] == '\n') {
      return i + 2;
    } else if (buf[i] == '\n' && i + 2 < buf_len && buf[i + 1] == '\r' &&
               buf[i + 2] == '\n') {
      return i + 3;
    }
  }

  return 0;
}

static const char *mg_http_parse_headers(const char *s, const char *end,
                                         int len, struct http_message *req) {
  int i = 0;
  while (i < (int) ARRAY_SIZE(req->header_names) - 1) {
    struct mg_str *k = &req->header_names[i], *v = &req->header_values[i];

    s = mg_skip(s, end, ": ", k);
    s = mg_skip(s, end, "\r\n", v);

    while (v->len > 0 && v->p[v->len - 1] == ' ') {
      v->len--; /* Trim trailing spaces in header value */
    }

    /*
     * If header value is empty - skip it and go to next (if any).
     * NOTE: Do not add it to headers_values because such addition changes API
     * behaviour
     */
    if (k->len != 0 && v->len == 0) {
      continue;
    }

    if (k->len == 0 || v->len == 0) {
      k->p = v->p = NULL;
      k->len = v->len = 0;
      break;
    }

    if (!mg_ncasecmp(k->p, "Content-Length", 14)) {
      req->body.len = (size_t) strtoll(v->p, NULL, 10);
      req->message.len = len + req->body.len;
    }

    i++;
  }

  return s;
}

static int mg_parse_http(const char *s, int n, struct http_message *hm) {
  const char *end, *qs;
  int len = mg_http_get_request_len(s, n);

  if (len <= 0) return len;

  memset(hm, 0, sizeof(*hm));
  hm->message.p = s;
  hm->body.p = s + len;
  hm->message.len = hm->body.len = (size_t) ~0;
  end = s + len;

  /* Request is fully buffered. Skip leading whitespaces. */
  while (s < end && isspace(*(unsigned char *) s)) s++;

  /* Parse request line: method, URI, proto */
  s = mg_skip(s, end, " ", &hm->method);
  s = mg_skip(s, end, " ", &hm->uri);
  s = mg_skip(s, end, "\r\n", &hm->proto);
  if (hm->uri.p <= hm->method.p || hm->proto.p <= hm->uri.p) return -1;

  /* If URI contains '?' character, initialize query_string */
  if ((qs = (char *) memchr(hm->uri.p, '?', hm->uri.len)) != NULL) {
    hm->query_string.p = qs + 1;
    hm->query_string.len = &hm->uri.p[hm->uri.len] - (qs + 1);
    hm->uri.len = qs - hm->uri.p;
  }

  s = mg_http_parse_headers(s, end, len, hm);

  /*
   * mg_parse_http() is used to parse both HTTP requests and HTTP
   * responses. If HTTP response does not have Content-Length set, then
   * body is read until socket is closed, i.e. body.len is infinite (~0).
   *
   * For HTTP requests though, according to
   * http://tools.ietf.org/html/rfc7231#section-8.1.3,
   * only POST and PUT methods have defined body semantics.
   * Therefore, if Content-Length is not specified and methods are
   * not one of PUT or POST, set body length to 0.
   *
   * So,
   * if it is HTTP request, and Content-Length is not set,
   * and method is not (PUT or POST) then reset body length to zero.
   */
  if (hm->body.len == (size_t) ~0 &&
      mg_vcasecmp(&hm->method, "PUT") != 0 &&
      mg_vcasecmp(&hm->method, "POST") != 0) {
    hm->body.len = 0;
    hm->message.len = len;
  }

  return len;
}

static struct mg_str *mg_get_http_header(struct http_message *hm, const char *name) {
  size_t i, len = strlen(name);

  for (i = 0; hm->header_names[i].len > 0; i++) {
    struct mg_str *h = &hm->header_names[i], *v = &hm->header_values[i];
    if (h->p != NULL && h->len == len && !mg_ncasecmp(h->p, name, len))
      return v;
  }

  return NULL;
}

void mg_http_handler(struct mg_connection *nc, int ev, void *ev_data) {
  struct http_message shm;
  struct http_message *hm = &shm;
  struct mbuf *io = &nc->recv_mbuf;
  int req_len;
  if (ev == MG_EV_CLOSE) {
    if (io->len > 0 && mg_parse_http(io->buf, io->len, hm) > 0) {
      /*
      * For HTTP messages without Content-Length, always send HTTP message
      * before MG_EV_CLOSE message.
      */
      int ev2 = MG_EV_HTTP_REQUEST;
      hm->message.len = io->len;
      hm->body.len = io->buf + io->len - hm->body.p;
      mg_call(nc, nc->handler, ev2, hm);
    }
  }

  mg_call(nc, nc->handler, ev, ev_data);

  if (ev == MG_EV_RECV) {
    req_len = mg_parse_http(io->buf, io->len, hm);

    /* TODO(alashkin): refactor this ifelseifelseifelseifelse */
    if ((req_len < 0 ||
         (req_len == 0 && io->len >= MG_MAX_HTTP_REQUEST_SIZE))) {
      LOG(("invalid request"));
      nc->flags |= MG_F_CLOSE_IMMEDIATELY;
    } else if (req_len == 0) {
      /* Do nothing, request is not yet fully buffered */
    }
    else if (hm->message.len <= io->len) {
      int trigger_ev = nc->listener ? MG_EV_HTTP_REQUEST : MG_EV_HTTP_REPLY;

      /* Whole HTTP message is fully buffered, call event handler */
      mg_call(nc, nc->handler, trigger_ev, hm);
      mbuf_remove(io, hm->message.len);
    }
  }
}

static const char *mg_status_message(int status_code) {
  switch (status_code) {
    case 400:
      return "Bad Request";
    case 404:
      return "Not Found";
    case 418:
      return "I'm a teapot";
    case 500:
      return "Internal Server Error";

    default:
      return "OK";
  }
}

static void mg_send_response_line(struct mg_connection *nc, int status_code,
                             const struct mg_str extra_headers) {
  mg_printf(nc, "HTTP/1.1 %d %s\r\nServer: %s\r\n", status_code,
            mg_status_message(status_code), mg_version_header);
  if (extra_headers.len > 0) {
    mg_printf(nc, "%.*s\r\n", (int) extra_headers.len, extra_headers.p);
  }
}

static void mg_send_head(struct mg_connection *c, int status_code,
                  int64_t content_length, const char *extra_headers) {
  mg_send_response_line(c, status_code, mg_mk_str(extra_headers));
  mg_printf(c, "Content-Length: %ld\r\n", content_length);
  mg_send(c, "\r\n", 2);
}

static void mg_http_send_error(struct mg_connection *nc, int code,
                        const char *reason) {
  if (!reason) reason = mg_status_message(code);
  LOG(("%d %s", code, reason));
  mg_send_head(nc, code, strlen(reason),
               "Content-Type: text/plain\r\nConnection: close");
  mg_send(nc, reason, strlen(reason));
  nc->flags |= MG_F_SEND_AND_CLOSE;
}

static int mg_url_decode(const char *src, int src_len, char *dst, int dst_len,
                  int is_form_url_encoded) {
  int i, j, a, b;
#define HEXTOI(x) (isdigit(x) ? x - '0' : x - 'W')

  for (i = j = 0; i < src_len && j < dst_len - 1; i++, j++) {
    if (src[i] == '%') {
      if (i < src_len - 2 && isxdigit(*(const unsigned char *) (src + i + 1)) &&
          isxdigit(*(const unsigned char *) (src + i + 2))) {
        a = tolower(*(const unsigned char *) (src + i + 1));
        b = tolower(*(const unsigned char *) (src + i + 2));
        dst[j] = (char) ((HEXTOI(a) << 4) | HEXTOI(b));
        i += 2;
      } else {
        return -1;
      }
    } else if (is_form_url_encoded && src[i] == '+') {
      dst[j] = ' ';
    } else {
      dst[j] = src[i];
    }
  }

  dst[j] = '\0'; /* Null-terminate the destination */

  return i >= src_len ? j : -1;
}

static int mg_uri_to_local_path(struct http_message *hm,
                                     char **local_path,
                                     struct mg_str *remainder) {
  int ok = 1;
  const char *cp = hm->uri.p, *cp_end = hm->uri.p + hm->uri.len;
  struct mg_str root = {NULL, 0};
  const char *file_uri_start = cp;
  *local_path = NULL;
  remainder->p = NULL;
  remainder->len = 0;

  { /* 1. Determine which root to use. */

    root.p = DOCUMENT_ROOT;
    root.len = strlen(DOCUMENT_ROOT);
    assert(root.p != NULL && root.len > 0);
  }

  { /* 2. Find where in the canonical URI path the local path ends. */
    const char *u = file_uri_start + 1;
    char *lp = (char *) malloc(root.len + hm->uri.len + 1);
    char *lp_end = lp + root.len + hm->uri.len + 1;
    char *p = lp, *ps;
    int exists = 1;
    if (lp == NULL) {
      ok = 0;
      goto out;
    }
    memcpy(p, root.p, root.len);
    p += root.len;
    if (*(p - 1) == DIRSEP) p--;
    *p = '\0';
    ps = p;

    /* Chop off URI path components one by one and build local path. */
    while (u <= cp_end) {
      const char *next = u;
      struct mg_str component;
      if (exists) {
        cs_stat_t st;
        exists = (mg_stat(lp, &st) == 0);
        if (exists && S_ISREG(st.st_mode)) {
          /* We found the terminal, the rest of the URI (if any) is path_info.
           */
          if (*(u - 1) == '/') u--;
          break;
        }
      }
      if (u >= cp_end) break;
      parse_uri_component((const char **) &next, cp_end, '/', &component);
      if (component.len > 0) {
        int len;
        memmove(p + 1, component.p, component.len);
        len = mg_url_decode(p + 1, component.len, p + 1, lp_end - p - 1, 0);
        if (len <= 0) {
          ok = 0;
          break;
        }
        component.p = p + 1;
        component.len = len;
        if (mg_vcmp(&component, ".") == 0) {
          /* Yum. */
        } else if (mg_vcmp(&component, "..") == 0) {
          while (p > ps && *p != DIRSEP) p--;
          *p = '\0';
        } else {
          size_t i;
          *p++ = DIRSEP;
          /* No NULs and DIRSEPs in the component (percent-encoded). */
          for (i = 0; i < component.len; i++, p++) {
            if (*p == '\0' || *p == DIRSEP
                ) {
              ok = 0;
              break;
            }
          }
        }
      }
      u = next;
    }
    if (ok) {
      *local_path = lp;
      if (u > cp_end) u = cp_end;
      remainder->p = u;
      remainder->len = cp_end - u;
    } else {
      free(lp);
    }
  }

out:
  return ok;
}

void mg_serve_http(struct mg_connection *nc, struct http_message *hm) {
  char *path = NULL;
  struct mg_str path_info;

  /* Normalize path - resolve "." and ".." (in-place). */
  if (!mg_normalize_uri_path(&hm->uri, &hm->uri)) {
    mg_http_send_error(nc, 400, NULL);
    return;
  }
  if (mg_uri_to_local_path(hm, &path, &path_info) == 0) {
    mg_http_send_error(nc, 404, NULL);
    return;
  }

  char *index_file = INDEX_FILE;
  mg_handle_cgi(nc, index_file ? index_file : path, &path_info, hm);

  free(path);
  path = NULL;

  nc->flags |= MG_F_SEND_AND_CLOSE;
}

#define MG_MAX_CGI_ENVIR_VARS 64
#define MG_ENV_EXPORT_TO_CGI "MONGOOSE_CGI"

/*
 * This structure helps to create an environment for the spawned CGI program.
 * Environment is an array of "VARIABLE=VALUE\0" ASCIIZ strings,
 * last element must be NULL.
 * However, on Windows there is a requirement that all these VARIABLE=VALUE\0
 * strings must reside in a contiguous buffer. The end of the buffer is
 * marked by two '\0' characters.
 * We satisfy both worlds: we create an envp array (which is vars), all
 * entries are actually pointers inside buf.
 */
struct mg_cgi_env_block {
  struct mg_connection *nc;
  char buf[MG_CGI_ENVIRONMENT_SIZE];       /* Environment buffer */
  const char *vars[MG_MAX_CGI_ENVIR_VARS]; /* char *envp[] */
  int len;                                 /* Space taken */
  int nvars;                               /* Number of variables in envp[] */
};

static int mg_start_process(const char *interp, const char *cmd,
                            const char *env, const char *envp[],
                            const char *dir, sock_t sock) {
  char buf[500];
  pid_t pid = fork();
  (void) env;

  if (pid == 0) {
    /*
     * In Linux `chdir` declared with `warn_unused_result` attribute
     * To shutup compiler we have yo use result in some way
     */
    int tmp = chdir(dir);
    (void) tmp;
    (void) dup2(sock, 0);
    (void) dup2(sock, 1);
    close(sock);

    /*
     * After exec, all signal handlers are restored to their default values,
     * with one exception of SIGCHLD. According to POSIX.1-2001 and Linux's
     * implementation, SIGCHLD's handler will leave unchanged after exec
     * if it was set to be ignored. Restore it to default action.
     */
    signal(SIGCHLD, SIG_IGN);

    alarm(CGI_TIMEOUT);

    execle(cmd, cmd, (char *) 0, envp); /* (char *) 0 to squash warning */
    snprintf(buf, sizeof(buf),
             "Status: 500\r\n\r\n"
             "500 Server Error: %s%s%s: %s",
             interp == NULL ? "" : interp, interp == NULL ? "" : " ", cmd,
             strerror(errno));
    send(1, buf, strlen(buf), 0);
    exit(EXIT_FAILURE); /* exec call failed */
  }

  return (pid != 0);
}

/*
 * Append VARIABLE=VALUE\0 string to the buffer, and add a respective
 * pointer into the vars array.
 */
static char *mg_addenv(struct mg_cgi_env_block *block, const char *fmt, ...) {
  int n, space;
  char *added = block->buf + block->len;
  va_list ap;

  /* Calculate how much space is left in the buffer */
  space = sizeof(block->buf) - (block->len + 2);
  if (space > 0) {
    /* Copy VARIABLE=VALUE\0 string into the free space */
    va_start(ap, fmt);
    n = vsnprintf(added, (size_t) space, fmt, ap);
    va_end(ap);

    /* Make sure we do not overflow buffer and the envp array */
    if (n > 0 && n + 1 < space &&
        block->nvars < (int) ARRAY_SIZE(block->vars) - 2) {
      /* Append a pointer to the added string into the envp array */
      block->vars[block->nvars++] = added;
      /* Bump up used length counter. Include \0 terminator */
      block->len += n + 1;
    }
  }

  return added;
}

static void mg_addenv2(struct mg_cgi_env_block *blk, const char *name) {
  const char *s;
  if ((s = getenv(name)) != NULL) mg_addenv(blk, "%s=%s", name, s);
}

static void mg_prepare_cgi_environment(struct mg_connection *nc,
                                       const char *prog,
                                       const struct mg_str *path_info,
                                       const struct http_message *hm,
                                       struct mg_cgi_env_block *blk) {
  const char *s;
  struct mg_str *h;
  char *p;
  size_t i;
  char buf[100];

  blk->len = blk->nvars = 0;
  blk->nc = nc;

  if ((s = getenv("SERVER_NAME")) != NULL) {
    mg_addenv(blk, "SERVER_NAME=%s", s);
  } else {
    mg_sock_to_str(nc->sock, buf, sizeof(buf), 3);
    mg_addenv(blk, "SERVER_NAME=%s", buf);
  }
  mg_addenv(blk, "SERVER_ROOT=%s", DOCUMENT_ROOT);
  mg_addenv(blk, "DOCUMENT_ROOT=%s", DOCUMENT_ROOT);
  mg_addenv(blk, "SERVER_SOFTWARE=%s/%s", "Mongoose", MG_VERSION);

  /* Prepare the environment block */
  mg_addenv(blk, "%s", "GATEWAY_INTERFACE=CGI/1.1");
  mg_addenv(blk, "%s", "SERVER_PROTOCOL=HTTP/1.1");
  mg_addenv(blk, "%s", "REDIRECT_STATUS=200"); /* For PHP */

  mg_addenv(blk, "REQUEST_METHOD=%.*s", (int) hm->method.len, hm->method.p);

  mg_addenv(blk, "REQUEST_URI=%.*s%s%.*s", (int) hm->uri.len, hm->uri.p,
            hm->query_string.len == 0 ? "" : "?", (int) hm->query_string.len,
            hm->query_string.p);

  mg_conn_addr_to_str(nc, buf, sizeof(buf),
                      MG_SOCK_STRINGIFY_REMOTE | MG_SOCK_STRINGIFY_IP);
  mg_addenv(blk, "REMOTE_ADDR=%s", buf);
  mg_conn_addr_to_str(nc, buf, sizeof(buf), MG_SOCK_STRINGIFY_PORT);
  mg_addenv(blk, "SERVER_PORT=%s", buf);

  s = hm->uri.p + hm->uri.len - path_info->len - 1;
  if (*s == '/') {
    const char *base_name = strrchr(prog, DIRSEP);
    mg_addenv(blk, "SCRIPT_NAME=%.*s/%s", (int) (s - hm->uri.p), hm->uri.p,
              (base_name != NULL ? base_name + 1 : prog));
  } else {
    mg_addenv(blk, "SCRIPT_NAME=%.*s", (int) (s - hm->uri.p + 1), hm->uri.p);
  }
  mg_addenv(blk, "SCRIPT_FILENAME=%s", prog);

  if (path_info != NULL && path_info->len > 0) {
    mg_addenv(blk, "PATH_INFO=%.*s", (int) path_info->len, path_info->p);
    /* Not really translated... */
    mg_addenv(blk, "PATH_TRANSLATED=%.*s", (int) path_info->len, path_info->p);
  }

  mg_addenv(blk, "HTTPS=off");

  if ((h = mg_get_http_header((struct http_message *) hm, "Content-Type")) !=
      NULL) {
    mg_addenv(blk, "CONTENT_TYPE=%.*s", (int) h->len, h->p);
  }

  if (hm->query_string.len > 0) {
    mg_addenv(blk, "QUERY_STRING=%.*s", (int) hm->query_string.len,
              hm->query_string.p);
  }

  if ((h = mg_get_http_header((struct http_message *) hm, "Content-Length")) !=
      NULL) {
    mg_addenv(blk, "CONTENT_LENGTH=%.*s", (int) h->len, h->p);
  }

  mg_addenv2(blk, "PATH");
  mg_addenv2(blk, "TMP");
  mg_addenv2(blk, "TEMP");
  mg_addenv2(blk, "TMPDIR");
  mg_addenv2(blk, "PERLLIB");
  mg_addenv2(blk, MG_ENV_EXPORT_TO_CGI);

  mg_addenv2(blk, "LD_LIBRARY_PATH");

  /* Add all headers as HTTP_* variables */
  for (i = 0; hm->header_names[i].len > 0; i++) {
    p = mg_addenv(blk, "HTTP_%.*s=%.*s", (int) hm->header_names[i].len,
                  hm->header_names[i].p, (int) hm->header_values[i].len,
                  hm->header_values[i].p);

    /* Convert variable name into uppercase, and change - to _ */
    for (; *p != '=' && *p != '\0'; p++) {
      if (*p == '-') *p = '_';
      *p = (char) toupper(*(unsigned char *) p);
    }
  }

  blk->vars[blk->nvars++] = NULL;
  blk->buf[blk->len++] = '\0';
}

static void mg_cgi_ev_handler(struct mg_connection *cgi_nc, int ev,
                              void *ev_data) {
  struct mg_connection *nc = (struct mg_connection *) cgi_nc->user_data;
  (void) ev_data;

  if (nc == NULL) {
    cgi_nc->flags |= MG_F_CLOSE_IMMEDIATELY;
    return;
  }

  switch (ev) {
    case MG_EV_RECV:
      /*
       * CGI script does not output reply line, like "HTTP/1.1 CODE XXXXX\n"
       * It outputs headers, then body. Headers might include "Status"
       * header, which changes CODE, and it might include "Location" header
       * which changes CODE to 302.
       *
       * Therefore we do not send the output from the CGI script to the user
       * until all CGI headers are received.
       *
       * Here we parse the output from the CGI script, and if all headers has
       * been received, send appropriate reply line, and forward all
       * received headers to the client.
       */
      if (nc->flags & MG_F_USER_1) {
        struct mbuf *io = &cgi_nc->recv_mbuf;
        int len = mg_http_get_request_len(io->buf, io->len);

        if (len == 0) break;
        if (len < 0 || io->len > MG_MAX_HTTP_REPLY_SIZE) {
          cgi_nc->flags |= MG_F_CLOSE_IMMEDIATELY;
          mg_http_send_error(nc, 500, "Bad headers");
        } else {
          struct http_message hm;
          struct mg_str *h;
          mg_http_parse_headers(io->buf, io->buf + io->len, io->len, &hm);
          if (mg_get_http_header(&hm, "Location") != NULL) {
            mg_printf(nc, "%s", "HTTP/1.1 302 Moved\r\n");
          } else if ((h = mg_get_http_header(&hm, "Status")) != NULL) {
            mg_printf(nc, "HTTP/1.1 %.*s\r\n", (int) h->len, h->p);
          } else {
            mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\n");
          }
        }
        nc->flags &= ~MG_F_USER_1;
      }
      if (!(nc->flags & MG_F_USER_1)) {
        mg_forward(cgi_nc, nc);
      }
      break;
    case MG_EV_CLOSE:
      mg_http_free_proto_data_cgi(mg_http_get_proto_data(nc));
      nc->flags |= MG_F_SEND_AND_CLOSE;
      break;
  }
}

static void mg_handle_cgi(struct mg_connection *nc, const char *prog,
                               const struct mg_str *path_info,
                               const struct http_message *hm) {
  struct mg_cgi_env_block blk;
  char dir[MAX_PATH_SIZE];
  const char *p;
  sock_t fds[2];

  LOG(("%s", prog));
  mg_prepare_cgi_environment(nc, prog, path_info, hm, &blk);
  /*
   * CGI must be executed in its own directory. 'dir' must point to the
   * directory containing executable program, 'p' must point to the
   * executable program name relative to 'dir'.
   */
  if ((p = strrchr(prog, DIRSEP)) == NULL) {
    snprintf(dir, sizeof(dir), "%s", ".");
  } else {
    snprintf(dir, sizeof(dir), "%.*s", (int) (p - prog), prog);
    prog = p + 1;
  }

  /*
   * Try to create socketpair in a loop until success. mg_socketpair()
   * can be interrupted by a signal and fail.
   * TODO(lsm): use sigaction to restart interrupted syscall
   */
  do {
    mg_socketpair(fds);
  } while (fds[0] == INVALID_SOCKET);

  if (mg_start_process(NULL, prog, blk.buf, blk.vars, dir, fds[1]) != 0) {
    size_t n = nc->recv_mbuf.len - (hm->message.len - hm->body.len);
    struct mg_connection *cgi_nc =
        mg_add_sock(nc->mgr, fds[0], mg_cgi_ev_handler);

    struct mg_http_proto_data *cgi_pd = mg_http_get_proto_data(nc);
    cgi_pd->cgi_nc = cgi_nc;
    cgi_pd->cgi_nc->user_data = nc;
    nc->flags |= MG_F_USER_1;
    /* Push POST data to the CGI */
    if (n > 0 && n < nc->recv_mbuf.len) {
      mg_send(cgi_pd->cgi_nc, hm->body.p, n);
    }
    mbuf_remove(&nc->recv_mbuf, nc->recv_mbuf.len);
  } else {
    close(fds[0]);
    mg_http_send_error(nc, 500, "CGI failure");
  }
  close(fds[1]);
}

static void mg_http_free_proto_data_cgi(struct mg_http_proto_data *d) {
  if (d != NULL) {
    if (d->cgi_nc != NULL) {
      d->cgi_nc->flags |= MG_F_CLOSE_IMMEDIATELY;
      d->cgi_nc->user_data = NULL; // May be send this to developers
    }

    d->cgi_nc = NULL;
  }
}

static const char *mg_skip(const char *s, const char *end, const char *delims,
                    struct mg_str *v) {
  v->p = s;
  while (s < end && strchr(delims, *(unsigned char *) s) == NULL) s++;
  v->len = s - v->p;
  while (s < end && strchr(delims, *(unsigned char *) s) != NULL) s++;
  return s;
}

static int mg_stat(const char *path, cs_stat_t *st) {
  return stat(path, st);
}

/* Set close-on-exec bit for a given socket. */
static void mg_set_close_on_exec(sock_t sock) {
  fcntl(sock, F_SETFD, FD_CLOEXEC);
}

static void mg_sock_addr_to_str(const union socket_address *sa, char *buf, size_t len,
                         int flags) {
  if (buf == NULL || len <= 0) return;
  buf[0] = '\0';
  if (flags & MG_SOCK_STRINGIFY_IP) {
    inet_ntop(AF_INET, (void *) &sa->sin.sin_addr, buf, len);
  }
  if (flags & MG_SOCK_STRINGIFY_PORT) {
    int port = ntohs(sa->sin.sin_port);
    if (flags & MG_SOCK_STRINGIFY_IP) {
      snprintf(buf + strlen(buf), len - (strlen(buf) + 1), "%s:%d",
               "", port);
    } else {
      snprintf(buf, len, "%d", port);
    }
  }
}

static void mg_conn_addr_to_str(struct mg_connection *nc, char *buf, size_t len,
                         int flags) {
  union socket_address sa;
  memset(&sa, 0, sizeof(sa));
  mg_if_get_conn_addr(nc, flags & MG_SOCK_STRINGIFY_REMOTE, &sa);
  mg_sock_addr_to_str(&sa, buf, len, flags);
}

DO_NOT_WARN_UNUSED static int mg_get_errno(void) {
  return errno;
}
