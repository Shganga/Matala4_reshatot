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

int transmitted_packets = 0; // מספר ההודעות שנשלחו
int recv_pings = 0;    // מספר ההודעות שהתקבלו
double totalRtt = 0;        // סך זמני RTT
double minRtt = 1e9;        // זמן ה-RTT המינימלי
double maxRtt = 0;         

// Signal handler ל-SIGINT
void handle_sigint(int sig) {
    printf("--- ping statistics ---\n");
    printf("%d packets transmitted, %d received, %.2f%% packet loss\n", 
           transmitted_packets, 
           recv_pings, 
           (transmitted_packets - recv_pings) * 100.0 / transmitted_packets);

    if (recv_pings > 0) {
        double avg_rtt = totalRtt / recv_pings;
        double mdev = maxRtt - minRtt;
        printf("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n", minRtt, avg_rtt, maxRtt, mdev);
    } else {
        printf("No valid responses received.\n");
    }
    exit(0);
}


PingFlags manFlags(int argc,char *argv[]){
	PingFlags optFlag ={NULL,0,0,0};
	int opt;
	while ((opt=getopt(argc,argv,"a:t:c:f"))!= -1){
		if (opt=='a'){
			optFlag.a=optarg;	
		}
		else if (opt=='t'){
			optFlag.t=atoi(optarg);
		}
		else if (opt=='c')
		{
			optFlag.c=atoi(optarg);
		}
		else if(opt=='f'){
			optFlag.f=atoi(optarg);
		}
		else{
			fprintf(stderr,"Invalid flag option.\n");
			exit(EXIT_FAILURE);
		}

	}
	if (optFlag.a==NULL){
		fprintf(stderr,"Error:the address is required!.\n");
		exit(EXIT_FAILURE);
	}
	if (optFlag.t!=4 && optFlag.t!=6)
	{
		fprintf(stderr,"Error:the type is invalid!.\n");
		exit(EXIT_FAILURE);
	}
	return optFlag;

}
unsigned short int calculate_checksum(void *data, unsigned int bytes) {
	unsigned short int *data_pointer = (unsigned short int *)data;
	unsigned int total_sum = 0;

	// Main summing loop.
	while (bytes > 1)
	{
		total_sum += *data_pointer++; // Some magic pointer arithmetic.
		bytes -= 2;
	}

	// Add left-over byte, if any.
	if (bytes > 0)
		total_sum += *((unsigned char *)data_pointer);

	// Fold 32-bit sum to 16 bits.
	while (total_sum >> 16)
		total_sum = (total_sum & 0xFFFF) + (total_sum >> 16);

	// Return the one's complement of the result.
	return (~((unsigned short int)total_sum));
}

