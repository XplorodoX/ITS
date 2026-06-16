#ifndef NETWORK_H
#define NETWORK_H

#include <ESP8266WiFi.h>

bool resolveBrokerIP(IPAddress& brokerIP);
void mqttCallback(char* topic, byte* payload, unsigned int length);
bool mqttReconnect();
void publishConnect();
void connectMqttAsPlayer();
bool handleConnectionLoss();
void checkConnection();

#endif // NETWORK_H
