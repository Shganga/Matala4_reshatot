#include <stdio.h> // Standard input/output definitions
#include <arpa/inet.h> // Definitions for internet operations (inet_pton, inet_ntoa)
#include <netinet/in.h> // Internet address family (AF_INET, AF_INET6)
#include <netinet/ip.h> // Definitions for internet protocol operations (IP header)
#include <netinet/ip_icmp.h> // Definitions for internet control message protocol operations (ICMP header)
#include <poll.h> // Poll API for monitoring file descriptors (poll)
#include <errno.h> // Error number definitions. Used for error handling (EACCES, EPERM)
#include <string.h> // String manipulation functions (strlen, memset, memcpy)
#include <sys/socket.h> // Definitions for socket operations (socket, sendto, recvfrom)
#include <sys/time.h> // Time types (struct timeval and gettimeofday)
#include <unistd.h> // UNIX standard function definitions (getpid, close, sleep)
#include "ping.h" // Header file for the program (calculate_checksum function and some constants)
#include <getopt.h>
#include <stdlib.h>
#include <netinet/icmp6.h>
#include <netinet/ip6.h>
#include <signal.h>

int sent_packets = 0; // Number of sent packets
int received_packets = 0; // Number of received packets
double total_rtt = 0; // Total RTT times
double min_rtt = 1e9; // Minimum RTT
double max_rtt = 0; // Maximum RTT

// Signal handler for SIGINT (Ctrl+C)
void handle_interrupt_signal() {
    printf("--- ping statistics ---\n");
    printf("%d packets transmitted, %d received, %.2f%% packet loss\n", 
           sent_packets, 
           received_packets, 
           (sent_packets - received_packets) * 100.0 / sent_packets);

    if (received_packets > 0) {
        double avg_rtt = total_rtt / received_packets;
        double mdev = max_rtt - min_rtt;
        printf("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n", min_rtt, avg_rtt, max_rtt, mdev);
    } else {
        printf("No valid responses received.\n");
    }
    exit(0);
}

PingFlags parse_flags(int argc, char *argv[]) {
    PingFlags ping_flags = {NULL, 0, 0, 0};  // Default values
    int opt;

    while ((opt = getopt(argc, argv, "a:t:c:f")) != -1) {
        switch (opt) {
            case 'a':
                ping_flags.a = optarg;
                break;
            case 't':
                ping_flags.t = atoi(optarg);  // Convert string to int
                break;
            case 'c':
                ping_flags.c = atoi(optarg);  // Convert string to int
                break;
            case 'f':
                ping_flags.f = 1;  // Set flood mode to 1 when -f is passed
                break;
            default:
                fprintf(stderr, "Invalid flag option.\n");
                exit(EXIT_FAILURE);
        }
    }

    if (ping_flags.a == NULL) {
        fprintf(stderr, "Error: the address is required!\n");
        exit(EXIT_FAILURE);
    }

    if (ping_flags.t != 4 && ping_flags.t != 6) {
        fprintf(stderr, "Error: the type is invalid! Use 4 for IPv4 and 6 for IPv6.\n");
        exit(EXIT_FAILURE);
    }

    return ping_flags;
}


unsigned short int calculate_checksum(void *data, unsigned int bytes) {
    unsigned short int *data_pointer = (unsigned short int *)data;
    unsigned int total_sum = 0;

    while (bytes > 1) {
        total_sum += *data_pointer++;
        bytes -= 2;
    }

    if (bytes > 0) {
        total_sum += *((unsigned char *)data_pointer);
    }

    while (total_sum >> 16) {
        total_sum = (total_sum & 0xFFFF) + (total_sum >> 16);
    }

    return (~((unsigned short int)total_sum));
}