void typePing(PingFlags opt){
	int sock;
	struct sockaddr_in destaddr_4;
	struct sockaddr_in6 destaddr_6;
	char buffer[BUFFER_SIZE];
	struct timeval start,end;
	int seq_num=0;
	if(opt.t==4){
		memset(&destaddr_4, 0, sizeof(destaddr_4));	
		destaddr_4.sin_family = AF_INET;
		if (inet_pton(AF_INET, opt.a, &destaddr_4.sin_addr) <= 0){
		fprintf(stderr, "Error: \"%s\" is not a valid IPv4 address\n", opt.a);
		return;
		}
		sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP);
		if (sock < 0){
			perror("socket");
			exit(EXIT_FAILURE);
		}
		
	}
	else if(opt.t==6){
		memset(&destaddr_6, 0, sizeof(destaddr_6));	
		destaddr_6.sin6_family = AF_INET6;
		if (inet_pton(AF_INET6, opt.a, &destaddr_6.sin6_addr) <= 0){
		fprintf(stderr, "Error: \"%s\" is not a valid IPv4 address\n", opt.a);
		return;
		}
		sock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
		if (sock < 0){
			perror("socket");
			if (errno == EACCES || errno == EPERM)
			fprintf(stderr, "You need to run the program with sudo.\n");
			exit(EXIT_FAILURE);
		}
	}
	else{
		fprintf(stderr,"Error:the type is not valid!.\n");
		exit(EXIT_FAILURE);
	}
	printf("Pinging %s with %d bytes of data:\n",opt.a,BUFFER_SIZE);



	while(opt.c==0||transmitted_packets<opt.c){
		memset(buffer,0,BUFFER_SIZE);
		if (opt.t== 4) {
            struct icmphdr icmp_hdr;
            icmp_hdr.type = ICMP_ECHO;
            icmp_hdr.code = 0;
            icmp_hdr.un.echo.id = getpid();
            icmp_hdr.un.echo.sequence = htons(seq_num++);
            icmp_hdr.checksum = 0;
            memcpy(buffer, &icmp_hdr, sizeof(icmp_hdr));
            icmp_hdr.checksum = calculate_checksum(buffer, sizeof(icmp_hdr));
            memcpy(buffer, &icmp_hdr, sizeof(icmp_hdr));}
		else {
            struct icmp6_hdr icmp6_hdr;
            icmp6_hdr.icmp6_type = ICMP6_ECHO_REQUEST;
            icmp6_hdr.icmp6_code = 0;
            icmp6_hdr.icmp6_id = getpid();
            icmp6_hdr.icmp6_seq = htons(seq_num++);
            memcpy(buffer, &icmp6_hdr, sizeof(icmp6_hdr));
        }

        gettimeofday(&start, NULL);
        if (opt.t == 4) {
            sendto(sock, buffer, sizeof(struct icmphdr), 0, (struct sockaddr *)&destaddr_4, sizeof(destaddr_4));
        } else {
            sendto(sock, buffer, sizeof(struct icmp6_hdr), 0, (struct sockaddr *)&destaddr_6, sizeof(destaddr_6));
        }
		transmitted_packets++;
	

        struct pollfd fds[1];
        fds[0].fd = sock;
        fds[0].events = POLLIN;
        int ret = poll(fds, 1, TIMEOUT);

        if (ret == 0) {
            printf("Request timeout for icmp_seq %d\n", seq_num);
            continue;}
		else if (ret < 0) {
            perror("poll");
            continue;
        }

	struct sockaddr_in source_addr;
    socklen_t addr_len = sizeof(source_addr);

    int bytes_received = recvfrom(sock, buffer, sizeof(buffer), 0, 
                                  (struct sockaddr *)&source_addr, &addr_len);
    if (bytes_received> 0) {
		recv_pings++;
	}
	else{
        perror("recvfrom");
        continue;
	}


        gettimeofday(&end, NULL);
		int ttl=0;
		if (opt.t==4){
			struct iphdr *ip_hdr=(struct iphdr *)buffer;
			ttl = ip_hdr->ttl;
		}
		else{
			struct ip6_hdr *ip6_hdr = (struct ip6_hdr *)buffer;
    		ttl = ip6_hdr->ip6_hops; 
		}
        double rtt = (end.tv_sec - start.tv_sec) * 1000.0 + (end.tv_usec - start.tv_usec) / 1000.0;
        totalRtt += rtt;
        if (rtt < minRtt) minRtt = rtt;
        if (rtt > maxRtt) maxRtt = rtt;
		
        printf("%d bytes from %s: icmp_seq=%d ttl=%d time=%.2f ms\n", BUFFER_SIZE, opt.a, seq_num,ttl, rtt);

        if (!opt.f) {
            sleep(1);
        }
    }
    printf("--- %s ping statistics ---\n", opt.a);
	printf("%d packets transmitted, %d received, time %.2fms\n", opt.c, recv_pings, (totalRtt + ((opt.c - recv_pings) * TIMEOUT)) / 1000);
	if (recv_pings > 0) {
    double mdev = maxRtt - minRtt;
    printf("rtt min/avg/max/mdev = %.3f/%.3f/%.3f/%.3f ms\n", minRtt, totalRtt / recv_pings, maxRtt, mdev);}
	else {
    printf("No valid responses received.\n");
	}
	 close(sock);
}


/*
 * @brief Main function of the program.
 * @param argc Number of command-line arguments.
 * @param argv Array of command-line arguments.
 * @return 0 on success, 1 on failure.
 * @note The program requires one command-line argument: the destination IP address.
*/
int main(int argc, char *argv[]) {
	if (geteuid()!=0)
	{
		fprintf(stderr,"Error:You need to run the program with sudo.\n");
		return EXIT_FAILURE;
	}
	if (argc< 5)
	{
		fprintf(stderr, "Usage: %s <destination_ip>\n", argv[0]);
		return 1;
	}
	signal(SIGINT,handle_sigint);
	
	PingFlags optFlag=manFlags(argc,argv);
	typePing(optFlag);
	
}