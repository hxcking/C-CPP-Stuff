#include <iostream>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <cstring>
#include <unistd.h>

int main(int argc, char *argv[]){

    int sd;
    struct hostent* hp;
    struct sockaddr_in servaddr;
    struct servent *srvport;
    int port, portlow, porthigh;

    if(argc != 4){
        std::cerr << "Usage: " << argv[0] << " <address> <portlow> <porthigh> \n";
        return EXIT_FAILURE;
    }

    hp = gethostbyname(argv[1]);
    if(hp == NULL){
        std::cerr << "gethostbyname() failed\n";
        return EXIT_FAILURE;
    }

    portlow = std::stoi(argv[2]);
    porthigh = std::stoi(argv[3]);

    std::cout << "Scanning..." << std::endl;

    for(port = portlow; port <= porthigh; port++){
        if((sd = socket(PF_INET, SOCK_STREAM, 0)) < 0){
            perror("socket() failed\n");
            return EXIT_FAILURE;
        }

        memset(&servaddr, 0, sizeof(servaddr));
        servaddr.sin_family = AF_INET;
        servaddr.sin_port = htons(port);
        servaddr.sin_addr = *((struct in_addr *)hp->h_addr);

        if(connect(sd, (struct sockaddr *)&servaddr, sizeof(servaddr)) == 0){
            srvport = getservbyport(htons(port), "tcp");
            if(srvport == NULL)
                std::cout << "Open: " << port << " (unknown)";
            else
                std::cout << "Open: " << port << "(" << srvport->s_name << ")";
            std::cout << std::endl;    
        }
        close(sd);
    }
    std::cout << std::endl;

    return 0;
}