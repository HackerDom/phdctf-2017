#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <signal.h>

extern "C" {
#include "coap_config.h"
#include "resource.h"
#include "coap.h"
}

#define COAP_RESOURCE_CHECK_TIME 2

static int quit = 0;

static void handle_sigint(int signum) {
  quit = 1;
}

static void hnd_get_index(
              coap_context_t *ctx,
              struct coap_resource_t *resource,
              const coap_endpoint_t *local_interface,
              coap_address_t *peer,
              coap_pdu_t *request,
              str *token,
              coap_pdu_t *response) {
  unsigned char buf[3];
  char data[] = "DOORLOCK Service\n\n";

  response->hdr->code = COAP_RESPONSE_CODE(205);

  coap_add_option(response, COAP_OPTION_CONTENT_TYPE, coap_encode_var_bytes(buf, COAP_MEDIATYPE_TEXT_PLAIN), buf);
  coap_add_option(response, COAP_OPTION_MAXAGE, coap_encode_var_bytes(buf, 0x2ffff), buf);
  coap_add_data(response, strlen(data), (unsigned char *)data);
}

static void hnd_put_echo(
              coap_context_t *ctx,
              struct coap_resource_t *resource,
              const coap_endpoint_t *local_interface,
              coap_address_t *peer,
              coap_pdu_t *request,
              str *token,
              coap_pdu_t *response ) {
  size_t size;
  unsigned char *data;

  (void)coap_get_data(request, &size, &data);

  if (size == 0) { /* coap_get_data() sets size to 0 on error */
    response->hdr->code = COAP_RESPONSE_CODE(400);
  }
  else {
    unsigned char buf[3];
    response->hdr->code = COAP_RESPONSE_CODE(205);
    coap_add_option(response, COAP_OPTION_CONTENT_TYPE, coap_encode_var_bytes(buf, COAP_MEDIATYPE_TEXT_PLAIN), buf);
    coap_add_option(response, COAP_OPTION_MAXAGE, coap_encode_var_bytes(buf, 0x2ffff), buf);
    coap_add_data(response, size, data);
  }
}

static void init_resources( coap_context_t *ctx ) {
  coap_resource_t *r;

  r = coap_resource_init(NULL, 0, 0);
  coap_register_handler(r, COAP_REQUEST_GET, hnd_get_index);
  coap_add_resource(ctx, r);

  r = coap_resource_init((unsigned char *)"echo", 4, 0);
  coap_register_handler(r, COAP_REQUEST_PUT, hnd_put_echo);
  coap_add_resource(ctx, r);
}

static void usage( const char *program ) {
  const char *p;

  p = strrchr( program, '/' );
  if ( p )
    program = ++p;

  fprintf( stderr,
    "usage: %s [-A address] [-p port]\n\n"
    "\t-A address\tinterface address to bind to\n"
    "\t-p port\t\tlisten on specified port\n",
    program );
}

static coap_context_t *
get_context(const char *node, const char *port) {
  coap_context_t *ctx = NULL;
  int s;
  struct addrinfo hints;
  struct addrinfo *result, *rp;

  memset(&hints, 0, sizeof(struct addrinfo));
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_DGRAM;
  hints.ai_flags = AI_PASSIVE | AI_NUMERICHOST;

  s = getaddrinfo(node, port, &hints, &result);
  if ( s != 0 ) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
    return NULL;
  }

  /* iterate through results until success */
  for (rp = result; rp != NULL; rp = rp->ai_next) {
    coap_address_t addr;

    if (rp->ai_addrlen <= sizeof(addr.addr)) {
      coap_address_init(&addr);
      addr.size = rp->ai_addrlen;
      memcpy(&addr.addr, rp->ai_addr, rp->ai_addrlen);

      ctx = coap_new_context(&addr);
      if (ctx) {
        goto finish;
      }
    }
  }

  fprintf(stderr, "no context available for interface '%s'\n", node);

  finish:
  freeaddrinfo(result);
  return ctx;
}

int
main(int argc, char **argv) {
  coap_context_t  *ctx;
  fd_set readfds;
  struct timeval tv, *timeout;
  coap_tick_t now;
  char addr_str[NI_MAXHOST] = "0.0.0.0";
  char port_str[NI_MAXSERV] = "5683";
  int opt;
  coap_log_t log_level = LOG_WARNING;

  while ((opt = getopt(argc, argv, "A:p:")) != -1) {
    switch (opt) {
    case 'A' :
      strncpy(addr_str, optarg, NI_MAXHOST-1);
      addr_str[NI_MAXHOST - 1] = '\0';
      break;
    case 'p' :
      strncpy(port_str, optarg, NI_MAXSERV-1);
      port_str[NI_MAXSERV - 1] = '\0';
      break;
    default:
      usage( argv[0] );
      exit( 1 );
    }
  }

  coap_set_log_level( log_level );

  ctx = get_context( addr_str, port_str );
  if (!ctx)
    return -1;

  init_resources( ctx );

  signal( SIGINT, handle_sigint );

  while ( !quit ) {
    FD_ZERO( &readfds );
    FD_SET( ctx->sockfd, &readfds );

    coap_queue_t * nextpdu = coap_peek_next( ctx );

    coap_ticks( &now );
    while ( nextpdu && nextpdu->t <= now - ctx->sendqueue_basetime ) {
      coap_retransmit( ctx, coap_pop_next( ctx ) );
      nextpdu = coap_peek_next( ctx );
    }

    if ( nextpdu && nextpdu->t <= (COAP_RESOURCE_CHECK_TIME * COAP_TICKS_PER_SECOND) ) {
      tv.tv_usec = ((nextpdu->t) % COAP_TICKS_PER_SECOND) * 1000000 / COAP_TICKS_PER_SECOND;
      tv.tv_sec = (nextpdu->t) / COAP_TICKS_PER_SECOND;
      timeout = &tv;
    } else {
      tv.tv_usec = 0;
      tv.tv_sec = COAP_RESOURCE_CHECK_TIME;
      timeout = &tv;
    }

    int result = select( FD_SETSIZE, &readfds, 0, 0, timeout );
    if ( result < 0 ) {         /* error */
      if ( errno != EINTR )
        perror("select");
    } else if ( result > 0 ) {  /* read from socket */
      if ( FD_ISSET( ctx->sockfd, &readfds ) ) {
        coap_read( ctx );
      }
    }
  }

  coap_free_context(ctx);

  return 0;
}
