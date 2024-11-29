//main function
//UDP Protokoll, also werden wir keine Verbindung aufbauen sondern nur Daten senden
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MULTICAST_ADDR "FF02::1:FF00:1"  // IPv6-Multicast-Adresse
#define PORT 12345                      // Multicast-Port
#define DATA_SIZE 1024                  // Größe der Nutzdaten

void error(const char* msg) {
    perror(msg);
    exit(1);
}

int main() {
    int sock;
    struct sockaddr_in6 multicast_addr;
    char data[DATA_SIZE];
    int seq_num = 0;  // Startsequenznummer

    // Socket erstellen
    sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) error("Socket creation failed");

    // Multicast-Adresse vorbereiten
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin6_family = AF_INET6;
    multicast_addr.sin6_port = htons(PORT);
    inet_pton(AF_INET6, MULTICAST_ADDR, &multicast_addr.sin6_addr);

    // Daten senden
    while (1) {
        // Nutzdaten mit Sequenznummer vorbereiten
        snprintf(data, DATA_SIZE, "SEQ:%d Hello Multicast!", seq_num);

        // Paket senden
        if (sendto(sock, data, strlen(data), 0, (struct sockaddr*)&multicast_addr, sizeof(multicast_addr)) < 0) {
            error("Sendto failed");
        }
        printf("Sent: %s\n", data);

        seq_num++;  // Nächste Sequenznummer
        sleep(1);   // Sendeintervall
    }

    close(sock);
    return 0;
}
