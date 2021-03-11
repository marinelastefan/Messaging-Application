#include <iostream>
#include <unordered_map>
#include <cstdlib>
#include <sstream>
#include <cstring>
#include <string>
#include <algorithm>
#include <unistd.h>
#include <cmath>
#include <iomanip>
#include <vector>
#include <unordered_set>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include "helpers.h"
using namespace std;

using namespace std;
void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_address server_port\n", file);
	exit(0);
}
int main(int argc, char *argv[]) {
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[BUFLEN];
	int i;

	fd_set read_fds;
    fd_set tmp_fds;
    int fdmax;
    //golesc multimea de descriptori de citire si multimea temporara
    FD_ZERO(&tmp_fds);
  	FD_ZERO(&read_fds);
	//deschid socketul de tcp
	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");
	//adaug noii file descriptori in multime, socketul si citirea de la tastatura
	FD_SET(sockfd, &read_fds);
	FD_SET(0, &read_fds);
    fdmax = sockfd;

    int flag = 1;
    //dezactivez algoritmul lui Nagle
    int result = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY,
                    (char *) &flag, sizeof(int));

    //completez informatiile legate de port si adresa ip
    serv_addr.sin_family = AF_INET;
    ret = inet_aton(argv[2], &serv_addr.sin_addr);
    DIE(ret == 0, "inet_aton");
	serv_addr.sin_port = htons(atoi(argv[3]));
	//ma conectez la socket
	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");
    //trimit informatiile legate de id
    char id[10];
    memset(id, 0, 10);
    memcpy(id, argv[1], strlen(argv[1]) + 1);
    n = send(sockfd, argv[1], strlen(argv[1]) + 1 , 0);
    DIE(n < 0, "send");

	while (1) {
		tmp_fds = read_fds;
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
    	DIE(ret < 0, "select");
    	for (i = 0; i <= fdmax; i++) {
    		if (FD_ISSET(i, &tmp_fds)) {
    			//s-a primit un mesaj de tastatura
    			if (i == STDIN_FILENO) {
    				memset(buffer, 0, BUFLEN);
    				fgets(buffer, BUFLEN -1, stdin);
    				//daca primeste comanda exit ne deconectam
    				if (strncmp(buffer, "exit", 4) == 0) {
    					FD_CLR(i, &read_fds);
                        FD_CLR(i, &tmp_fds);
                        FD_CLR(sockfd, &read_fds);
                        FD_CLR(sockfd, &tmp_fds);
    					close(sockfd);
    					return 0;
    				}
    				//s-a primit o comanda diferita de exit
    				//pun la finalul \0 in caz ca s-au primit mai putin de 1500  bytes
    				buffer[strlen(buffer) - 1] = 0;
    				string message = "";
    				char *token = strtok(buffer, " ");
    				if (token == NULL) {
    					printf("Please try subscribe or unsubscribe");
    				} else {
    					//retin tipul comenzii subscribe || unsubscribe
    					if(strncmp(token, "subscribe", 9) == 0) {
    						message += token;
    						message += " ";
    						//retin topicul
    						token = strtok(NULL, " ");
    						if (token == NULL) {
    							printf("Topic not found\n");
    						} else {
    							message += token;
    							message += " ";
        						//retin valoarea sf-ului
        						token = strtok(NULL, " ");
        						if (token == NULL) {
        							printf("SF not found\n");
        						} else {
        							message += token;
        						}
                            }
    						n = send(sockfd, message.c_str(), message.size(), 0);
    					} else if (strncmp(token, "unsubscribe", 11) == 0) {
    							message += token;
    							message += " ";
    							//retin topicul
    							token = strtok(NULL, " ");
    							if (token == NULL) {
    							printf("Topic not found\n");
    							} else {
    								message += token;
    								message += " ";
    							}
    							n = send(sockfd, message.c_str(), message.size(), 0);

    					} else {
    						printf("Wrong command\n");
    					}

    				}
    				//s-au primit date de la server
				} else if(i ==  sockfd) {
					memset(buffer, 0, BUFLEN);
					ret = recv(sockfd, buffer, BUFLEN, 0);
					DIE(ret < 0, "recv");

					//serverul a inchis conexiunea
					if (ret == 0) {
						close(sockfd);
						return 0;
					}
                    //ma ocup de despartirea mesajelor
                    char *token = strtok(buffer, "$");
                    while(token != NULL) {
                        printf("%s\n", token);
                        token = strtok(NULL, "$");
                    }
                }
    		}

    	}


	}
close(sockfd);
return 0;
}