void ping(PingFlags ping_flags) {
    int socket_fd;
    struct sockaddr_in destaddr_4;
    struct sockaddr_in6 destaddr_6;
    char packet_buffer[BUFFER_SIZE];
    struct timeval start, end;
    int sequence_number = 0;

    if (ping_flags.t == 4) {
        memset(&destaddr_4, 0, sizeof(destaddr_4));    
        destaddr_4.sin_family = AF_INET;
        if (inet_pton(AF_INET, ping_flags.a, &destaddr_4.sin_addr) <= 0) {
            fprintf(stderr, "Error: \"%s\" is not a valid IPv4 address\n", ping_flags.a);
            return;
        }
        socket_fd = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
        if (socket_fd < 0) {
            perror("socket");
            exit(EXIT_FAILURE);
        }
        
    } else if (ping_flags.t == 6) {
        memset(&destaddr_6, 0, sizeof(destaddr_6));    
        destaddr_6.sin6_family = AF_INET6;
        if (inet_pton(AF_INET6, ping_flags.a, &destaddr_6.sin6_addr) <= 0) {
            fprintf(stderr, "Error: \"%s\" is not a valid IPv6 address\n", ping_flags.a);
            return;
        }
        socket_fd = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
        if (socket_fd < 0) {
            perror("socket");
            if (errno == EACCES || errno == EPERM)
                fprintf(stderr, "You need to run the program with sudo.\n");
            exit(EXIT_FAILURE);
        }
    } else {
        fprintf(stderr, "Error: the type is not valid!\n");
        exit(EXIT_FAILURE);
    }

    printf("Pinging %s with %d bytes of data:\n", ping_flags.a, BUFFER_SIZE);

    while (ping_flags.c == 0 || sent_packets < ping_flags.c) {
        memset(packet_buffer, 0, BUFFER_SIZE);
        if (ping_flags.t == 4) {
            struct icmphdr icmp_hdr;
            icmp_hdr.type = ICMP_ECHO;
            icmp_hdr.code = 0;
            icmp_hdr.un.echo.id = getpid();
            icmp_hdr.un.echo.sequence = htons(sequence_number++);
            icmp_hdr.checksum = 0;
            memcpy(packet_buffer, &icmp_hdr, sizeof(icmp_hdr));
            icmp_hdr.checksum = calculate_checksum(packet_buffer, sizeof(icmp_hdr));
            memcpy(packet_buffer, &icmp_hdr, sizeof(icmp_hdr));
        } else {
            struct icmp6_hdr icmp6_hdr;
            icmp6_hdr.icmp6_type = ICMP6_ECHO_REQUEST;
            icmp6_hdr.icmp6_code = 0;
            icmp6_hdr.icmp6_id = getpid();
            icmp6_hdr.icmp6_seq = htons(sequence_number++);
            memcpy(packet_buffer, &icmp6_hdr, sizeof(icmp6_hdr));
        }

        gettimeofday(&start, NULL);
        if (ping_flags.t == 4) {
            sendto(socket_fd, packet_buffer, sizeof(struct icmphdr), 0, (struct sockaddr *)&destaddr_4, sizeof(destaddr_4));
        } else {
            sendto(socket_fd, packet_buffer, sizeof(struct icmp6_hdr), 0, (struct sockaddr *)&destaddr_6, sizeof(destaddr_6));
        }
        sent_packets++;

        struct pollfd fds[1];
        fds[0].fd = socket_fd;
        fds[0].events = POLLIN;
        int ret = poll(fds, 1, TIMEOUT);

        if (ret == 0) {
            printf("Request timeout for icmp_seq %d\n", sequence_number);
            continue;
        } else if (ret < 0) {
            perror("poll");
            continue;
        }

        struct sockaddr_in source_addr;
        socklen_t addr_len = sizeof(source_addr);

        int bytes_received = recvfrom(socket_fd, packet_buffer, sizeof(packet_buffer), 0, 
                                      (struct sockaddr *)&source_addr, &addr_len);
        if (bytes_received > 0) {
            received_packets++;
        } else {
            perror("recvfrom");
            continue;
        }

        gettimeofday(&end, NULL);
        int time_to_live = 0;
        if (ping_flags.t == 4) {
            struct iphdr *ip_hdr = (struct iphdr *)packet_buffer;
            time_to_live = ip_hdr->ttl;
        } else {
            struct ip6_hdr *ip6_hdr = (struct ip6_hdr *)packet_buffer;
            time_to_live = ip6_hdr->ip6_hops; 
        }

        double rtt = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;
        total_rtt += rtt;
        if (rtt < min_rtt) min_rtt = rtt;
        if (rtt > max_rtt) max_rtt = rtt;

        printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.2f ms\n", BUFFER_SIZE, ping_flags.a, sequence_number, time_to_live, rtt);

        if (!ping_flags.f) {
            sleep(1);
        }
    }

    printf("--- %s ping statistics ---\n", ping_flags.a);
    printf("%d packets transmitted, %d received, time %.2fms\n", ping_flags.c, received_packets, (total_rtt + ((ping_flags.c - received_packets) * TIMEOUT)) / 1000);
    if (received_packets > 0) {
        double mdev = max_rtt - min_rtt;
        printf("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n", min_rtt, total_rtt / received_packets, max_rtt, mdev);
    } else {
        printf("No valid responses received.\n");
    }
    close(socket_fd);
}

int main(int argc, char *argv[]) {
    if (geteuid() != 0) {
        fprintf(stderr, "Error: You need to run the program with sudo.\n");
        return EXIT_FAILURE;
    }

    if (argc < 5) {
        fprintf(stderr, "Usage: %s <destination_ip>\n", argv[0]);
        return 1;
    }

    signal(SIGINT, handle_interrupt_signal);
    
    PingFlags ping_flags = parse_flags(argc, argv);
    ping(ping_flags);
}
