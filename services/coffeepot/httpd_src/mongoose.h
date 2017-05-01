/*
 * Copyright (c) 2004-2013 Sergey Lyubka
 * Copyright (c) 2013-2015 Cesanta Software Limited
 * All rights reserved
 *
 * This software is dual-licensed: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation. For the terms of this
 * license, see <http://www.gnu.org/licenses/>.
 *
 * You are free to use this software under the terms of the GNU General
 * Public License, but WITHOUT ANY WARRANTY; without even the implied
 * warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * Alternatively, you can license this software under a commercial
 * license, as set out in <https://www.cesanta.com/license>.
 */

#define MG_VERSION "6.7"

#define DO_NOT_WARN_UNUSED __attribute__((unused))

#define ARRAY_SIZE(array) (sizeof(array) / sizeof(array[0]))

/* Enable 64-bit file offsets */
#define _FILE_OFFSET_BITS 64

#include <arpa/inet.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <unistd.h>

typedef int sock_t;
typedef struct stat cs_stat_t;
#define INVALID_SOCKET (-1)
#define DIRSEP '/'

/* Describes chunk of memory */
struct mg_str {
  const char *p; /* Memory chunk pointer */
  size_t len;    /* Memory chunk length */
};

/* Memory buffer descriptor */
struct mbuf {
  char *buf;   /* Buffer pointer */
  size_t len;  /* Data length. Data is located between offset 0 and len. */
  size_t size; /* Buffer size allocated by realloc(1). Must be >= len */
};

struct mg_connection;

union socket_address {
  struct sockaddr sa;
  struct sockaddr_in sin;
  struct sockaddr sin6;
};

/*
 * Callback function (event handler) prototype. Must be defined by the user.
 * Mongoose calls the event handler, passing the events defined below.
 */
typedef void (*mg_event_handler_t)(struct mg_connection *nc, int ev,
                                   void *ev_data);

/* Events. Meaning of event parameter (evp) is given in the comment. */
#define MG_EV_POLL 0    /* Sent to each connection on each mg_mgr_poll() call */
#define MG_EV_ACCEPT 1  /* New connection accepted. union socket_address * */
#define MG_EV_CONNECT 2 /* connect() succeeded or failed. int *  */
#define MG_EV_RECV 3    /* Data has benn received. int *num_bytes */
#define MG_EV_SEND 4    /* Data has been written to a socket. int *num_bytes */
#define MG_EV_CLOSE 5   /* Connection is closed. NULL */
#define MG_EV_TIMER 6   /* now >= conn->ev_timer_time. double * */

/*
 * Mongoose event manager.
 */
struct mg_mgr {
  struct mg_connection *active_connections;
};

struct mg_http_proto_data {
  struct mg_connection *cgi_nc;
};

/*
 * Mongoose connection.
 */
struct mg_connection {
  struct mg_connection *next, *prev; /* mg_mgr::active_connections linkage */
  struct mg_connection *listener;    /* Set only for accept()-ed connections */
  struct mg_mgr *mgr;                /* Pointer to containing manager */

  sock_t sock; /* Socket to the remote peer */
  union socket_address sa; /* Remote peer address */
  size_t recv_mbuf_limit;  /* Max size of recv buffer */
  struct mbuf recv_mbuf;   /* Received data */
  struct mbuf send_mbuf;   /* Data scheduled for sending */
  time_t last_io_time;     /* Timestamp of the last socket IO */
  double ev_timer_time;    /* Timestamp of the future MG_EV_TIMER */
  mg_event_handler_t proto_handler; /* Protocol-specific event handler */
  struct mg_http_proto_data *proto_data;    /* Protocol-specific data */
  void (*proto_data_destructor)(void *proto_data);
  mg_event_handler_t handler; /* Event handler function */
  void *user_data;            /* User-specific data */
  unsigned long flags;
/* Flags set by Mongoose */
#define MG_F_LISTENING (1 << 0)          /* This connection is listening */
#define MG_F_IS_WEBSOCKET (1 << 1)       /* Websocket specific */

/* Flags that are settable by user */
#define MG_F_SEND_AND_CLOSE (1 << 2)       /* Push remaining data and close  */
#define MG_F_CLOSE_IMMEDIATELY (1 << 3)    /* Disconnect */

#define MG_F_USER_1 (1 << 4) /* Flags left for application */
};

/*
 * Initialise Mongoose manager. Side effect: ignores SIGPIPE signal.
 */
void mg_mgr_init(struct mg_mgr *mgr);

/*
 * De-initialises Mongoose manager.
 *
 * Closes and deallocates all active connections.
 */
void mg_mgr_free(struct mg_mgr *);

/*
 * This function performs the actual IO and must be called in a loop
 * (an event loop). It returns the current timestamp.
 * `milli` is the maximum number of milliseconds to sleep.
 * `mg_mgr_poll()` checks all connections for IO readiness. If at least one
 * of the connections is IO-ready, `mg_mgr_poll()` triggers the respective
 * event handlers and returns.
 */
time_t mg_mgr_poll(struct mg_mgr *, int milli);

/*
 * Creates a listening connection.
 */
struct mg_connection *mg_bind(struct mg_mgr *, const char *,
                              mg_event_handler_t);

#define MAX_PATH_SIZE 500

#define MG_SOCK_STRINGIFY_IP 1
#define MG_SOCK_STRINGIFY_PORT 2
#define MG_SOCK_STRINGIFY_REMOTE 4

#define MG_MAX_HTTP_HEADERS 20
#define MG_MAX_HTTP_REQUEST_SIZE 4092
#define MG_CGI_ENVIRONMENT_SIZE 8192

/* HTTP message */
struct http_message {
  struct mg_str message; /* Whole message: request line + headers + body */

  struct mg_str method; /* "GET" */
  struct mg_str uri;    /* "/my_file.html" */
  struct mg_str proto;  /* "HTTP/1.1" -- for both request and response */
  struct mg_str query_string;

  /* Headers */
  struct mg_str header_names[MG_MAX_HTTP_HEADERS];
  struct mg_str header_values[MG_MAX_HTTP_HEADERS];

  /* Message body */
  struct mg_str body; /* Zero-length for requests with no body */
};

/* HTTP and websocket events. void *ev_data is described in a comment. */
#define MG_EV_HTTP_REQUEST 100 /* struct http_message * */
#define MG_EV_HTTP_REPLY 101   /* struct http_message * */

void mg_serve_http(struct mg_connection *nc, struct http_message *hm);
