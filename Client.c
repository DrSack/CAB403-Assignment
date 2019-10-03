#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 
#include <netdb.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <sys/shm.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>

#include "Commands.h"


int sockfd, numbytes;
int livefeed = 0;
int livefeed2 = 0;
int manualdestroy = 0;
int mainbool= 1;
char buf[1028];
ClientID ID;

void ConnectToServer(char* argv[]);
void AddedToServerCheck();
void close_client();
void close_livefeed();
void close_livefeedALL();
void MainRun();


int main(int argc, char *argv[])
{

	signal(SIGINT, close_client);
	signal(SIGHUP, close_client);

	if (argc != 3) {
		printf("Usage:%s [hostname] [portnumber]\n", argv[0]);
		exit(1);
	}

	ConnectToServer(argv);
	AddedToServerCheck();
	MainRun();

	return 0;
}

void ConnectToServer(char* argv[])
{
	struct hostent *he;
	struct sockaddr_in their_addr; /* connector's address information */

	if ((he=gethostbyname(argv[1])) == NULL) {  /* get the host info */
		herror("gethostbyname");
		exit(1);
	}

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	their_addr.sin_family = AF_INET;      /* host byte order */
	their_addr.sin_port = htons(atoi(argv[2]));    /* short, network byte order */
	their_addr.sin_addr = *((struct in_addr *)he->h_addr);
	bzero(&(their_addr.sin_zero), 8);     /* zero the rest of the struct */

	if (connect(sockfd, (struct sockaddr *)&their_addr,sizeof(struct sockaddr)) == -1) {
		printf("Can't connect to server Please try again.\n");
		exit(1);
	}
}

void MainRun(){
		while(mainbool == 1){
				char to_send[sizeof(ClientID)];
				ID.mode = OFF;
				printf("Command:");
				fgets(buf, 1028, stdin);

				if (sscanf(buf, "%[^\n]%*c", to_send) == 1 )
					strcpy(ID.Message,to_send);
					ssendClientID(sockfd, ID);

				if(strcmp("BYE",ID.Message) == 0){
					printf("Ending session..\n");
					fflush(stdin);
					fflush(stdout);
					break;
				}

				if ((numbytes=recv(sockfd, &ID, sizeof(ClientID), 0)) == -1) {
            		printf("Ending session..\n");
					fflush(stdin);
					fflush(stdout);
					break;
        		}

				else if(numbytes > 0){
					if(strcmp("NEXT ALL:",ID.Message) == 0){
						while(1)
						{
							numbytes=recv(sockfd, &ID, sizeof(ClientID), 0);
							if(numbytes > 0){
								if(strstr(ID.Message,"PASS")==NULL){
									printf("%s\n",ID.Message);
								}
								else{
									break;
								}	
							}
						}
					}

					else if(strcmp("CHANNELS ALL:",ID.Message) == 0){
						while(1)
						{
							numbytes=recv(sockfd, &ID, sizeof(ClientID), 0);
							if(numbytes > 0){
								if(ID.mode != PASS){
									printf("%s\n",ID.Message);
								}
								else{
									break;
								}	
							}
						}
					}

					else if(strcmp("LivefeedALL",ID.Message) == 0){
						signal(SIGINT, close_livefeedALL);
						while(livefeed2 == 0)
						{
							numbytes=recv(sockfd, &ID, sizeof(ClientID), 0);
							if(numbytes > 0){
							
								if(strcmp(ID.Message,"STOP")==0){
									if(manualdestroy == 1){
										strcpy(ID.Message,"BREAK");
										ssendClientID(sockfd, ID);
										break;
									}
									else{
										if(manualdestroy == 1){
										strcpy(ID.Message,"BREAK");
										ssendClientID(sockfd, ID);
										break;
										}
										strcpy(ID.Message,"STOP");
										ssendClientID(sockfd, ID);
										
									}
								}
								else if(strstr(ID.Message,"NONE")==NULL){
									printf("%s\n",ID.Message);
									strcpy(ID.Message,"READY");
									ssendClientID(sockfd, ID);
									
								}
								else{
									printf("Not Subscribed to any channels");
									break;
								}
							}
						}
						livefeed2 = 0;
						manualdestroy = 0;
						signal(SIGINT, close_client);
						signal(SIGHUP, close_client);
						
						for(int i = 0; i <100; i++) {
							ClientID temp;
							recv(sockfd, &temp, sizeof(ClientID), MSG_DONTWAIT);
    					}
					}

					else if(strcmp("Live Feed:",ID.Message) == 0){
						signal(SIGINT, close_livefeed);
						while(livefeed == 0)
						{
							numbytes=recv(sockfd, &ID, sizeof(ClientID), 0);

							if(numbytes > 0){
								if(strcmp("Pass",ID.Message) == 0){

								}
								else{
									printf("%s\n",ID.Message);
								}
								

								strcpy(ID.Message,"READY");
								ssendClientID(sockfd, ID);
							}
						}
						livefeed = 0;
						numbytes=recv(sockfd, &ID, sizeof(ClientID), 0);
						strcpy(ID.Message,"STOP");
						ssendClientID(sockfd,ID);
						signal(SIGINT, close_client);
						signal(SIGHUP, close_client);
						
					}

					else if(strstr(ID.Message,"STOP")!=NULL){
						//Empty out from buffer.
						while (1)
						{
							numbytes=recv(sockfd, &ID, sizeof(ClientID), 0);
							if(strstr(ID.Message,"STOP")!=NULL){
								continue;
							}
							else
							{
								break;
							}
						}
						printf("From server: %s",ID.Message);
					}

					else if(ID.mode == OFF){
						printf("From server: %s",ID.Message);
					}	

					printf("\n\n");
					memset(buf,0,MAXDATASIZE);
					
				}

				else{
					printf("Server has disconnected\n");
					close(sockfd);
					shutdown(sockfd,SHUT_RDWR);
					break;
				}
				
			}

		close(sockfd);
		shutdown(sockfd,SHUT_RDWR);
		exit(1);
}
		
void close_client()
{
	printf("\nClient Closing...\n");
	close(sockfd);
	shutdown(sockfd,SHUT_RDWR);
	exit(1);
}

void close_livefeedALL()
{
	printf("\nClient Livefeed closed...\n");
	manualdestroy = 1;
	livefeed2 = 1;
	numbytes = 100;
}

void close_livefeed()
{
	printf("\nClient Livefeed closed...\n");
	livefeed = 1;
	numbytes = 100;
}

void AddedToServerCheck()
{
	if ((numbytes=recv(sockfd, &ID, sizeof(ClientID), 0)) == -1) {
		perror("recv");
		exit(1);
	}
	if(ID.ID == 0){
		printf("ID: %d",ID.ID);
		printf("Queue full.. Please Try Again\n");
		shutdown(sockfd,SHUT_RDWR);
		close(sockfd);
		exit(1);
	}
	else{
		printf("%s %d\n",ID.Message, ID.ID);
	}
	
}