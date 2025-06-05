#ifndef MQTT_CLIENT_H
#define MQTT_CLIENT_H

int mqtt_init(const char *address, const char *client_id);
int mqtt_publish(const char *topic, const char *payload);
void mqtt_subscribe(const char *topic);
void mqtt_cleanup();

#endif // MQTT_CLIENT_H