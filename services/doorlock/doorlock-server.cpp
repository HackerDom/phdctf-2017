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
#include <algorithm>
#include <time.h>
#include <stdlib.h>

#define LDAP_DEPRECATED 1
#include <lber.h>
#include <ldap.h>

extern "C" {
#include "coap_config.h"
#include "resource.h"
#include "coap.h"
}

#define COAP_RESOURCE_CHECK_TIME 2
#define LOCK_ID_LEN 10

char ldap_url[]      = "ldap://127.0.0.1:389";
char ldap_root[]     = "dc=iot,dc=phdays,dc=com";
char ldap_user[]     = "cn=admin,dc=iot,dc=phdays,dc=com";
char ldap_pass[]     = "XfhC57uwby3plBWD";
char charset[]       = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
int charset_len      = strlen(charset);

static LDAP *ld = NULL;

static int quit = 0;

static void handle_sigint(int signum) {
  quit = 1;
}

static void send_response(coap_pdu_t *response, int code, const char *data) {
  unsigned char buf[3];

  response->hdr->code = COAP_RESPONSE_CODE(code);

  coap_add_option( response, COAP_OPTION_CONTENT_TYPE, coap_encode_var_bytes(buf, COAP_MEDIATYPE_TEXT_PLAIN), buf );
  coap_add_option( response, COAP_OPTION_MAXAGE, coap_encode_var_bytes(buf, 0x2ffff), buf );
  coap_add_data( response, strlen(data), (unsigned char *)data );

  fprintf( stderr, "   -> %d / %s\n", code, data );
}

static int ldap_register_lock(
  char *lock_id,
  char *lock_model,
  char *lock_floor,
  char *lock_room)
{
  char dn[64];
  sprintf( dn, "cn=%s,cn=locks,%s", lock_id, ldap_root );

  LDAPMod OClass, LockModel, LockFloor, LockRoom, Timestamp;

  char *oc_values[] = { "top", "device", "lockObject", NULL };
  OClass.mod_op     = LDAP_MOD_ADD;
  OClass.mod_type   = "objectClass";
  OClass.mod_values = oc_values;

  char *lm_values[] = { lock_model, NULL };
  LockModel.mod_op     = LDAP_MOD_ADD;
  LockModel.mod_type   = "lockModel";
  LockModel.mod_values = lm_values;

  char *lf_values[] = { lock_floor, NULL };
  LockFloor.mod_op     = LDAP_MOD_ADD;
  LockFloor.mod_type   = "lockFloor";
  LockFloor.mod_values = lf_values;

  char *lr_values[] = { lock_room, NULL };
  LockRoom.mod_op     = LDAP_MOD_ADD;
  LockRoom.mod_type   = "lockRoom";
  LockRoom.mod_values = lr_values;

  char *ts_values[] = { "199412161032Z", NULL }; // TODO
  Timestamp.mod_op     = LDAP_MOD_ADD;
  Timestamp.mod_type   = "timestamp";
  Timestamp.mod_values = ts_values;

  LDAPMod *NewEntry[6] = { &OClass, &LockModel, &LockFloor, &LockRoom, &Timestamp, NULL };

  int ret = ldap_add_s( ld, dn, NewEntry );
  if ( ret != LDAP_SUCCESS ) {
    fprintf( stderr, "ldap_add_s failed: %s (%d)\n", ldap_err2string(ret), ret );
    return -1;
  }
  return 0;
}

