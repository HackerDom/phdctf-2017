#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <microhttpd.h>
#include "MQTTClient.h"

#include <sys/timeb.h>

#define PORT        8888
#define ADDRESS     "tcp://mqtt-broker:1883"
#define CLIENTID    "MODULE1"
#define QOS         1
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

void delivered(void *context, MQTTClient_deliveryToken dt)
{
    printf("Message with token value %d delivery confirmed\n", dt);
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    int i;
    char* payloadptr;

    printf("Message arrived\n");
    printf("     topic: %s\n", topicName);
    printf("   message: ");

    payloadptr = message->payload;
    for(i=0; i<message->payloadlen; i++)
    {
        putchar(*payloadptr++);
    }
    putchar('\n');
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void connlost(void *context, char *cause)
{
    printf("\nConnection lost\n");
    printf("     cause: %s\n", cause);
}

int process_message(MQTTClient* client)
{
    char* topicName = NULL;
    int topicLen;
    MQTTClient_message* message = NULL;
    int rc;

    rc = MQTTClient_receive(client, &topicName, &topicLen, &message, 1000);
    
    if (message)
    {
	printf("%s\t", topicName);
	printf("%.*s", message->payloadlen, (char*)message->payload);
	MQTTClient_freeMessage(&message);
	MQTTClient_free(topicName);
    }

    return rc;
}

int main ()
{
    MQTTClient client;
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    //MQTTClient_deliveryToken deliveredtoken;
    int rc;
    int ch;

    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 0;

    MQTTClient_setCallbacks(client, NULL, NULL, msgarrvd, NULL);

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        exit(EXIT_FAILURE);
    }

    rc = MQTTClient_subscribe(client, "#", 0);

    sleep(20000);

    rc = MQTTClient_unsubscribe(client, "#");

    //do
    //{
        //rc = process_message(&client);
        //printf("Processing result: %d\n", rc);

        //if (rc == -1)
        //{
        //    MQTTClient_connect(client, &conn_opts);
        //}
    //}
    //while(1);

    //struct MHD_Daemon *daemon;

    //daemon = MHD_start_daemon (MHD_USE_SELECT_INTERNALLY, PORT, NULL, NULL, &answer_to_connection, NULL, MHD_OPTION_END);
    //if (NULL == daemon) return 1;

    //MHD_stop_daemon (daemon);

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);

    return 0;
}
