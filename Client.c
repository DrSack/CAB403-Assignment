#include "Commands.h"
/*---------------- Initialize VARIABLES ---------------- */
int sockfd, numbytes;
int livefeed = 0;
int manualdestroy = 0;
int mainbool= 1;
char buf[1024];
pthread_t thread;
ClientID ID;

/*---------------------- DECLARE FUNCTIONS ---------------- */
void ConnectToServer(char* argv[]);
void AddedToServerCheck();
void close_client();
void close_livefeed();
void close_livefeedALL();
void MainRun();

/*---------------------- MAIN FUNCTION ------------------- */
int main(int argc, char *argv[])
{
	signal(SIGINT, close_client);//Set sigints for client termination
	signal(SIGHUP, close_client);

	if (argc != 3) {
		printf("Usage:%s [hostname] [portnumber]\n", argv[0]);//If parameters are not 3 then error.
		exit(1);
	}

	ConnectToServer(argv);
	AddedToServerCheck();
	MainRun();

	return 0;
}


/*
	This function sets up the required sockets and sockaddr 
	to connect and bind to the server

	Parameters:
	argv: The string value of the parameters from the main function. 

	Returns:
	Nothing
*/

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

/*
	This is the main Client Function which will running continously in a while loop.
	The purpose of this function is to get the input of the user and send that message 
	towards the server, the server will then output results back to the client, in which,
	this function will interpret.

	Parameters:
	Nothing

	Returns:
	Nothing
*/

void MainRun(){
while(mainbool == 1){
		char to_send[sizeof(ClientID)];//Set buffer size to send.
		int chunck, c; ID.mode = OFF;
		printf("Command:");
		fgets(buf, 1024, stdin);
		if(buf[0] == '\n'){//If first element is newline
			strcpy(buf," ");//Set to nothing.
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

		if(strcmp("BYE",ID.Message) == 0){//Break out of the loop if user inputs BYE.
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

		/*---------Displays function ----------- 
		Whenever the server sends a continous stream of messages with a final break.
		This function handles that server functionality.
		*/
		
		else if(numbytes > 0){
			if(strcmp("Display",ID.Message) == 0){
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
			
			/*---------Livefeed function ----------- 
			allows users to have a live read of all subscribed channels.
			*/
			else if(strcmp("LivefeedALL",ID.Message) == 0){
				signal(SIGINT, close_livefeedALL);
				manualdestroy = 0;
				while(livefeed == 0)
				{
					numbytes=recv(sockfd, &ID, sizeof(ClientID), 0);
					if(numbytes > 0){
						if(ID.mode == STOP){
							if(manualdestroy == 1){
								break;//break out of the loop if sigint is called
							}
							else{
								ID.mode = STOP;
								RelayBackMsg(ID," ",sockfd);
							}
						}

						else if(ID.mode != NONE){
							printf("%s\n",ID.Message);
							RelayBackMsg(ID," ",sockfd);
						}

						else if(ID.mode == NONE){
							printf("Not subscribed to any channels.");
							break;
						}
					}
				}
				livefeed = 0;//reset back to default values.
				manualdestroy = 0;
				signal(SIGINT, close_client);
				signal(SIGHUP, close_client);
			}

			else if(ID.mode == OFF){
				printf("%s",ID.Message);//print any valid messages
			}	
				
			else if(ID.mode != OFF){// If the message is invalid then loop.
				continue;
			}

			printf("\n\n");//Add space and reset buffer.
			fflush(stdin);
			memset(buf,0,sizeof(buf));
			break;
		}

		else{
			printf("Server has disconnected\n");// If the loop is broken shutdown
			close(sockfd);
			shutdown(sockfd,SHUT_RDWR);
			mainbool = 0;
			break;
		}

		}	
	}

close(sockfd);
shutdown(sockfd,SHUT_RDWR);//Shutdown if loop is broken
exit(1);
}

/*----- Closes the client using SIGINT ----- */
void close_client()
{
	printf("\nClient Closing...\n");
	RelayBackMsg(ID,"BYE",sockfd);
	close(sockfd);
	shutdown(sockfd,SHUT_RDWR);
	exit(1);
}

/*------- Closes the livefeed function using SIGINT ----- */
void close_livefeedALL()
{
	printf("\nClient Livefeed closed...\n");
	ID.mode = BREAK;
	RelayBackMsg(ID," ",sockfd);
	manualdestroy = 1;
	livefeed = 1;
	numbytes = 100;
}


/*----- Check if the server is full ----- */
void AddedToServerCheck()
{
	if ((numbytes=recv(sockfd, &ID, sizeof(ClientID), 0)) == -1) {
		perror("recv");
		exit(1);
	}
	if(ID.ID == 0){//If returned with 0 shutdown
		ID.mode=SHUTDOWN;
		printf("Server Full....\n");
		RelayBackMsg(ID,"",sockfd);
		shutdown(sockfd,SHUT_RDWR);
		close(sockfd);
		exit(1);
	}
	else{// Proceed to Mainrun
		printf("%s %d\n",ID.Message, ID.ID);
		strcpy(ID.Message,"");
	}
	
}