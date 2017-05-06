#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <microhttpd.h>
#include "MQTTClient.h"
#include <sys/timeb.h>
#include <signal.h>

#define HTTP_SERVER_PORT            8888
#define MQTT_BROKER_ADDRESS         "tcp://mqtt-broker:1883"
#define MQTT_MODULE_CLIENT_ID       "temperature-module"
#define QOS                         1
#define AUTHORIZATION_REQUEST_TOPIC "house/authorization"

int answer_to_connection (void *cls, struct MHD_Connection *connection,
                          const char *url,
                          const char *method, const char *version,
                          const char *upload_data,
                          size_t *upload_data_size, void **con_cls)
{
    char *user;
    char *pass;
    int fail;
    int ret;
    struct MHD_Response *response;

    if (0 != strcmp(method, MHD_HTTP_METHOD_GET))
        return MHD_NO;

    if (NULL == *con_cls)
    {
        *con_cls = connection;
        return MHD_YES;
    }

    pass = NULL;
    user = MHD_basic_auth_get_username_password(connection, &pass);
    fail = ((user == NULL) || (0 != strcmp(user, "admin")) || (0 != strcmp(pass, "webrelay")));

    if (user != NULL)
        free(user);

    if (pass != NULL)
        free(pass);

    if (fail)
    {
        const char *page = "<html><body>Go away.</body></html>";
        response = MHD_create_response_from_buffer(strlen(page), (void *)page, MHD_RESPMEM_PERSISTENT);
        ret = MHD_queue_basic_auth_fail_response (connection, "my realm", response);
    }
    else
    {
        const char *page = "<html><body>A secret.</body></html>";
        response = MHD_create_response_from_buffer (strlen (page), (void*) page, MHD_RESPMEM_PERSISTENT);
        ret = MHD_queue_response (connection, MHD_HTTP_OK, response);
    }

    MHD_destroy_response (response);
    return ret;
}

int process_mqtt_message(void *context, char *topic_name, int topic_len, MQTTClient_message *message)
{
    if (strcmp(topic_name, AUTHORIZATION_REQUEST_TOPIC) == 0) 
    {
        printf("Authorization request for client_id '%s'.\n", message->payload);
    }
    else
    {
        printf("Message from topic '%s' arrived.\n", topic_name);
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topic_name);

    return 1;
}

void connect_to_mqtt_broker(MQTTClient* mqtt_client)
{
    int rc;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 0;
    conn_opts.connectTimeout = 10;
    conn_opts.retryInterval = 1;

    do {
        if ((rc = MQTTClient_connect(*mqtt_client, &conn_opts)) != MQTTCLIENT_SUCCESS)
        {
            printf("Failed to connect to MQTT broker '%s', return code %d.\n", MQTT_BROKER_ADDRESS, rc);
            sleep(1);
        }
    }
    while (rc != MQTTCLIENT_SUCCESS);

    printf("Connected to '%s'\n", MQTT_BROKER_ADDRESS);
}

void disconnect_from_mqtt_broker(MQTTClient* mqtt_client)
{
    int rc;

    if ((rc = MQTTClient_disconnect(*mqtt_client, 10000)) != MQTTCLIENT_SUCCESS)
        printf("Failed to disconnect MQTT client, return code %d.\n", rc);
    else
        printf("Disconnected from '%s'.\n", MQTT_BROKER_ADDRESS);
}

void subscribe_to_topic(MQTTClient* mqtt_client, char *topic)
{
    int rc;

    if ((rc = MQTTClient_subscribe(*mqtt_client, AUTHORIZATION_REQUEST_TOPIC, 0)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to subscribe to topic '%s', return code %d.\n", AUTHORIZATION_REQUEST_TOPIC, rc);
        exit(EXIT_FAILURE);
    }

    printf("Subscribed to topic '%s'\n", AUTHORIZATION_REQUEST_TOPIC);
}

void unsubscribe_from_topic(MQTTClient* mqtt_client, char *topic)
{
    int rc;

    if ((rc = MQTTClient_unsubscribe(*mqtt_client, topic)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to unsubscribe from topic '%s', return code %d.\n", topic, rc);
        exit(EXIT_FAILURE);
    }

    printf("Unsubscribed from topic '%s'.\n", topic);
}

void process_connection_lost(void *context, char *cause)
{
    printf("Connection to '%s' lost.\n", MQTT_BROKER_ADDRESS);
    if (context != NULL)
    {
        MQTTClient* mqtt_client = (MQTTClient*)context;
        connect_to_mqtt_broker(mqtt_client);
        subscribe_to_topic(mqtt_client, AUTHORIZATION_REQUEST_TOPIC);
    }
}

void init_mqtt_client(MQTTClient* mqtt_client)
{
    int rc;

    if ((rc = MQTTClient_create(mqtt_client, MQTT_BROKER_ADDRESS, MQTT_MODULE_CLIENT_ID, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to create MQTT client, return code %d.\n", rc);
        exit(EXIT_FAILURE);
    }
}

void set_mqtt_callbacks(MQTTClient* mqtt_client)
{
    int rc;

    if ((rc = MQTTClient_setCallbacks(*mqtt_client, mqtt_client, process_connection_lost, process_mqtt_message, NULL)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to set callbacks for MQTT client, return code %d.\n", rc);
        exit(EXIT_FAILURE);
    }
}

volatile sig_atomic_t stop = 0;

void term(int signum)
{
    stop = 1;
}

void subscribe_to_signals()
{
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = term;
    sigaction(SIGTERM, &action, NULL);
    sigaction(SIGINT, &action, NULL);
}

int main()
{
    subscribe_to_signals();

    MQTTClient mqtt_client;
    init_mqtt_client(&mqtt_client);
    set_mqtt_callbacks(&mqtt_client);
    connect_to_mqtt_broker(&mqtt_client);
    subscribe_to_topic(&mqtt_client, AUTHORIZATION_REQUEST_TOPIC);

    struct MHD_Daemon *daemon;
    daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, HTTP_SERVER_PORT, NULL, NULL, &answer_to_connection, NULL, MHD_OPTION_END);
    if (NULL == daemon)
        goto stop;

    while (!stop)
    {
        sleep(10000);
    }

    MHD_stop_daemon (daemon);

stop:
    unsubscribe_from_topic(&mqtt_client, AUTHORIZATION_REQUEST_TOPIC);
    disconnect_from_mqtt_broker(&mqtt_client);
    MQTTClient_destroy(&mqtt_client);

    return 0;
}
