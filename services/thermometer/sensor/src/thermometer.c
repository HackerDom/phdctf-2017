#include "stdio.h"
#include "stdlib.h"
#include "string.h"
#include "MQTTClient.h"
#include <uuid/uuid.h>
#include <math.h>

#define ADDRESS     "tcp://mqtt-broker:1883"
#define TOPIC       "house/kitchen/temperature"
#define PAYLOAD     "Hello World!"
#define QOS         1
#define TIMEOUT     10000L

#define MQTT_

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

int initialize_user_password()
{
}

int init(char* client_id)
{
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;

    MQTTClient_create(&client, ADDRESS, client_id, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        return -1;
    }

    pubmsg.payload = PAYLOAD;
    pubmsg.payloadlen = strlen(PAYLOAD);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    char *init_topic = (char *)malloc(51 * sizeof(char));
    sprintf(init_topic, "house/modules/%s", client_id);
    MQTTClient_publishMessage(client, init_topic, &pubmsg, &token);

    printf("Waiting for up to %d seconds for publication of %s\n"
            "on topic %s for client with ClientID: %s\n",
            (int)(TIMEOUT/1000), PAYLOAD, init_topic, client_id);
    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    printf("Message with delivery token %d delivered\n", token);

    MQTTClient_disconnect(client, TIMEOUT);
    MQTTClient_destroy(&client);
    free(init_topic);
    return rc;
}

int generate_init_temperature(const int min, const int max)
{
    return max + rand() / (RAND_MAX / (min - max + 1) + 1);
}

int generate_next_temperature(const int current, const int min, const int max)
{
   return fmin(fmax(current + round(((float)rand()/(float)(RAND_MAX)) * 2.0 - 1), min), max);
}

int main(int argc, char* argv[])
{
    char* client_id = generate_random_id();
    //init(client_id);
    //free(client_id);
    const int min_temperature = 10;
    const int max_temperature = 40;
    int temperature = generate_init_temperature(min_temperature, max_temperature);
    printf("Temperature: %d\n", temperature);

    int i;
    for (i = 0; i < 100; i++) {
        temperature = generate_next_temperature(temperature, min_temperature, max_temperature);
        printf("Next temperature: %d\n", temperature);
    }
    return 0;
}

/*
int main(int argc, char* argv[])
{
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;
    int rc;

    MQTTClient_create(&client, ADDRESS, CLIENTID,
        MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    conn_opts.username = "temperature_sensor";
    conn_opts.password = "test";

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        exit(-1);
    }
    pubmsg.payload = PAYLOAD;
    pubmsg.payloadlen = strlen(PAYLOAD);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token);
    printf("Waiting for up to %d seconds for publication of %s\n"
            "on topic %s for client with ClientID: %s\n",
            (int)(TIMEOUT/1000), PAYLOAD, TOPIC, CLIENTID);
    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    printf("Message with delivery token %d delivered\n", token);
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return rc;
}
*/
