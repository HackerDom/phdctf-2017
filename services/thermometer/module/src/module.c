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
#include <mysql.h>
#include <uuid/uuid.h>

#define HTTP_SERVER_PORT            8888
#define MQTT_BROKER_ADDRESS         "tcp://mqtt-broker:1883"
#define MQTT_MODULE_CLIENT_ID       "temperature-module"
#define QOS                         1
#define AUTHORIZATION_REQUEST_TOPIC "house/authorization"
#define TIMEOUT                     10000L
#define MQTT_DB_HOST                "mqtt-db"
#define MQTT_DB_PORT                3306
#define MQTT_DB_DATABASE            "mqtt"
#define MQTT_DB_USER                "thermometer_module"
#define MQTT_DB_PASSWORD            "4VS6yKnPF9eLoZkB"

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

typedef struct MQTT_CONNECTION_INFO {
  MQTTClient *mqtt_client;
  char *username;
  char *password;
} MQTT_CONNECTION_INFO;



void publish_message(const MQTTClient* client, char* topic, void* payload, int payloadlen)
{
    int rc;
    MQTTClient_message msg = MQTTClient_message_initializer;
    msg.payload = payload;
    msg.payloadlen = payloadlen;
    msg.qos = 1;
    msg.retained = 0;

    MQTTClient_deliveryToken token;

    if ((rc = MQTTClient_publishMessage(*client, topic, &msg, &token)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to publish message to topic '%s', return code %d.\n", topic, rc);
        exit(EXIT_FAILURE);
    }

    printf("Published message to topic '%s'.\n", topic);
}

int process_mqtt_message(void *context, char *topic_name, int topic_len, MQTTClient_message *message)
{
    if (strcmp(topic_name, AUTHORIZATION_REQUEST_TOPIC) == 0) 
    {
        printf("Authorization request for client_id '%s'.\n", message->payload);

        if (context != NULL)
        {
            MQTTClient* mqtt_client = ((MQTT_CONNECTION_INFO*)context)->mqtt_client;

            char *client_authorization_topic = (char *)malloc((strlen("house/authorization/") + strlen(message->payload) + 1) * sizeof(char));
            sprintf(client_authorization_topic, "house/authorization/%s", message->payload);
            char* password = "Pass";
            publish_message(mqtt_client, client_authorization_topic, password, strlen(password) + 1);
            free(client_authorization_topic);
        }
    }
    else
    {
        printf("Message from topic '%s' arrived.\n", topic_name);
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topic_name);

    return 1;
}

void connect_to_mqtt_broker(MQTTClient* mqtt_client, char *username, char *password)
{
    int rc;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 0;
    conn_opts.connectTimeout = 10;
    conn_opts.username = username;
    conn_opts.password = password;

    do {
        if ((rc = MQTTClient_connect(*mqtt_client, &conn_opts)) != MQTTCLIENT_SUCCESS)
        {
            printf("Failed to connect to MQTT broker '%s', return code %d.\n", MQTT_BROKER_ADDRESS, rc);
            sleep(1);
        }
    }
    while (rc != MQTTCLIENT_SUCCESS);

    printf("Connected to '%s'.\n", MQTT_BROKER_ADDRESS);
}

void disconnect_from_mqtt_broker(MQTTClient* mqtt_client)
{
    int rc;

    if ((rc = MQTTClient_disconnect(*mqtt_client, 10000)) != MQTTCLIENT_SUCCESS)
        printf("Failed to disconnect MQTT client, return code %d.\n", rc);
    else
        printf("Disconnected from '%s'.\n", MQTT_BROKER_ADDRESS);
}

volatile MQTTClient_deliveryToken deliveredtoken;
void process_delivered(void *context, MQTTClient_deliveryToken dt)
{
    printf("Message with token value %d delivery confirmed.\n", dt);
    deliveredtoken = dt;
}

void subscribe_to_topic(MQTTClient* mqtt_client, char *topic)
{
    int rc;

    if ((rc = MQTTClient_subscribe(*mqtt_client, topic, 0)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to subscribe to topic '%s', return code %d.\n", topic, rc);
        exit(EXIT_FAILURE);
    }

    printf("Subscribed to topic '%s'.\n", topic);
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
        MQTT_CONNECTION_INFO* connection_info = (MQTT_CONNECTION_INFO*)context;
        connect_to_mqtt_broker(connection_info->mqtt_client, connection_info->username, connection_info->password);
        subscribe_to_topic(connection_info->mqtt_client, AUTHORIZATION_REQUEST_TOPIC);
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

void set_mqtt_callbacks(MQTTClient* mqtt_client, MQTT_CONNECTION_INFO *context)
{
    int rc;

    if ((rc = MQTTClient_setCallbacks(*mqtt_client, context, process_connection_lost, process_mqtt_message, process_delivered)) != MQTTCLIENT_SUCCESS)
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

MYSQL* mqtt_db_init()
{
    MYSQL *mysql = mysql_init(NULL);
    if (mysql == NULL) 
    {
        printf("Failed to init MySQL object: %s\n", mysql_error(mysql));
        exit(EXIT_FAILURE);
    }

    return mysql;
}

void mqtt_db_connect(MYSQL* mysql)
{
    do
    {
        if (mysql_real_connect(mysql, MQTT_DB_HOST, MQTT_DB_USER, MQTT_DB_PASSWORD, MQTT_DB_DATABASE, 0, NULL, 0) == NULL) 
        {
            printf("Can't connect to MySQL db '%s:%d': %s\n", MQTT_DB_HOST, MQTT_DB_PORT, mysql_error(mysql));
            sleep(1);
        }
        else
            break;
    } while(1);

    printf("Connected to '%s:%d'.\n", MQTT_DB_HOST, MQTT_DB_PORT);
}

void add_mqtt_user(MYSQL *mysql, char *user, char *password)
{
    char *query = (char *)malloc((strlen("INSERT INTO users (username, pw) VALUES(\"\", \"\") ON DUPLICATE KEY UPDATE username=\"\", pw=\"\"") + strlen(user)*2 + strlen(password)*2 + 1) * sizeof(char));
    sprintf(query, "INSERT INTO users (username, pw) VALUES(\"%s\", \"%s\") ON DUPLICATE KEY UPDATE username=\"%s\", pw=\"%s\"", user, password, user, password);
    if (mysql_query(mysql, query)) 
    {
        printf("Failed to add MQTT user: %s\n", mysql_error(mysql));
        free(query);
        mysql_close(mysql);
        exit(EXIT_FAILURE);
    }

    printf("Added MQTT user %s.\n", user);

    free(query);
}

char* generate_random_password()
{
    uuid_t uuid;
    char *uuid_str;

    uuid_generate(uuid);

    uuid_str = malloc (sizeof (char) * 37);
    uuid_unparse_lower(uuid, uuid_str);

    return uuid_str;
}

int main()
{
    subscribe_to_signals();

    char *mqtt_password = generate_random_password();

    MYSQL *mysql = mqtt_db_init();
    mqtt_db_connect(mysql);
    add_mqtt_user(mysql, MQTT_MODULE_CLIENT_ID, mqtt_password);

    MQTTClient mqtt_client;
    init_mqtt_client(&mqtt_client);

    MQTT_CONNECTION_INFO info = { .mqtt_client = &mqtt_client, .username = MQTT_MODULE_CLIENT_ID, .password = mqtt_password };

    set_mqtt_callbacks(&mqtt_client, &info);
    connect_to_mqtt_broker(&mqtt_client, MQTT_MODULE_CLIENT_ID, mqtt_password);
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

    mysql_close(mysql);
    free(mqtt_password);

    return 0;
}