static int ldap_add_card(
  char *lock_id,
  char *card_id,
  char *card_tag)
{
  char dn[256];
  sprintf( dn, "cn=%s,cn=%s,cn=locks,%s", card_id, lock_id, ldap_root );

  LDAPMod OClass, LockId, CardTag, CardOwner, Timestamp;

  char *oc_values[] = { "top", "device", "cardObject", NULL };
  OClass.mod_op     = LDAP_MOD_ADD;
  OClass.mod_type   = "objectClass";
  OClass.mod_values = oc_values;

  char *li_values[] = { lock_id, NULL };
  LockId.mod_op     = LDAP_MOD_ADD;
  LockId.mod_type   = "lockId";
  LockId.mod_values = li_values;

  char *ct_values[] = { card_tag, NULL };
  CardTag.mod_op     = LDAP_MOD_ADD;
  CardTag.mod_type   = "cardTag";
  CardTag.mod_values = ct_values;

  char *co_values[] = { "John Doe", NULL }; // TODO
  CardOwner.mod_op     = LDAP_MOD_ADD;
  CardOwner.mod_type   = "cardOwner";
  CardOwner.mod_values = co_values;

  char *ts_values[] = { "199412161032Z", NULL }; // TODO
  Timestamp.mod_op     = LDAP_MOD_ADD;
  Timestamp.mod_type   = "timestamp";
  Timestamp.mod_values = ts_values;

  LDAPMod *NewEntry[6] = { &OClass, &LockId, &CardTag, &CardOwner, &Timestamp, NULL };

  int ret = ldap_add_s( ld, dn, NewEntry );
  if ( ret != LDAP_SUCCESS ) {
    fprintf( stderr, "ldap_add_s failed: %s (%d)\n", ldap_err2string(ret), ret );
    return -1;
  }
  return 0;
}

static void ldap_get_card( coap_pdu_t *response, char *query )
{
  char *searchattrs[2] = { "cardTag", NULL };

  LDAPMessage *results;
  int ret = ldap_search_s( ld, ldap_root,	LDAP_SCOPE_SUBTREE, query, searchattrs, 0, &results);
	if (ret != LDAP_SUCCESS) {
		fprintf( stderr, "ldap_search_s: %s (%d)\n", ldap_err2string(ret), ret );
		send_response( response, 500, "ERROR" );
    return;
	}

  LDAPMessage *entry = ldap_first_entry( ld, results );
	if ( entry ) {
    BerElement *ber = NULL;
  	char *attr = ldap_first_attribute( ld, entry, &ber );
  	if ( attr ) {
      char **vals = ldap_get_values( ld, entry, attr );
    	if ( vals ) {
        send_response( response, 205, vals[0] );
        ldap_value_free( vals );
      }
      else {
    		send_response( response, 205, "EMPTY" );
    	}
      ldap_memfree( attr );
    }
    else {
  		send_response( response, 205, "EMPTY" );
  	}
    if ( ber != NULL ) {
  		ber_free(ber, 0);
  	}
    ldap_msgfree( entry );
  }
  else {
		send_response( response, 205, "EMPTY" );
	}
}

void url_decode(char *s) {
    int n = strlen(s);
    int src = 0, dst = 0;
    while (src < n) {
        if (s[src] == '%' && src+2 < n) {
            s[dst] = ((s[src+1]-'0')<<4) | (s[src+2]-'0');
            src += 3;
        }
        else {
            s[dst] = s[src];
            src += 1;
        }
        dst++;
    }
    s[dst] = 0;
}

void ldap_sanitize(char s[]) {
    int depth = 0;
    for (int i = 0; i < strlen(s); ++i) {
        depth += (s[i] == '(') - (s[i] == ')');
        if (s[i] == '*' || s[i] == '~' || s[i] == '\\' || s[i] == '|') {
            s[i] = '_';
        }
        s[i+1] = depth ? s[i+1] : 0;
    }
}

int ldap_validate(char s[]) {
  int open = 0;
  int close = 0;
  for (int i = 0; i < strlen(s); ++i) {
    open += (s[i] == '(');
    close += (s[i] == ')');
  }
  return close == 3 && open == 3;
}

static void hnd_get_index(
              coap_context_t *ctx,
              struct coap_resource_t *resource,
              const coap_endpoint_t *local_interface,
              coap_address_t *peer,
              coap_pdu_t *request,
              str *token,
              coap_pdu_t *response) {
  send_response(response, 205, "DOORLOCK Service 5.19 built May 2017");
}

