#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

#define MULTICAST_ADDR "FF02::1:FF00:1"  // IPv6-Multicast-Adresse
#define PORT 12345                      // Multicast-Port
#define DATA_SIZE 1024                  // Größe der Nutzdaten
int fenstergroesse = 1;
void error(const char* msg) {
    perror(msg);
    exit(1);
}
void fileWriter(char* filename, char* data) {
    FILE* file = fopen(filename, "a");
    if (file == NULL) {
		file= fopen(filename, "wb");
        printf("File created\n");
	}
    fprintf(file, "%s", data);
    fclose(file);
    printf("Data written to file\n");
    }

int main(int argc, char* argv[]) {
    printf("Argc: %d\n", argc);
    for(int i = 0; i < argc; i++) {
		printf("Argv[%d]: %s\n", i, argv[i]);
	}
    char multicast_addr[50] = MULTICAST_ADDR;
    char filename[20] = "data.txt";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0) {
            if (i + 1 < argc) {
                // Multicast-Adresse setzen
                strncpy(multicast_addr, argv[i + 1], sizeof(multicast_addr) - 1);
                multicast_addr[sizeof(multicast_addr) - 1] = '\0';  // Nullterminierung sicherstellen
                printf("Multicast-Adresse: %s\n", multicast_addr);
                printf("Multicast-Adresse: %s\n", argv[i + 1]);
            }
        }
        else if (strcmp(argv[i], "-w") == 0) {
            if (i + 1 < argc) {
                fenstergroesse = atoi(argv[i + 1]);
            }
        }
        else if (strcmp(argv[i], "-f") == 0) {
            if (i + 1 < argc) {
                strcpy(filename, argv[i + 1]);
            }
        }
    }
    printf("Multicast-Adresse: %s\n", multicast_addr);
    printf("Fenstergroesse: %d\n", fenstergroesse);
    int sock;
    struct sockaddr_in6 local_addr, sender_addr;
    char buffer[DATA_SIZE];
    socklen_t sender_addr_len = sizeof(sender_addr);
    int last_seq_num = -1;  // Verfolgt die letzte empfangene Sequenznummer

    // Socket erstellen
    sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) error("Socket creation failed");

    // Lokale Adresse binden
    memset(&local_addr, 0, sizeof(local_addr));
    local_addr.sin6_family = AF_INET6;
    local_addr.sin6_port = htons(PORT);
    local_addr.sin6_addr = in6addr_any;

    if (bind(sock, (struct sockaddr*)&local_addr, sizeof(local_addr)) < 0) {
        error("Bind failed");
    }

    // Multicast-Gruppe beitreten
    struct ipv6_mreq mreq;
    inet_pton(AF_INET6, multicast_addr, &mreq.ipv6mr_multiaddr);
    mreq.ipv6mr_interface = 0;  // Standardnetzwerkschnittstelle
    if (setsockopt(sock, IPPROTO_IPV6, IPV6_JOIN_GROUP, &mreq, sizeof(mreq)) < 0) {
        error("Joining multicast group failed");
    }
    while (1) {
        if (strcmp(buffer, "hi") == 0) {
            char ack_msg[] = "ACK";
            sendto(sock, ack_msg, strlen(ack_msg), 0, (struct sockaddr*)&sender_addr, sender_addr_len);
            printf("Sent ACK\n");
        }
        int n = recvfrom(sock, buffer, DATA_SIZE, 0, (struct sockaddr*)&sender_addr, &sender_addr_len);
        if (n < 0) error("Recvfrom failed");

        buffer[n] = '\0';  // Null-Terminierung
        printf("Received: %s\n", buffer);
        fileWriter(filename, buffer);

        // Sequenznummer extrahieren
        int seq_num = 0;
        sscanf(buffer, "SEQ:%d", &seq_num);

        // Prüfen, ob Pakete fehlen
        if (seq_num > last_seq_num + 1) {
            printf("Missing packet(s) from %d to %d\n", last_seq_num + 1, seq_num - 1);

            // NACK senden (Vereinfachung: Wir senden nur die erste fehlende Sequenznummer)
            char nack_msg[50];
            snprintf(nack_msg, sizeof(nack_msg), "NACK:%d", last_seq_num + 1);
            if (sendto(sock, nack_msg, strlen(nack_msg), 0, (struct sockaddr*)&sender_addr, sender_addr_len) < 0) {
                error("Sendto (NACK) failed");
            }
            printf("Sent NACK for packet %d\n", last_seq_num + 1);
        }

        last_seq_num = seq_num;  // Sequenznummer aktualisieren
    }

    close(sock);
    return 0;
}
