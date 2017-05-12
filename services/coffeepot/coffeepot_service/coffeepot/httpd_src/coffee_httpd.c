#include "mongoose.h"

static void ev_handler(struct mg_connection *nc, int ev, void *p) {
  if (ev == MG_EV_HTTP_REQUEST) {
    mg_serve_http(nc, (struct http_message *) p);
  }
}

int main(int argc, char **argv) {
  struct mg_mgr mgr;
  struct mg_connection *nc;

  const char* listen_addr = "127.0.0.1:3255";

  if (argc >= 2) {
    listen_addr = argv[1];
  }

  mg_mgr_init(&mgr);
  printf("Starting web server on %s\n", listen_addr);

  nc = mg_bind(&mgr, listen_addr, ev_handler);
  if (nc == NULL) {
    printf("Failed listen port\n");
    return 1;
  }

  for (;;) {
    mg_mgr_poll(&mgr, 3000);
  }
  mg_mgr_free(&mgr);

  return 0;
}