static void hnd_register_lock(
              coap_context_t *ctx,
              struct coap_resource_t *resource,
              const coap_endpoint_t *local_interface,
              coap_address_t *peer,
              coap_pdu_t *request,
              str *token,
              coap_pdu_t *response) {
  coap_opt_iterator_t opt_iter;
  char model[64];
  char floor[8];
  char room[8];
  memset(model, 0, 64);
  memset(floor, 0, 8);
  memset(room, 0, 8);

  coap_opt_t *option = coap_check_option(request, COAP_OPTION_URI_QUERY, &opt_iter);
  while (option) {
    char buf[1024];
    memset(buf, 0, 1024);
    strncpy(buf, (char *)COAP_OPT_VALUE(option), std::min(1023, (int)COAP_OPT_LENGTH(option)));
    char *eq = strchr((char *)buf, '=');
    if (eq) {
      *eq = 0;
      if (0 == strcmp("model", buf)) {
        strncpy(model, eq + 1, std::min(63, (int)strlen(eq + 1)));
        url_decode(model);
      }
      if (0 == strcmp("floor", buf)) {
        strncpy(floor, eq + 1, std::min(7, (int)strlen(eq + 1)));
        url_decode(floor);
      }
      if (0 == strcmp("room", buf)) {
        strncpy(room, eq + 1, std::min(7, (int)strlen(eq + 1)));
        url_decode(room);
      }
    }
    option = coap_option_next(&opt_iter);
  }

  fprintf( stderr, "hnd_register_lock( model='%s', floor='%s', room='%s' )\n", model, floor, room );

  if (strlen(model) == 0 || strlen(floor) == 0 || strlen(room) == 0) {
    send_response(response, 400, "BAD PARAMS");
    return;
  }

  char lock_id[LOCK_ID_LEN];
  for (int i = 0; i < LOCK_ID_LEN - 1; ++i) {
    lock_id[i] = charset[rand() % charset_len];
  }
  lock_id[LOCK_ID_LEN - 1] = 0;

  int err = ldap_register_lock( lock_id, model, floor, room );
  if ( err ) {
    send_response( response, 500, "ERROR" );
  }
  else {
    send_response( response, 205, lock_id );
  }
}

static void hnd_add_card(
              coap_context_t *ctx,
              struct coap_resource_t *resource,
              const coap_endpoint_t *local_interface,
              coap_address_t *peer,
              coap_pdu_t *request,
              str *token,
              coap_pdu_t *response) {
  coap_opt_iterator_t opt_iter;
  char lock[64];
  char card[64];
  char tag[64];
  memset(lock, 0, 64);
  memset(card, 0, 64);
  memset(tag, 0, 64);

  coap_opt_t *option = coap_check_option(request, COAP_OPTION_URI_QUERY, &opt_iter);
  while (option) {
    char buf[1024];
    memset(buf, 0, 1024);
    strncpy(buf, (char *)COAP_OPT_VALUE(option), std::min(1023, (int)COAP_OPT_LENGTH(option)));
    char *eq = strchr((char *)buf, '=');
    if (eq) {
      *eq = 0;
      if (0 == strcmp("lock", buf)) {
        strncpy(lock, eq + 1, std::min(63, (int)strlen(eq + 1)));
        url_decode(lock);
      }
      if (0 == strcmp("card", buf)) {
        strncpy(card, eq + 1, std::min(63, (int)strlen(eq + 1)));
        url_decode(card);
      }
      if (0 == strcmp("tag", buf)) {
        strncpy(tag, eq + 1, std::min(63, (int)strlen(eq + 1)));
        url_decode(tag);
      }
    }
    option = coap_option_next(&opt_iter);
  }
  printf("hnd_add_card( lock='%s', card='%s', tag='%s' )\n", lock, card, tag);

  int err = ldap_add_card( lock, card, tag );
  if ( err ) {
    send_response( response, 500, "ERROR" );
  }
  else {
    send_response( response, 205, "OK" );
  }
}

