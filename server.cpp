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

#define MAX_CLIENTS 15

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}


int main(int argc, char* argv[]) {
	int tcpsock, udpsock, new_sock;
	int portno;
	/*tip de date pentru a retine adresa unui socket in cazul comunicatiei prin Internet
     are nevoie de un numar de port si adresa IP a calculatorului*/
	struct sockaddr_in tcp_addr, udp_addr, client_addr;
	socklen_t clilen;
	int ret;
	int i, n;
	int flag = 1;

	fd_set read_fds; //multimea de citire folosita de select()
	fd_set tmp_fds; //multime folosita temporar
	int fdmax;  //valoarea maxima fd din multimea read_fds

	char buffer[BUFLEN];

	//o mapare pentru toti clientii "fd-id"
	unordered_map<string, int> allClients;

	//o mapare sa retin daca clientii sunt conectati sau deconectati
	unordered_map<string, bool> onlineClients;

	//o mapare pentru un topic si clientii abonati la acel topic
	unordered_map<string, unordered_set<string>> topics;

	//o mapare pentru id client si topicele la care are sf-ul 1
	unordered_map<string, vector<string>> sfs;

	//o mapare pentru id client si mesajele pe care trebuie sa i le trimit
	unordered_map<string, vector<string>> messageToSendSf;

	if (argc < 2) {
		usage(argv[0]);
	}

	//se goleste multimea de descriproti de citire 
	//se goleste multimea temporara
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	//deschid un socket pentru udp
	udpsock = socket(PF_INET, SOCK_DGRAM, 0);
	DIE(udpsock < 0, "updsocket");

	//deshid un socket pentru tcp
	tcpsock = socket(AF_INET, SOCK_STREAM, 0);
	DIE(tcpsock < 0, "tcpsocket");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *) &tcp_addr, 0, sizeof(tcp_addr));
	//completez informatiile pentru socketul de tcp
	tcp_addr.sin_family = AF_INET;
	tcp_addr.sin_port = htons(portno);
	tcp_addr.sin_addr.s_addr = INADDR_ANY;

	//completez informatiile pentru socketul de udp
	memset((char *) &udp_addr, 0, sizeof(udp_addr));
	udp_addr.sin_family = AF_INET;
	udp_addr.sin_port = htons(portno);
	udp_addr.sin_addr.s_addr = INADDR_ANY;

	//asociez socketilor de udp si tcp port pe masina locala
	ret = bind(tcpsock, (struct sockaddr *) &tcp_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = bind(udpsock, (struct sockaddr *) &udp_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = listen(tcpsock, MAX_CLIENTS);

	//se adauga noii file descriptori pt socketii de upd si tcp in multimea read_fds
	//si pentru read de la tastatura
	FD_SET(tcpsock, &read_fds);
	FD_SET(udpsock, &read_fds);
	FD_SET(0, &read_fds);

	//fdmax va fi maximul dintre tcp si udp
	fdmax = (tcpsock > udpsock) ? tcpsock : udpsock;

	while(1) {
		tmp_fds = read_fds;
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		for (i = 0; i < fdmax + 1; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
			    //daca s-a primit un mesaj de la tastatura
				if (i == STDIN_FILENO) {
					//se accepta doar mesajul "exit"
					cin>>buffer;
					if (strncmp(buffer, "exit", 4) == 0) {
						FD_CLR(i, &read_fds);
						FD_CLR(i, &tmp_fds);
						close(tcpsock);
						close(udpsock);
						return 0;
					} else {
						printf("Wrong command, please try exit");
					}
				} else if (i == udpsock){
					//s-a primit o conexiune de pe un socket de udp
					memset(buffer, 0, BUFLEN);
					socklen_t udp_socklen = sizeof(sockaddr);
					int n = recvfrom(udpsock, buffer, BUFLEN, 0, 
						(struct sockaddr *) &udp_addr, &udp_socklen);
					DIE(n < 0, "recvfrom");
					//convertesc mesajul de la udp in formatul ce trebuie trimis la client
					char topic[50];
					int val_tip_date;
					memset(topic, 0, 50);
					strncpy(topic, buffer, 50);
					val_tip_date = buffer[50];

					stringstream messageToSendTcp;
					uint16_t port = ntohs(udp_addr.sin_port);
					char ip[10];
					strcpy(ip, inet_ntoa(udp_addr.sin_addr));
					messageToSendTcp << ip << ":" << port << "-" << topic;
					//atunci cand formatez mesajul adaug un & care ma va ajuta la despartirea
					//mesajelor
					switch(val_tip_date) {
							//int
							case 0: {
								int sign = buffer[51];
								//bitul de semn poate fi doar 0 sau 1
								if (sign > 1) {
									printf("Incorrect\n");
								} else {
									int payload;
									memcpy(&payload, buffer + 52, sizeof(int));
									int payloadToTcp = ntohl(payload);
									//daca e numar negativ
									if (sign == 1) {
										payloadToTcp *= -1;

									}
									messageToSendTcp << " - INT - "<< payloadToTcp << "$";

								}
								break;
							}
							//short-real
							case 1: {
								uint16_t payload  = 
								((int8_t)buffer[51] << 8) + (int8_t)buffer[52];
								float payloadToTcp = (float) payload / 100;
								messageToSendTcp << " - SHORT-REAL - " <<payloadToTcp << "$";
								break;
							}
							//float
							case 2: {
								float payload;
								int sign = buffer[51];
								uint8_t power = buffer[56];
								if (sign > 1){
									printf("Incorrect\n");
								} else {
									
									int aux1 = (uint8_t) buffer[52] << 24;
									int aux2 = (uint8_t) buffer[53] << 16;
									int aux3 = (uint8_t) buffer[54] << 8;
									int aux4 = (uint8_t) buffer[55];
									payload = aux1 + aux2 + aux3 + aux4;

									if (sign == 1) {
										payload *= -1;
									}
									float payloadToTcp = payload / ((int32_t)pow(10, power));
									messageToSendTcp<< " - FLOAT - " << payloadToTcp << "$";
								}
								break;
							}
							//string
							default: {
								string payload;
								payload.assign(buffer + 51);
								messageToSendTcp << " - STRING - " << payload << "$";
								break;
							}
						}
						//il adaug pentu despartirea mesajelor
						//cout << messageToSendTcp.str() << "\n";
						//verific daca sunt clienti abonati la topicul respectiv
						unordered_set<string> sb = topics[topic];
						if (sb.size() != 0) {
							for (auto subscriber : sb) {
								//daca clientul este conectat ii trimit mesajul
								if (onlineClients.find(subscriber)->second == true) {
									int fd = allClients.at(subscriber);
									n = send(fd, messageToSendTcp.str().data(), 
										messageToSendTcp.str().size(), 0);
									DIE(n < 0, "send");
								} else {
									//verific daca are sf-ul activat pt acel topic 
									//pentru a ii retine mesajele
									if (find(sfs[subscriber].begin(), sfs[subscriber].end(), topic) 
										!= sfs[subscriber].end()) {
										messageToSendSf[subscriber].push_back(messageToSendTcp.str());

									}
								}
							}
						}

				//s-a primit o conexiune de un un socket tcp
				} else if (i == tcpsock) {
					clilen = sizeof(sockaddr);
					new_sock = accept(tcpsock, (struct sockaddr *) &client_addr, &clilen);
					DIE(new_sock < 0, "accept");
					//se dezactiveaza algoritmul lui Nagle pentru conexiunea la clientul TCP
                    setsockopt(new_sock, IPPROTO_TCP, TCP_NODELAY, (char *)&flag, sizeof(int));
					//se adauga noul socket intors de accept in multimea descriptorilor de citire
					FD_SET(new_sock, &read_fds);
					//se actualizeazz fdmax
					if (new_sock > fdmax) {
						fdmax = new_sock;
					}
					memset(buffer, 0, BUFLEN);
					n = recv(new_sock, buffer, BUFLEN - 1, 0);
					DIE(n < 0, "recv");
					char client_id[10];
					memset(client_id, 0, 10);
					strcpy(client_id, buffer);
					printf("New client %s connected from %s:%hu\n", client_id,
						inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
					//trebuie sa adaugam noul client in lista cu clientii activi 
					//si in lista cu clientii online
					string string_id = "";
					int size = sizeof(client_id) / sizeof(char);
					for (i = 0; i < size; i++) {
						string_id += client_id[i];
					}
					if (allClients.find(string_id) == allClients.end()) {
						//adaug clientul in lista de clienti daca nue exista deja
						allClients.insert({string_id, new_sock});
						//il marchez ca fiind online
						onlineClients.insert({string_id, true});
					} else {
						//il marchez ca fiind conectat
						for(auto onlineClient : onlineClients) {
							if (onlineClient.first == string_id) {
								onlineClients[string_id] = true;
							}
						}

					}
					//verific daca are mesaje de primit
					if (messageToSendSf[string_id].size() != 0) {
						for (auto m : messageToSendSf[string_id]) {
							memset(buffer, 0, BUFLEN);
							memcpy(buffer, m.c_str(), m.size() + 1);
							n = send(new_sock, buffer, BUFLEN, 0);
							DIE(n < 0, "send");
						}

						messageToSendSf[string_id].clear();
						sfs.erase(string_id);
					}
					//s-au primit comenzi de subscribe sau unsubscribe
				} else {
					memset(buffer, BUFLEN, 0);
					n = recv(i, buffer, sizeof(buffer), 0);
					//daca un client s-a deconectat
					if (n == 0) {
						string id;
						for( auto client : allClients) {
							if (client.second == i) {
									id = client.first;
									break;
							}
						}
						//il marchez ca fiind deconectat
						for(auto onlineClient : onlineClients) {
							if (onlineClient.first == id) {
								onlineClients[id] = false;
							}
						}

						cout << "Client " << id << " disconnected\n";
						close(i);
						//scot in multimea de citire socketul inchis
						FD_CLR(i, &read_fds);
					} else {

						//clientul este conectat
						//nu verific daca id-ul exista deja, deoarece este 
						//specificat ca nu vor exista 2 clienti cu acelsi 
						//id conectati la acelasi moment
						//clientul se aboneaza/dezaboneaza
						string message(buffer);
						stringstream command(message);
						string subscrbcmd, topic;
						int sf;
						command >> subscrbcmd >> topic;
						string unsubscribe("unsubscribe");
						string subscribe("subscribe");
						string id;
						for( auto client : allClients) {
							if (client.second == i) {
								id = client.first;
								break;
							}
						}

						//il marchez ca fiind deconectat
						for(auto onlineClient : onlineClients) {
							if (onlineClient.first == id) {
								onlineClients[id] = true;
							}
						}

						if (subscrbcmd == unsubscribe) {
							//verific daca topicul exista
						if (topics.find(topic) != topics.end()) {
								//sterg clientul din lista de abonati ai topicului
								auto it = find(topics[topic].begin(), 
									topics[topic].end(), id);
								topics[topic].erase(it);
								cout << "Client " << id << " unsubscribed " << topic << "\n";
							}
						} else if (subscrbcmd == subscribe) {
							//aflu sf-ul clientului
							command >> sf;
							// il adaug in lista de topicuri
							topics[topic].insert(id);
							//daca are sf ul 1 il salvez in lista de topicuri
							if (sf == 1) {
								sfs[id].push_back(topic);
							}
							cout << "Client " << id << " subscribed " << topic << "\n";

						}
						
					} 

				}

				}
			}
	}
	close(tcpsock);
	close(udpsock);
	return 0;

}