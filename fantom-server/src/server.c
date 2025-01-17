#include <string.h>
#include <signal.h>
#include "mongoose.h"
#include "logger.h"
#include "server.h"
#include "security.h"
#include "enp/get_nonce.h"

static int running = 1;
static void signal_handler(int signo)
{
    running = 0;
    lprintf(LOG_WARNING, "SIG %d received, terminating...\n", signo);
}

static void cb(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
    if (ev == MG_EV_HTTP_MSG) {
        struct mg_http_message *hm = ev_data;
        fantom_server_t s = *(fantom_server_t *) fn_data;

        char uri[256];
        size_t len = MIN(sizeof(uri), hm->uri.len);
        strncpy(uri, hm->uri.ptr, len);
        uri[len] = 0;

        char ip_addr[sizeof("255.255.255.255")];
        mg_ntoa(&c->rem, ip_addr, sizeof(ip_addr));

        lprintf(LOG_INFO, "%s%s\n", ip_addr, uri);

        if (mg_http_match_uri(hm, "/api/get_nonce")) {
            get_nonce_enp(c, s);
        } else {
            mg_http_reply(c, 404, NULL, "404 - Page not found");
        }
    }
}

void start_fantom_server(fantom_config_t *config, fantom_db_t *db)
{
    lprintf(LOG_INFO, "Starting server on %s with the database file %s and, max log age of %d\n", config->bind_url, config->db_file, config->max_log_age_days);

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    fantom_nonce_manager_t nonce_mgr;
    init_nonce_manager(&nonce_mgr);

    fantom_server_t fantom_server = {config, db, &nonce_mgr};
    struct mg_mgr mgr;
    mg_log_set(MG_DEBUG_LVL);
    mg_mgr_init(&mgr);

    struct mg_connection *c = mg_http_listen(&mgr, config->bind_url, cb, &fantom_server);
    if (c == NULL) {
        lprintf(LOG_ERROR, "Cannot start server on %s\n", config->bind_url);
        return;
    }

    while (running) {
        mg_mgr_poll(&mgr, 1000);
    }

    lprintf(LOG_INFO, "Stopping server...\n");
    free_nonce_manager(&nonce_mgr);
    mg_mgr_free(&mgr);
}