static void hnd_get_card(
              coap_context_t *ctx,
              struct coap_resource_t *resource,
              const coap_endpoint_t *local_interface,
              coap_address_t *peer,
              coap_pdu_t *request,
              str *token,
              coap_pdu_t *response) {
  coap_opt_iterator_t opt_iter;
  char lock[64];
  char card[64];
  memset(lock, 0, 64);
  memset(card, 0, 64);

  coap_opt_t *option = coap_check_option(request, COAP_OPTION_URI_QUERY, &opt_iter);
  while (option) {
    char buf[1024];
    memset(buf, 0, 1024);
    strncpy(buf, (char *)COAP_OPT_VALUE(option), std::min(1023, (int)COAP_OPT_LENGTH(option)));
    char *eq = strchr((char *)buf, '=');
    if (eq) {
      *eq = 0;
      if (0 == strcmp("lock", buf)) {
        strncpy(lock, eq + 1, std::min(63, (int)strlen(eq + 1)));
        url_decode(lock);
      }
      if (0 == strcmp("card", buf)) {
        strncpy(card, eq + 1, std::min(63, (int)strlen(eq + 1)));
        url_decode(card);
      }
    }
    option = coap_option_next(&opt_iter);
  }
  printf( "hnd_get_card( lock='%s', card='%s' )\n", lock, card );

  char query[512];
  sprintf( query, "(&(cn=%s)(lockId=%s))", card, lock );
  ldap_sanitize( query );
  if (!ldap_validate( query )) {
    send_response( response, 403, "FORBIDDEN" );
  }
  else {
    ldap_get_card( response, query );
  }
}

static void add_resource(
              coap_context_t *ctx,
              unsigned char method,
              const char *uri,
              coap_method_handler_t handler )
{
  coap_resource_t * r = coap_resource_init((unsigned char *)uri, uri ? strlen(uri) : 0, 0);
  coap_register_handler(r, method, handler);
  coap_add_resource(ctx, r);
}

static void init_resources( coap_context_t *ctx )
{
  add_resource(ctx, COAP_REQUEST_GET,  NULL,            hnd_get_index);
  add_resource(ctx, COAP_REQUEST_POST, "register_lock", hnd_register_lock);
  add_resource(ctx, COAP_REQUEST_POST, "add_card",      hnd_add_card);
  add_resource(ctx, COAP_REQUEST_GET,  "get_card",      hnd_get_card);
}

static void usage( const char *program )
{
  const char *p = strrchr( program, '/' );
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

int init_ldap()
{
  int ret;

  ret = ldap_initialize( &ld, ldap_url );
  if (ret) {
    fprintf( stderr, "ldap_initialize" );
    return ret;
  }

  int protocol_version = LDAP_VERSION3;
  ret = ldap_set_option( ld, LDAP_OPT_PROTOCOL_VERSION, &protocol_version );
  if (ret) {
    fprintf( stderr, "ldap_set_option" );
    return ret;
  }

  ret = ldap_simple_bind_s( ld, ldap_user, ldap_pass );
	if (ret) {
		fprintf( stderr, "ldap_simple_bind_s" );
		return ret;
	}

  fprintf( stderr, "Connected to ldap: %s\n", ldap_url );
  return 0;
}

void init_random()
{
  unsigned int seed = time( NULL );
  FILE *f = fopen( "/dev/urandom", "rb" );
  if (!f) {
    fprintf( stderr, "Warning: cannot open urandom\n" );
  }
  else {
    for (int i = 0; i < 10; ++i) {
      unsigned int val;
      int count = fread( &val, sizeof(val), 1, f );
      if ( count != 1 ) {
        break;
      }
      seed ^= val;
    }
    fclose( f );
  }
  srand( seed );
}

int main(int argc, char **argv)
{
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

  int ret = init_ldap();
  if (ret) {
    fprintf( stderr, " failed: %s (%d)\n", ldap_err2string(ret), ret );
    return -1;
  }

  signal( SIGINT, handle_sigint );

  fprintf( stderr, "Starting doorlock server at %s:%s\n", addr_str, port_str);

  init_random();

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

  ldap_unbind_s( ld );
  coap_free_context( ctx );

  return 0;
}
