/*!
 * EasyNextionMultiStream.h - Easy library for sending the serial output to multiple clients (including Telnet)
 * Copyright (c) 2025 Christophe Belmont
 * All rights reserved under the library's licence
 */
#ifndef EasyNextionMultiStrem_h
#define EasyNextionMultiStrem_h
#include "EasyNextionLibrary.h"
#include <WiFi.h>
#include <WiFiClient.h>

#define MAX_CLIENTS 5

// === Classe MultiStream ===
class MultiStream : public Print {
public:
    MultiStream(Stream* serialStream) : _serial(serialStream) {}

    void addClient(WiFiClient client) {
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (!_clients[i] || !_clients[i].connected()) {
                _clients[i] = client;
                break;
            }
        }
    }

    void handleClients() {
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (_clients[i] && !_clients[i].connected()) {
                _clients[i].stop();
            }
        }
    }

    virtual size_t write(uint8_t c) override {
        size_t n = _serial->write(c);
        for (int i = 0; i < MAX_CLIENTS; ++i) {
            if (_clients[i] && _clients[i].connected()) {
                _clients[i].write(c);
            }
        }
        return n;
    }

    using Print::write;

private:
    Stream* _serial;
    WiFiClient _clients[MAX_CLIENTS];
};


/*
void setup() {
    Serial.begin(115200);
    delay(1000);

    // Connexion Wi-Fi
    WiFi.begin(ssid, password);
    Serial.print("Connexion à ");
    Serial.println(ssid);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("\nWiFi connecté !");
    Serial.print("Adresse IP : ");
    Serial.println(WiFi.localIP());

    // Démarrage du serveur Telnet
    telnetServer.begin();
    telnetServer.setNoDelay(true);
    Serial.println("Serveur Telnet démarré.");
}

void loop() {
    // Gestion des nouveaux clients Telnet
    if (telnetServer.hasClient()) {
        WiFiClient newClient = telnetServer.available();
        if (newClient) {
            output.addClient(newClient);
            output.println("[Telnet] Nouveau client connecté.");
        }
    }

    // Nettoyage des clients déconnectés
    output.handleClients();

    // Exemple de message
    static unsigned long lastMsg = 0;
    if (millis() - lastMsg > 2000) {
        lastMsg = millis();
        output.println("Message périodique vers Serial + Telnet");
    }
}
*/
#endif // EasyNextionMultiStrem_h