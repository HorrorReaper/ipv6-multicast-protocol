//main function
//UDP Protokoll, also werden wir keine Verbindung aufbauen sondern nur Daten senden
/*#include <stdio.h>
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
    struct sockaddr_in6 multicast_addr, sender_addr;
    char data[DATA_SIZE]; 
    char buffer[DATA_SIZE];
    int seq_num = 0;  // Startsequenznummer

    // Socket erstellen
    sock = socket(AF_INET6, SOCK_DGRAM, 0);
    if (sock < 0) error("Socket creation failed");

    // Multicast-Adresse vorbereiten
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin6_family = AF_INET6;
    multicast_addr.sin6_port = htons(PORT);
    inet_pton(AF_INET6, MULTICAST_ADDR, &multicast_addr.sin6_addr);
    int readyToSend = 0;
    snprintf(data, DATA_SIZE, "hi");
    while (readyToSend == 0) {
        sendto(sock, data, strlen(data), 0, (struct sockaddr*)&multicast_addr, sizeof(multicast_addr));
        //hier soll eine Antwort vom receiver erfolgen
        int n = recvfrom(sock, buffer, DATA_SIZE, 0, (struct sockaddr*)&sender_addr, &sender_addr_len);
        if (n!=0)
        {
            readyToSend = 1;
        }
    }
    // Daten senden
    while (1) {
        //Data soll erstmal eine normale Nachricht sein, damit der Sender es dem Empfänger sendet und der Empfänger es bestätigt, bevor die eigentlichten Daten gesendet werden
        
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
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/time.h>

#define MULTICAST_ADDR "FF02::1:FF00:1"  // Multicast-Adresse
#define PORT 12345                      // Multicast-Port
#define TIMEOUT_MS 300                  // Timeout in Millisekunden
#define DATA_SIZE 1024                  // Größe der Nutzdaten
typedef struct{
	int seq_num;
	char data[DATA_SIZE];
} packet;
packet* packets = NULL;
int filesize = 0;
int fenstergröße = 1;

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
    //hier werden die Anzahl der Zeilen gezählt
    while ((ch = fgetc(file)) != EOF) {
        if (ch == '\n') {
            filesize++;
            continue;
        }
    }
    packets = (packet*)malloc(filesize * sizeof(packet));
    rewind(file);
    for(int i = 0; i < filesize; i++) {
		fgets(packets[i].data, 256, file);
        packets[i].seq_num = i;
    }
    fclose(file);
}
void printPackets() {
	for(int i = 0; i < sizeof(packets); i++) {
		printf("Packet %d: %s\n", packets[i].seq_num, packets[i].data);
	}
}
int main() {
    int sock;
    char filename[] = "data.txt";
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

    // Timeout für recvfrom setzen
    timeout.tv_sec = 0;
    timeout.tv_usec = TIMEOUT_MS * 1000;  // Umrechnung in Mikrosekunden
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
        perror("Setting socket timeout failed");
        close(sock);
        exit(1);
    }

    // Multicast-Adresse und Port einstellen
    memset(&multicast_addr, 0, sizeof(multicast_addr));
    multicast_addr.sin6_family = AF_INET6;
    multicast_addr.sin6_port = htons(PORT);
    inet_pton(AF_INET6, MULTICAST_ADDR, &multicast_addr.sin6_addr);

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
                break;  // Nachricht erfolgreich empfangen, weitere Logik einfügen
            }
        }

        // Warte kurz vor erneutem Senden (optional)
        usleep(100000);  // 100 ms
    }
    while (seq_num < filesize) {
        
        // Nutzdaten mit Sequenznummer vorbereiten
        //snprintf(data, DATA_SIZE, "SEQ:%d Hello Multicast!", seq_num);
        snprintf(data, DATA_SIZE, "SEQ:%d %s", packets[seq_num].seq_num, packets[seq_num].data);

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
