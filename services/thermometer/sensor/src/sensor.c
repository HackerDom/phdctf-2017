#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "MQTTClient.h"
#include <uuid/uuid.h>
#include <math.h>
#include <signal.h>

#define MQTT_BROKER_ADDRESS     "tcp://mqtt-broker:1883"
#define TEMPERATURE_TOPIC       "house/kitchen/temperature"
#define QOS         1
#define TIMEOUT     10000L
#define AUTHORIZATION_REQUEST_TOPIC "house/authorization"
#define AUTHORIZATION_RESPONSE_TOPIC_PREFIX "house/authorization/"

char* generate_random_id()
{
    uuid_t uuid;
    char *uuid_str;

    uuid_generate(uuid);

    uuid_str = malloc (sizeof (char) * 37);
    uuid_unparse_lower(uuid, uuid_str);

    printf("Generate client_id=%s\n", uuid_str);
    return uuid_str;
}

int generate_init_temperature(const int min, const int max)
{
    return max + rand() / (RAND_MAX / (min - max + 1) + 1);
}

int generate_next_temperature(const int current, const int min, const int max)
{
    return fmin(fmax(current + round(((float)rand()/(float)(RAND_MAX)) * 2.0 - 1), min), max);
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

volatile char* password = NULL;

int process_mqtt_message(void *context, char *topic_name, int topic_len, MQTTClient_message *message)
{
    if (strncmp(AUTHORIZATION_RESPONSE_TOPIC_PREFIX, topic_name, strlen(AUTHORIZATION_RESPONSE_TOPIC_PREFIX)) == 0)
    {
        printf("Authorization response: '%s'.\n", message->payload);
        password = strdup(message->payload);
    }
    else
        printf("Message from topic '%s' arrived.\n", topic_name);

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topic_name);

    return 1;
}

void connect_to_mqtt_broker(MQTTClient* mqtt_client)
{
    int rc;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.connectTimeout = 10;

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

void init_mqtt_client(MQTTClient* mqtt_client, const char* client_id)
{
    int rc;

    if ((rc = MQTTClient_create(mqtt_client, MQTT_BROKER_ADDRESS, client_id, MQTTCLIENT_PERSISTENCE_NONE, NULL)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to create MQTT client, return code %d.\n", rc);
        exit(EXIT_FAILURE);
    }
}

volatile MQTTClient_deliveryToken deliveredtoken;
void process_delivered(void *context, MQTTClient_deliveryToken dt)
{
    printf("Message with token value %d delivery confirmed\n", dt);
    deliveredtoken = dt;
}

void set_mqtt_callbacks(MQTTClient* mqtt_client)
{
    int rc;

    if ((rc = MQTTClient_setCallbacks(*mqtt_client, mqtt_client, process_connection_lost, process_mqtt_message, process_delivered)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to set callbacks for MQTT client, return code %d.\n", rc);
        exit(EXIT_FAILURE);
    }
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

int main(int argc, char* argv[])
{
    subscribe_to_signals();

    char* client_id = generate_random_id();

    MQTTClient anonymous_mqtt_client;
    init_mqtt_client(&anonymous_mqtt_client, client_id);

    set_mqtt_callbacks(&anonymous_mqtt_client);
    connect_to_mqtt_broker(&anonymous_mqtt_client);

    char *password_topic = (char *)malloc((strlen(AUTHORIZATION_RESPONSE_TOPIC_PREFIX) + strlen(client_id) + 1) * sizeof(char));
    sprintf(password_topic, "%s%s", AUTHORIZATION_RESPONSE_TOPIC_PREFIX, client_id);
    subscribe_to_topic(&anonymous_mqtt_client, password_topic);

    do
    {
        publish_message(&anonymous_mqtt_client, AUTHORIZATION_REQUEST_TOPIC, client_id, strlen(client_id) + 1);
        sleep(1);
    }
    while(!stop && password == NULL);

    if (password != NULL)
    {
        unsubscribe_from_topic(&anonymous_mqtt_client, password_topic);
        free((void *)password);
    }

    free(password_topic);

    disconnect_from_mqtt_broker(&anonymous_mqtt_client);
    MQTTClient_destroy(&anonymous_mqtt_client);
    free(client_id);

    /*
    //const int min_temperature = 10;
    //const int max_temperature = 40;
    int temperature = generate_init_temperature(min_temperature, max_temperature);
    printf("Temperature: %d\n", temperature);

    int i;
    for (i = 0; i < 100; i++) {
        temperature = generate_next_temperature(temperature, min_temperature, max_temperature);
        printf("Next temperature: %d\n", temperature);
    }
    */
    return 0;
}
