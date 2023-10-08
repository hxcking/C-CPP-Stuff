#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <linux/ip.h>
#include <linux/tcp.h>
#include <netdb.h>
#include <sys/ioctl.h>

#define DEVICE "wlx00e0480d8a60"

// Calculating the checksum
unsigned short in_cksum(unsigned short *addr, int len){
    unsigned short result;
    unsigned int sum = 0;

    // Adding all 2-byte words
    while(len > 1){
        sum += *addr++;
        len -= 2;
    }

    // If there is a byte left over, adding it to the sum
    if(len == 1)
        sum += *(unsigned char*) addr;

    sum = (sum >> 16) + (sum & 0xFFFF); // Adding the carry
    sum += (sum >> 16); // Adding carry again
    result = ~sum; // Inverting the result
    return result;
}

// Assembling and sending a packet
int send_packet(int sd, unsigned short port, struct sockaddr_in source, struct hostent* hp){
    struct sockaddr_in servaddr;
    struct tcphdr tcp_hdr;

    // Pseudo packet structure
    struct pseudo_hdr{
        unsigned int source_address;
        unsigned int dest_address;
        unsigned char place_holder;
        unsigned char protocol;
        unsigned short length;
        struct tcphdr tcp;
    } pseudo_hdr;

    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(port);
    servaddr.sin_addr = *((struct in_addr *)hp->h_addr);

    // Filling the TCP header
    tcp_hdr.source = getpid();
    tcp_hdr.dest = htons(port);
    tcp_hdr.seq = htons(getpid() + port);
    tcp_hdr.ack_seq = 0;
    tcp_hdr.res1 = 0;
    tcp_hdr.doff = 5;
    tcp_hdr.fin = 0;
    tcp_hdr.syn = 1;
    tcp_hdr.rst = 0;
    tcp_hdr.psh = 0;
    tcp_hdr.ack = 0;
    tcp_hdr.urg = 0;
    tcp_hdr.ece = 0;
    tcp_hdr.cwr = 0;
    tcp_hdr.window = htons(128);
    tcp_hdr.check = 0;
    tcp_hdr.urg_ptr = 0;

    // Filling the pseudo header
    pseudo_hdr.source_address = source.sin_addr.s_addr;
    pseudo_hdr.dest_address = servaddr.sin_addr.s_addr;
    pseudo_hdr.place_holder = 0;
    pseudo_hdr.protocol = IPPROTO_TCP;
    pseudo_hdr.length = htons(sizeof(struct tcphdr));

    // Pasting the filled TCP header after pseudo header
    std::memcpy(&pseudo_hdr.tcp, &tcp_hdr, sizeof(struct tcphdr));

    // Calculating the TCP header checksum
    tcp_hdr.check = in_cksum((unsigned short *)&pseudo_hdr, sizeof(struct pseudo_hdr));

    // Sending the TCP packet
    if(sendto(sd, &tcp_hdr, sizeof(struct tcphdr), 0, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
        std::cout << "sendto() failed";

}

// Receiving the reply packet and checking the flags
int recv_packet(int sd){

    char recvbuf[1500];
    struct tcphdr *tcphdr = (struct tcphdr *)(recvbuf + sizeof(struct iphdr));

    while(1){
        if(recv(sd, recvbuf, sizeof(recvbuf), 0) < 0)
            std::cout << "recv() failed";

        if(tcphdr->dest == getpid()){
            if(tcphdr->syn == 1 && tcphdr->ack == 1)
                return 1;
            else
                return 0;

        }
    }
}


int main(int argc, char *argv[]){
        
    int sd;
    struct ifreq *ifr = new ifreq;
    struct hostent* hp;
    int port, portLow, portHigh;
    unsigned int dest;
    struct sockaddr_in source;
    struct servent* srvport;

    if(argc != 4){
        std::cerr << "Usage: " << argv[0] << " <address> <portlow> <porthigh>\n";
        return EXIT_FAILURE;
    } 

    hp = gethostbyname(argv[1]);
    if(hp == NULL){
        std::cerr << "gethostbyname() failed";
        return EXIT_FAILURE;
    }

    portLow = std::stoi(argv[2]);
    portHigh = std::stoi(argv[3]);

    if((sd = socket(PF_INET, SOCK_RAW, IPPROTO_TCP)) < 0){
        perror("socket() failed");
        return EXIT_FAILURE;
    }

    std::cout << "Scanning..." << std::endl;

    // Obtaining the IP address of the interface and placing it into the source address structure
    std::snprintf(ifr->ifr_name, sizeof(ifr->ifr_name), "%s", DEVICE);
    ioctl(sd, SIOCGIFADDR, ifr);
    std::memcpy((char*)&source, (char*)&(ifr->ifr_addr), sizeof(struct sockaddr));

    for(port = portLow; port <= portHigh; port++){
        send_packet(sd, port, source, hp);
        if(recv_packet(sd) == 1){
            srvport = getservbyport(htons(port), "tcp");
            if(srvport == NULL)
                std::cout << "Open: " << port << " (unknown)", port;
            else
                std::cout << "Open: " << port << " (" << srvport->s_name << ")";

            std::cout << std::endl;
        }
    }
    close(sd);
    return 0;
}
