
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

#define MULTICAST_ADDR "FF02::1:FF00:1"  // Multicast-Adresse
#define PORT 12345                      // Multicast-Port
#define TIMEOUT_MS 300                  // Timeout in Millisekunden
#define DATA_SIZE 1024                  // Gr��e der Nutzdaten
typedef struct {
    int seq_num;
    char data[DATA_SIZE];
    int timer;
    int ack;
} packet;
packet* packets = NULL;
int filesize = 0;
int fenstergroesse = 1;
int fensterstart = 0;
int fensterende = 0;

void error(const char* msg) {
    perror(msg);
    exit(1);
}
void fileReader(char* filename) {
    char ch;
    int size = 0;
    FILE* file = fopen(filename, "r");
    if (file == NULL) {
        error("File not found");
    }
    //hier werden die Anzahl der Zeilen gez�hlt
    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\n') {
            filesize++;
            continue;
        }
    }
    packets = (packet*)malloc(filesize * sizeof(packet));
    rewind(file);
    for (int i = 0; i < filesize; i++) {
        fgets(packets[i].data, 256, file);
        packets[i].seq_num = i;
    }
    fclose(file);
}
void printPackets() {
    for (int i = 0; i < sizeof(packets); i++) {
        printf("Packet %d: %s, %i\n", packets[i].seq_num, packets[i].data, packets[i].ack);
    }
}
void sende_packete(int socket, struct sockaddr_in6* multicast_addr, socklen_t add_len) {
    for (int i = fensterstart; i < fensterende; i++) {
        if (!packets[i].ack && packets[i].timer == 0) {
            char data[DATA_SIZE];
            snprintf(data, DATA_SIZE, "SEQ:%d %s", packets[i].seq_num, packets[i].data);
            if (sendto(socket, data, strlen(data), 0, (struct sockaddr*)multicast_addr, add_len) < 0) {
                error("Sendto failed");
            }
            printf("Sent: %s\n", data);
            packets[i].timer = TIMEOUT_MS;
        }
    }
}

void verschiebe_fenster() {
    printf("Verschiebe Fenster: Start=%d, Ende=%d\n", fensterstart, fensterende);

    while (fensterstart < fensterende && packets[fensterstart].ack) {
        printf("Fensterstart verschoben von %d auf %d\n", fensterstart, fensterstart + 1);
        fensterstart++;
    }

    // Fensterende aktualisieren
    fensterende = fensterstart + fenstergroesse;
    if (fensterende > filesize) {
        fensterende = filesize;
    }
    printf("Fenster aktualisiert: Start=%d, Ende=%d\n", fensterstart, fensterende);
}
void handle_ack_nack(int socket, struct sockaddr_in6* multicast_addr, socklen_t add_len) {
    char buffer[DATA_SIZE];
    int n = recvfrom(socket, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)multicast_addr, &add_len);
    printf("Received: %s\n", buffer);
    if (n > 0) {
        buffer[n] = '\0';  // Null-terminieren
        int nack_seq;
        if (sscanf(buffer, "NACK:%d", &nack_seq) == 1) {
            for (int i = fensterstart; i < fensterende; i++) {
                if (packets[i].seq_num == nack_seq) {
                    printf("Resending packet SEQ:%d due to NACK\n", nack_seq);
                    packets[i].timer = 0; // Timer auf 0 setzen, um es sofort zu senden
                    break;
                }
            }
        }
        else if (sscanf(buffer, "ACK:%d", &nack_seq) == 1) {
            for (int i = fensterstart; i <= fensterende; i++) {
                if (packets[i].seq_num == nack_seq) {
                    packets[i].ack = 1;
                    printf("Packet SEQ:%d acknowledged\n", nack_seq);
                    break;
                }
            }
        }
    }
}

void update_timers() {
    for (int i = fensterstart; i < fensterende; i++) {
        if (packets[i].timer > 0) {
            packets[i].timer--;
        }
    }
}

int main(int argc, char* argv[]) {
    char multicast_addrr[] = MULTICAST_ADDR;
    char filename[] = "data.txt";
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-a") == 0) {
            if (i + 1 < argc) {
                strcpy(multicast_addrr, argv[i + 1]);

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
    printf("Multicast-Adresse: %s\n", multicast_addrr);
    printf("Fenstergroesse: %d\n", fenstergroesse);
    int sock;
    fileReader(filename);
    printPackets();
    struct sockaddr_in6 multicast_addr, recv_addr;
    char message[] = "hi";
    char buffer[1024];
    char data[DATA_SIZE];
    socklen_t recv_addr_len = sizeof(recv_addr);
    struct timeval timeout;
    int seq_num = 0;  // Startsequenznummer


    // Socket erstellen
    sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    // Timeout f�r recvfrom setzen
    timeout.tv_sec = 0;
    timeout.tv_usec = TIMEOUT_MS * 1000;  // Umrechnung in Mikrosekunden
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Setting socket timeout failed");
        close(sock);
        exit(1);
    }

    // Multicast-Adresse und Port einstellen
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin6_family = AF_INET6; // IPv6 verwenden
    multicast_addr.sin6_port = htons(PORT);// Port einstellen
    inet_pton(AF_INET6, multicast_addrr, &multicast_addr.sin6_addr);// Multicast-Adresse einstellen

    while (1) {
        // Nachricht senden
        if (sendto(sock, message, strlen(message), 0, (struct sockaddr*)&multicast_addr, sizeof(multicast_addr)) < 0) {
            perror("Sendto failed");
            close(sock);
            exit(1);
        }
        printf("Sent: %s\n", message);

        // Auf Antwort warten
        int n = recvfrom(sock, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&recv_addr, &recv_addr_len);
        if (n < 0) {
            // Timeout abgelaufen
            perror("No response received, resending...");
        }
        else {
            // Antwort erhalten
            buffer[n] = '\0';  // Null-terminieren
            printf("Received: %s\n", buffer);

            // Antwort validieren (optional)
            if (strcmp(buffer, "ACK") == 0) {
                printf("Acknowledgment received, sending next sequence.\n");
                break;  // Nachricht erfolgreich empfangen, weitere Logik einf�gen
            }
        }

        // Warte kurz vor erneutem Senden (optional)
        usleep(100000);  // 100 ms
    }
    fensterende = fensterstart + fenstergroesse;
    if (fensterende > filesize) {
        fensterende = filesize;
    }
    while (fensterstart < filesize) {
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(sock, &read_fds);

        timeout.tv_sec = 0;
        timeout.tv_usec = TIMEOUT_MS * 1000;

        int ready = select(sock + 1, &read_fds, NULL, NULL, &timeout);
        if (ready > 0 && FD_ISSET(sock, &read_fds)) {
            handle_ack_nack(sock, &recv_addr, recv_addr_len);
        }
        else {
            update_timers();
            sende_packete(sock, &multicast_addr, recv_addr_len);
            verschiebe_fenster();
            //printPackets();
        }
    }
    if(sendto(sock, "close", strlen("close"), 0, (struct sockaddr*)&multicast_addr, sizeof(multicast_addr)) < 0) {
		perror("Sendto failed");
		close(sock);
		exit(1);
	}
    printf("Sent: close\n");
    printf("Transmission complete. Exiting sender loop.\n");
    close(sock);
    return 0;
}