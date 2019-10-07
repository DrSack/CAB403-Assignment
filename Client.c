#include "Commands.h"

int sockfd, numbytes;
int livefeed = 0;
int livefeed2 = 0;
int manualdestroy = 0;
int mainbool= 1;
char buf[1024];
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
	struct timeval tv;// Set time out to socket.
	tv.tv_sec = 1;
	tv.tv_usec = 0;
	setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);

		while(mainbool == 1){
				char to_send[sizeof(ClientID)];
				int chunck, c; ID.mode = OFF;
				printf("Command:");
				fgets(buf, 1024, stdin);
				if(buf[0] == '\n'){
					strcpy(buf," ");
				}
				if (chunck = sscanf(buf, "%[^\n]%*c", to_send) >= 0 ){// check if empty string
					if(chunck == 1){// send package
						strcpy(ID.Message,to_send);
						ssendClientID(sockfd, ID);
					}
					else{
						strcpy(ID.Message,"");//replace ID.message with nothing.
						ssendClientID(sockfd, ID);
					}
				}	

				if(strcmp("BYE",ID.Message) == 0){
					printf("Ending session..\n");
					fflush(stdin);
					fflush(stdout);
					break;
				}

				while(1){
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
						manualdestroy = 0;
						while(livefeed2 == 0)
						{
							numbytes=recv(sockfd, &ID, sizeof(ClientID), 0);
							if(numbytes > 0){
								if(strcmp(ID.Message,"STOP")==0){
									if(manualdestroy == 1){
										break;
									}
									else{
										RelayBackMsg(ID,"STOP",sockfd);
									}
								}

								else if(strstr(ID.Message,"NONE")==NULL){
									printf("%s\n",ID.Message);
									strcpy(ID.Message,"READY");
									ssendClientID(sockfd, ID);
								}

								else{
									printf("Not subscribed to any channels.");
									break;
								}
							}
						}
						livefeed2 = 0;
						manualdestroy = 0;
						signal(SIGINT, close_client);
						signal(SIGHUP, close_client);
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

					else if(ID.mode == OFF){
						printf("From server: %s",ID.Message);
					}	
						
					else if(ID.mode != OFF){//If it aint off then loop
						continue;
					}

					printf("\n\n");
					fflush(stdin);
					memset(buf,0,sizeof(buf));
					break;
				}

				else{
					printf("Server has disconnected\n");
					close(sockfd);
					shutdown(sockfd,SHUT_RDWR);
					mainbool = 0;
					break;
				}

				}	
			}

		close(sockfd);
		shutdown(sockfd,SHUT_RDWR);
		exit(1);
}
		
void close_client()
{
	printf("\nClient Closing...\n");
	RelayBackMsg(ID,"BYE",sockfd);
	close(sockfd);
	shutdown(sockfd,SHUT_RDWR);
	exit(1);
}

void close_livefeedALL()
{
	printf("\nClient Livefeed closed...\n");
	RelayBackMsg(ID,"BREAK",sockfd);
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
		ID.mode=SHUTDOWN;
		printf("Server Full....\n");
		RelayBackMsg(ID,"",sockfd);
		shutdown(sockfd,SHUT_RDWR);
		close(sockfd);
		exit(1);
	}
	else{
		printf("%s %d\n",ID.Message, ID.ID);
		strcpy(ID.Message,"");
	}
	
}