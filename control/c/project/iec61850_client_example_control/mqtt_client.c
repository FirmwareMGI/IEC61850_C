#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "MQTTClient.h"
#include "mqtt_client.h"

#define QOS_PUBLISH 1
#define QOS_SUBSCRIBE 2
#define TIMEOUT 10000L

static MQTTClient client;

void message_arrived_callback(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
  printf("Message arrived on topic %s: %.*s\n", topicName, message->payloadlen, (char *)message->payload);
  MQTTClient_freeMessage(&message);
  MQTTClient_free(topicName);
}

int mqtt_init(const char *address, const char *client_id)
{
  MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
  int rc;

  MQTTClient_create(&client, address, client_id,
                    MQTTCLIENT_PERSISTENCE_NONE, NULL);

  conn_opts.keepAliveInterval = 20;
  conn_opts.cleansession = 1;

  rc = MQTTClient_connect(client, &conn_opts);
  if (rc != MQTTCLIENT_SUCCESS)
  {
    fprintf(stderr, "MQTT: Failed to connect, return code %d\n", rc);
    return rc;
  }

  return 0;
}

int mqtt_publish(const char *topic, const char *payload)
{
  MQTTClient_message pubmsg = MQTTClient_message_initializer;
  MQTTClient_deliveryToken token;

  pubmsg.payload = (void *)payload;
  pubmsg.payloadlen = (int)strlen(payload);
  pubmsg.qos = QOS_PUBLISH;
  pubmsg.retained = 1;

  int rc = MQTTClient_publishMessage(client, topic, &pubmsg, &token);
  if (rc != MQTTCLIENT_SUCCESS)
  {
    fprintf(stderr, "MQTT: Publish failed, return code %d\n", rc);
    return rc;
  }

  rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
  return rc;
}

void mqtt_subscribe(const char *topic)
{
  int rc = MQTTClient_subscribe(client, topic, QOS_SUBSCRIBE);
  if (rc != MQTTCLIENT_SUCCESS)
  {
    fprintf(stderr, "Failed to subscribe to topic %s, return code %d\n", topic, rc);
  }
  else
  {
    printf("Subscribed to topic %s\n", topic);
  }
}

void mqtt_cleanup()
{
  MQTTClient_disconnect(client, 10000);
  MQTTClient_destroy(&client);
}