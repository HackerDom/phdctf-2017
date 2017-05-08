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
#define SENSOR_LIST_SIZE            100
#define GET                         0
#define POST                        1
#define POSTBUFFERSIZE              2048

char *sensor_list[SENSOR_LIST_SIZE] = {0};
int current_sensor_list_element = 0;

MQTTClient mqtt_client;
MYSQL *mysql_client;

int send_auth_fail_response(struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;

    const char *page = "<html><body>Go away.</body></html>";
    response = MHD_create_response_from_buffer(strlen(page), (void *)page, MHD_RESPMEM_PERSISTENT);
    ret = MHD_queue_basic_auth_fail_response(connection, "my realm", response);
    MHD_destroy_response(response);

    return ret;
}

int check_auth(struct MHD_Connection *connection)
{
    char *user;
    char *pass;
    int success;

    pass = NULL;
    user = MHD_basic_auth_get_username_password(connection, &pass);
    success = (user != NULL) && (pass != NULL) && (0 == strcmp(user, "admin")) && (0 == strcmp(pass, "webrelay"));

    if (user != NULL)
        free(user);

    if (pass != NULL)
        free(pass);

    return success;
}

int send_page(const char *page, struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;

    response = MHD_create_response_from_buffer(strlen (page), (void*) page, MHD_RESPMEM_PERSISTENT);
    ret = MHD_queue_response(connection, MHD_HTTP_OK, response);
    MHD_destroy_response(response);

    return ret;
}

int send_home_page(struct MHD_Connection *connection)
{
    const char *page = "<html><body><ul><li><a href=\"/sensors\">Authorize temperature sensor</a></li><li><a href=\"/clients\">Authorize client</a></li><li><a href=\"/api/generate\">Generate API key</a></li></ul></body></html>";
    return send_page(page, connection);
}

int redirect_to_home_page(struct MHD_Connection *connection)
{
    int ret;
    struct MHD_Response *response;

    const char *page = "<html><body><b>302 Found</b></body></html>";
    response = MHD_create_response_from_buffer(strlen (page), (void*) page, MHD_RESPMEM_PERSISTENT);
    MHD_add_response_header(response, MHD_HTTP_HEADER_LOCATION, "/");
    ret = MHD_queue_response (connection, MHD_HTTP_FOUND, response);
    MHD_destroy_response(response);

    return ret;
}

int send_sensors_page(struct MHD_Connection *connection)
{
    int i;
    char page[1024 + SENSOR_LIST_SIZE*100];
    strcpy(page, "<html><body><form method='POST'><table><tr><td>Sensor ID:</td><td><select name='sensor'>");

    for (i = 0; i < SENSOR_LIST_SIZE; i++)
    {
        if (sensor_list[i] != NULL)
        {
            strcat(page, "<option value='");
            strcat(page, sensor_list[i]);
            strcat(page, "'>");
            strcat(page, sensor_list[i]);
            strcat(page, "</option>");
        }
    }
    strcat(page, "</select></td></tr><tr><td>Auth token:</td><td><input type='text' name='token'/></td></tr><td colspan='2'><input type='submit' value='submit'/></td></tr></table></form></body></html>");

    return send_page(page, connection);
}

struct CONNECTION_HTTP_INFO {
  int connection_type;
  struct MHD_PostProcessor* post_processor;

  char *sensor_id;
  char *auth_token;
};

int iterate_post(void *coninfo_cls, enum MHD_ValueKind kind, const char *key,
                 const char *filename, const char *content_type,
                 const char *transfer_encoding, const char *data, uint64_t off,
                 size_t size)
{
    struct CONNECTION_HTTP_INFO* con_info = coninfo_cls;

    if (size > 0)
    {
        if (0 == strcmp(key, "sensor"))
        {
            con_info->sensor_id = strdup(data);
        }
        else if (0 == strcmp(key, "token"))
        {
            con_info->auth_token = strdup(data);
        }
    }

    return MHD_YES;
}

void request_completed(void* cls, struct MHD_Connection* connection,
                       void** con_cls, enum MHD_RequestTerminationCode toe)
{
    struct CONNECTION_HTTP_INFO* con_info = *con_cls;
    if (NULL == con_info)
        return;

    if (con_info->connection_type == POST)
    {
        MHD_destroy_post_processor(con_info->post_processor);

        if (con_info->sensor_id)
            free(con_info->sensor_id);

        if (con_info->auth_token)
            free(con_info->auth_token);
    }

    free(con_info);
    con_info = NULL;
    *con_cls = NULL;
}

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

int answer_to_connection (void *cls, struct MHD_Connection *connection,
                          const char *url,
                          const char *method, const char *version,
                          const char *upload_data,
                          size_t *upload_data_size, void **con_cls)
{
    if (NULL == *con_cls)
    {
        struct CONNECTION_HTTP_INFO* con_info;
        con_info = malloc (sizeof (struct CONNECTION_HTTP_INFO));
        if (NULL == con_info)
            return MHD_NO;

        if (0 == strcmp(method, "POST"))
        {
            con_info->post_processor = MHD_create_post_processor(connection, POSTBUFFERSIZE, iterate_post, (void*) con_info);
            if (NULL == con_info->post_processor) 
            {
                free(con_info);
                return MHD_NO;
            }

            con_info->connection_type = POST;
        }
        else
            con_info->connection_type = GET;

        *con_cls = (void *)con_info;
        return MHD_YES;
    }

    if (!check_auth(connection))
        return send_auth_fail_response(connection);

    if (0 == strcmp(url, "/"))
    {
        if (0 != strcmp(method, MHD_HTTP_METHOD_GET))
            return MHD_NO;

        return send_home_page(connection);
    }

    if (strcmp(url, "/sensors") == 0)
    {
        if (0 == strcmp(method, MHD_HTTP_METHOD_GET))
            return send_sensors_page(connection);

        if (0 == strcmp(method, MHD_HTTP_METHOD_POST))
        {
            struct CONNECTION_HTTP_INFO* con_info = *con_cls;
            if (0 != *upload_data_size)
            {
                MHD_post_process(con_info->post_processor, upload_data, *upload_data_size);
                *upload_data_size = 0;
                return MHD_YES;
            }

            if (con_info->sensor_id != NULL && con_info->auth_token != NULL)
            {
                printf("Sensor id: '%s', auth_token: '%s'.\n", con_info->sensor_id, con_info->auth_token);

                add_mqtt_user(mysql_client, con_info->sensor_id, con_info->auth_token);

                char *client_authorization_topic = (char *)malloc((strlen("house/authorization/") + strlen(con_info->sensor_id) + 1) * sizeof(char));
                sprintf(client_authorization_topic, "house/authorization/%s", con_info->sensor_id);
                publish_message(mqtt_client, client_authorization_topic, con_info->auth_token, strlen(con_info->auth_token) + 1);
                free(client_authorization_topic);
 
                return redirect_to_home_page(connection);
            }
        }
    }

    return MHD_NO;
}

typedef struct MQTT_CONNECTION_INFO {
  MQTTClient *mqtt_client;
  char *username;
  char *password;
} MQTT_CONNECTION_INFO;

int process_mqtt_message(void *context, char *topic_name, int topic_len, MQTTClient_message *message)
{
    if (strcmp(topic_name, AUTHORIZATION_REQUEST_TOPIC) == 0) 
    {
        printf("Authorization request for client_id '%s'.\n", message->payload);

        if (context != NULL)
        {
            MQTTClient* mqtt_client = ((MQTT_CONNECTION_INFO*)context)->mqtt_client;

            int i;
            for (i = 0; i < SENSOR_LIST_SIZE; i++)
            {
                if (sensor_list[i] != NULL && strcmp(sensor_list[i], message->payload) == 0)
                {
                    printf("Authorization request for client_id '%s' already exists.\n");
                    goto end_processing;
                }
            }

            if (sensor_list[current_sensor_list_element] != NULL)
                free(sensor_list[current_sensor_list_element]);
            sensor_list[current_sensor_list_element] = strdup(message->payload);

            if (current_sensor_list_element + 1 < SENSOR_LIST_SIZE)
                current_sensor_list_element++;
            else
                current_sensor_list_element = 0;
       }
    }
    else
    {
        printf("Message from topic '%s' arrived.\n", topic_name);
    }

end_processing:
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

    mysql_client = mqtt_db_init();
    mqtt_db_connect(mysql_client);
    add_mqtt_user(mysql_client, MQTT_MODULE_CLIENT_ID, mqtt_password);

    init_mqtt_client(mqtt_client);

    MQTT_CONNECTION_INFO info = { .mqtt_client = &mqtt_client, .username = MQTT_MODULE_CLIENT_ID, .password = mqtt_password };

    set_mqtt_callbacks(&mqtt_client, &info);
    connect_to_mqtt_broker(&mqtt_client, MQTT_MODULE_CLIENT_ID, mqtt_password);
    subscribe_to_topic(&mqtt_client, AUTHORIZATION_REQUEST_TOPIC);

    struct MHD_Daemon *daemon;
    daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, HTTP_SERVER_PORT, NULL, NULL, &answer_to_connection, NULL, MHD_OPTION_NOTIFY_COMPLETED, &request_completed, NULL, MHD_OPTION_END);
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

    mysql_close(mysql_client);
    free(mqtt_password);

    return 0;
}
