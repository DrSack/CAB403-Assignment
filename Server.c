#include <arpa/inet.h>
#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <sys/wait.h> 
#include <unistd.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/mman.h>

#include "Commands.h"
#define BACKLOG 10 
#define MAXUSER 5
#define CNULL 256


Channel *Channels, *ChannelTail, *ChannelHead;
ChannelClient *CCNULL;
ChannelList *Clist;
ClientID totalusers[5];

int numbytes;
int totalChan[255];


int sockfd;
int ID_num = 1;

void DisplayChannels();
void ConnectAndAssign();
void RelayBackMsg();
void SubChannel();
void UnsubChannel();
void NEXT();
void LIVEFEED();
void RunClient();
void UNSUB_ALL();
void InvalidChannel();
void SEND();


void close_server()
{
	printf("\nServer Closing...\n");
	close(sockfd);
	shutdown(sockfd,SHUT_RDWR);
	exit(1);
}

void handler()
{
	 pid_t chpid = wait(NULL);

	for(int i = 0; i < 5; i++){
		if(totalusers[i].PID==chpid){
			totalusers[i].PID = 0;
			totalusers[i].ID = 0;
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	int new_fd;  /* listen on sock_fd, new connection on new_fd */
	struct sockaddr_in my_addr;    /* my address information */
	socklen_t sin_size;

  	int shmid = shmget(IPC_PRIVATE, sizeof(ChannelList), IPC_CREAT | 0666);// setup shared memory
	Clist = shmat(shmid, 0, 0);

	for(int i = 0; i < 255; i++){// Initialize all structs
		Clist->next[i].ID = 256;
		for(int x = 0; x < MAXUSER; x++){
			Clist->next[i].ClientChan[x].Client.ID = 0;
		}
		Clist->next[i].Msg[0].truth = 0;
	}

	for(int i = 0; i < 5; i++){// Default value of ID
		totalusers[i].ID=0;
	}
	

	if (argc != 2) {
		printf("usage:%s [portnumber]\n",argv[0]);
		exit(1);
	}

	
	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		perror("socket");
		exit(1);
	}

	my_addr.sin_family = AF_INET;         
	my_addr.sin_port = htons(atoi(argv[1]));     
	my_addr.sin_addr.s_addr = INADDR_ANY; 

	/* bind the socket to the end point */
	if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr)) == -1) {
		perror("bind");
		exit(1);
	}

	/* start listnening */
	if (listen(sockfd, BACKLOG) == -1) {
		perror("listen");
		exit(1);
	}
	ConnectAndAssign(sin_size,new_fd,sockfd);
	return EXIT_SUCCESS;
}

void ConnectAndAssign(socklen_t sin_size, int new_fd, int sockfd)
{
	printf("server starts listnening ...\n");
	while(1) {  
		fflush(stdin);
		fflush(stdout);	
		struct sockaddr_in their_addr; /* connector's address information */
		sin_size = sizeof(struct sockaddr_in);
		if ((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size)) == -1) {
			perror("accept");
			continue;
		}
		else{
			ClientID fail; pid_t natural; int truth = 0;
					for(int i = 0; i < 5; i++){
						if(totalusers[i].ID==0){
							truth = 1;
						break;
						}
					}
				
				//Client Function..
				if(truth == 1){
					natural = fork();
					if(natural == 0){// child process runs the backend
						RunClient(new_fd);
					}
					else{//Main process adds the user to the queue.
						signal(SIGCHLD, handler);
						signal(SIGINT, close_server);

						for(int i = 0; i < 5; i++){
						if(totalusers[i].ID==0){
							totalusers[i].ID=i+1;
							totalusers[i].PID = natural;
							strcpy(totalusers[i].Message, "Welcome! Your client ID is");
							printf("Client: %d has connected\n",i+1);
							if (send(new_fd, &totalusers[i], sizeof(ClientID), 0) == -1)
								perror("send");
							break;
							}
						}
					}
				}
				else{// If no free spot is avilable then send bad news.
					fail.ID = 0;
					fail.mode = SHUTDOWN;
					if(send(new_fd, &fail, sizeof(ClientID), 0) == -1)
						perror("send");
				}
		}
	}
}

void DisplayChannels(ClientID temp, int socket){// Sort and List out the current Subscribed Channels
	RelayBackMsg(temp,"CHANNELS ALL:",socket);
	bubbleSort(Clist);
	for(int i = 0; i < Clist->tail; i++){
				for(int x = 0; x < MAXUSER; x++){
					if(Clist->next[i].ClientChan[x].Client.ID == temp.ID){
					char num[128];
					char nice[256];
					strcpy(nice, "Subscribed to ");
		    		sprintf(num,"%d  \tTotal Messages:%d \nRead Messages:%d \tUnread Messages:%d \n\n", Clist->next[i].ID, Clist->next[i].TotalMsg, Clist->next[i].ClientChan[x].Read, Clist->next[i].ClientChan[x].NonRead);
					strcat(nice,num);
					RelayBackMsg(temp,nice,socket);
					}
				}
		}
		temp.mode = PASS;
		RelayBackMsg(temp,"",socket);
}

void UNSUB_ALL(ClientID temp){// Sort and List out the current Subscribed Channels
		for(int i = 0; i < Clist->tail; i++){
				for(int x = 0; x < MAXUSER; x++){
					if(Clist->next[i].ClientChan[x].Client.ID == temp.ID){
					Clist->next[i].ClientChan[x].Client.ID = 0;
				}
			}
		}
}

void RunClient(int new_fd){
	int destroy = 0;
	while(1){
		fflush(stdin);
		fflush(stdout);	
		ClientID temp; temp.mode = OFF;
    	while (1) {
			printf("Waiting for msg...\n\n");

	    	if ((numbytes=recv(new_fd, &temp, sizeof(ClientID), 0)) == -1) {
				
        	}

			if (numbytes > 0){
				if(strcmp(temp.Message,"NULL")!=0){
					printf("From Client %d: %s\n",temp.ID,temp.Message);
				}
				

				if(strstr(temp.Message,"UNSUB")!=NULL){
					UnsubChannel(temp,new_fd);
					break;
				}

				else if(strcmp(temp.Message,"NULL")==0){
					RelayBackMsg(temp,"",new_fd);
					break;
				}

				else if(strstr(temp.Message,"SUB")!=NULL){
					SubChannel(temp, new_fd);
					break;
				}

				else if(strstr(temp.Message,"CHANNELS")!=NULL){
					DisplayChannels(temp,new_fd);
					break;
				}

				else if(strstr(temp.Message,"NEXT")!=NULL){
					NEXT(temp,new_fd);
					break;
				}

				else if(strstr(temp.Message,"LIVEFEED")!=NULL){
					LIVEFEED(temp, new_fd);
					break;
				}
				else if(strstr(temp.Message,"SEND")!=NULL){
					SEND(temp,new_fd);
					break;
				}
				else if(strcmp(temp.Message,"BYE")==0){
					UNSUB_ALL(temp);
					destroy = 1;
					break;
				}
				else{
					RelayBackMsg(temp,"Unknown Command", new_fd);
					memset(&temp,0,sizeof(ClientID));
				}			
				break;
			}  		

			else{
				UNSUB_ALL(temp);
				destroy = 1;
				close(new_fd);
				break;
			}
		}
		if(destroy == 1){
			printf("Client has disconnected\n");
			UNSUB_ALL(temp);
			close(new_fd);
			break;
		}
	}
	shmdt(Channels);
	exit(0);
}

void UnsubChannel(ClientID ID, int socket){
	char str[80];
	char *ptr;

	if(CheckPara(ID,"UNSUB",socket) == -1)
		return;
	if(CheckNumber(socket,ID,"UNSUB") == "\0")
		return;
	ptr = malloc(sizeof(CheckNumber(socket,ID,"SUB")));
	strcpy(ptr,CheckNumber(socket,ID,"UNSUB"));
	int channel = atoi(ptr);

	if(checkString(ptr)){
		if(channel < 0 || channel > 255){
			InvalidChannel(ptr,str,ID,channel,socket);
			return;
		}
			if(Clist->next[0].ID == 256){
			ConfirmedChannel(ptr,str,ID,channel,socket,"Not subscribed to channel: ");
			return;
			}
				for(int i = 0; i < Clist->tail; i++)
				{	
					if(Clist->next[i].ID == channel){
								for(int x = 0; x < MAXUSER; x++){
								if(Clist->next[i].ClientChan[x].Client.ID == ID.ID){
								Clist->next[i].ClientChan[x].Client.ID = 0;
								ConfirmedChannel(ptr,str,ID,channel,socket,"Unsubscribed from channel: ");
								return;
								}
							}
					}
				}
				ConfirmedChannel(ptr,str,ID,channel,socket,"Not subscribed to channel: ");
		}
else{
	InvalidChannel(ptr,str,ID,channel,socket);
}
}

void SubChannel(ClientID ID, int socket)
{
	char str[80];
	char *ptr;

	if(CheckPara(ID,"SUB",socket) == -1)// Check if there is no parameter
		return;
	if(CheckNumber(socket,ID,"SUB") == "\0")// Check if valid second paramter
		return;
	ptr = malloc(sizeof(CheckNumber(socket,ID,"SUB")));
	strcpy(ptr,CheckNumber(socket,ID,"SUB"));
	int channel = atoi(ptr);

		if(checkString(ptr)){
				if(channel < 0 || channel > 255){// If below or over 255 display error
					InvalidChannel(ptr,str,ID,channel,socket);
					return;
				}
				
					if(Clist->next[0].ID == 256){
						for(int i = 0; i < MAXUSER; i++){Clist->next[0].ClientChan[i].Client.ID = 0;}// Initialize all to 0.
						for(int i = 0; i < MAXUSER; i++){
							if(Clist->next[0].ClientChan[i].Client.ID == 0){
								Clist->next[0].ClientChan[i].Client = ID;
								Clist->next[0].ClientChan[i].Read = 0;
								Clist->next[0].ClientChan[i].NonRead = 0;
								Clist->next[0].ClientChan[i].next = NULL;
								break;
							}
						}
						Clist->next[0].ID = channel;
						Clist->next[0].TotalMsg = 0;
						Clist->next[0].next = NULL;
						Clist->tail = 1;
						ConfirmedChannel(ptr,str,ID,channel,socket,"Subscribed to channel: ");
						return;
					}

						for(int i = 0; i < 255; i++)
						{					
							if(Clist->next[i].ID == channel){
								for(int x = 0; x < MAXUSER; x++){
								if(Clist->next[i].ClientChan[x].Client.ID == 0){
								Clist->next[i].ClientChan[x].Client = ID;
								Clist->next[i].ClientChan[x].Read = 0;
								Clist->next[i].ClientChan[x].NonRead = Clist->next[i].TotalMsg;
								Clist->next[i].ClientChan[x].next = NULL;
								ConfirmedChannel(ptr,str,ID,channel,socket,"Subscribed to channel: ");
								return;
								}

								else if(Clist->next[i].ClientChan[x].Client.ID == ID.ID){
								ConfirmedChannel(ptr,str,ID,channel,socket,"Already subscribed to channel: ");
								return;
								}
							}
							ConfirmedChannel(ptr,str,ID,channel,socket,"(FULL) channel: ");
							return;	
							}
							
							
							else if (Clist->next[i].ID == 256)
							{	
								Clist->next[i].ID = channel;
								Clist->next[i].TotalMsg = 0;
								Clist->next[i].next = NULL;
								Clist->tail +=1;

							for(int z = 0; z < MAXUSER; z++){Clist->next[i].ClientChan[z].Client.ID = 0;}// Initialize all to 0.
								for(int x = 0; x < MAXUSER; x++){
								if(Clist->next[i].ClientChan[x].Client.ID == 0){
								Clist->next[i].ClientChan[x].Client = ID;
								Clist->next[i].ClientChan[x].Read = 0;
								Clist->next[i].ClientChan[x].NonRead = 0;
								Clist->next[i].ClientChan[x].next = NULL;
								break;
								}
							}
								ConfirmedChannel(ptr,str,ID,channel,socket,"Subscribed to channel: ");
								return;
							}
						}				
		}	
					
	else{
		InvalidChannel(ptr,str,ID,channel,socket);// If not a valid parameter display error.
		}		
}	


void NEXT(ClientID ID, int socket)
{
	char msg[1024];// Declare Variables
	char str[80];
	char *ptr;

	if(strcmp(ID.Message,"NEXT")==0){// Single NEXT command
		RelayBackMsg(ID, "NEXT ALL:", socket);
		bubbleSort(Clist);
		int Subbed = 0;
		for(int i = 0; i < 255; i++){
		for(int x = 0; x < MAXUSER; x++){
			if(Clist->next[i].ClientChan[x].Client.ID == ID.ID){
				int read = Clist->next[i].ClientChan[x].Read;
				Subbed = 1;
				if(Clist->next[i].Msg[read].truth == 1){// If the message is not not null then read it.
					sprintf(msg,"%d:",Clist->next[i].ID);
					strcat(msg,Clist->next[i].Msg[read].Msg);
					RelayBackMsg(ID,msg,socket);
					Clist->next[i].ClientChan[x].Read++;
					Clist->next[i].ClientChan[x].NonRead--;
				}
		}
		}
		}
		if(Subbed == 0){
			RelayBackMsg(ID, "Not subscribed to any channels", socket);
		}
		RelayBackMsg(ID, "PASS", socket);
		return;
	}

	if(CheckNumber(socket,ID,"NEXT") == "\0")
		return;
	ptr = malloc(sizeof(CheckNumber(socket,ID,"NEXT")));
	strcpy(ptr,CheckNumber(socket,ID,"NEXT"));
	int channel = atoi(ptr);
		
	if(checkString(ptr) && ptr != NULL){
		if(channel < 0 || channel > 255){
			InvalidChannel(ptr,str,ID,channel,socket);
			return;
		}
			
		if(Clist->next[0].ID == 256){
		ConfirmedChannel(ptr,str,ID,channel,socket,"Not subscribed to channel: ");
		return;
		}

		for(int i = 0; i < 255; i++){	
			for(int x = 0; x < MAXUSER; x++){
			if(Clist->next[i].ClientChan[x].Client.ID == ID.ID && Clist->next[i].ID == channel){
				int read = Clist->next[i].ClientChan[x].Read;
				if(Clist->next[i].Msg[read].truth == 1){
					ID.mode = OFF;
					RelayBackMsg(ID, Clist->next[i].Msg[read].Msg, socket);
					Clist->next[i].ClientChan[x].Read++;
					Clist->next[i].ClientChan[x].NonRead--;
					return;
				}

				ID.mode = PASS;
				RelayBackMsg(ID,"\0",socket);
				return;
			}
			}
		}

		ConfirmedChannel(ptr,str,ID,channel,socket,"Not subscribed to channel: ");
		fflush(stdin);
		fflush(stdout);	
		}
		else{
			InvalidChannel(ptr,str,ID,channel,socket);
		}	
}

void LIVEFEED(ClientID ID, int socket){
	char msg[1024];
	char str[80];
	char *ptr;
	int Subbed = 0;

	if(strcmp(ID.Message,"LIVEFEED")==0){
		RelayBackMsg(ID,"LivefeedALL",socket);
		bubbleSort(Clist);
		int i = 0; 
		while(i < Clist->tail){
		for(int x = 0; x < MAXUSER; x++){
			if(Clist->next[i].ClientChan[x].Client.ID == ID.ID && Clist->next[i].ID != 256){
			Subbed = 1;
			while(1){
				int read = Clist->next[i].ClientChan[x].Read;
				ClientID temp; temp.ID = ID.ID; temp.mode = OFF;

				if(Clist->next[i].Msg[read].truth == 0){
					RelayBackMsg(temp,"STOP",socket);
				}

				if(Clist->next[i].Msg[read].truth == 1){
					sprintf(msg,"%d:",Clist->next[i].ID);
					strcat(msg,Clist->next[i].Msg[read].Msg);
					RelayBackMsg(temp,msg,socket);
					Clist->next[i].ClientChan[x].Read++;
					Clist->next[i].ClientChan[x].NonRead--;
				}

				numbytes=recv(socket, &temp, sizeof(ClientID), 0);
				if(numbytes > 0){
					if(strcmp(temp.Message,"BREAK")==0){
						RelayBackMsg(temp,"BEANS",socket);// CONFIRM BACK TO CLIENT SUCCESS THAT SERVER DISBANDED
						return;
						}
					else if(strcmp(temp.Message,"STOP")==0){
						break;
						}
				}
			}
		}
	}

		if(Clist->next[i+1].ID == 256){
			if(Subbed == 0){
				printf("subbed: %d\n", Subbed);
				RelayBackMsg(ID,"NONE",socket);
				return;
			}
			i = 0;
		}else{
			i++;
		}
	}
}

	if(CheckNumber(socket,ID,"LIVEFEED") == "\0")
		return;
	ptr = malloc(sizeof(CheckNumber(socket,ID,"LIVEFEED")));
	strcpy(ptr,CheckNumber(socket,ID,"LIVEFEED"));
	int channel = atoi(ptr);
		
	if(checkString(ptr) && ptr != NULL){
			if(channel < 0 || channel > 255){
				InvalidChannel(ptr,str,ID,channel,socket);
				return;
			}
				
			if(Clist->next[0].ID == 256){
				ConfirmedChannel(ptr,str,ID,channel,socket,"Not subscribed to channel: ");
				return;
			}

			for(int i = 0; i < 255; i++){
			for(int x = 0; x < MAXUSER; x++){
			if(Clist->next[i].ClientChan[x].Client.ID == ID.ID && Clist->next[i].ID == channel){
				RelayBackMsg(ID,"Live Feed:",socket);
				while(1){
					int read = Clist->next[i].ClientChan[x].Read;
					ClientID temp;
					if(Clist->next[i].Msg[read].truth == 0){
						RelayBackMsg(ID,"Pass",socket);
					}
					if(Clist->next[i].Msg[read].truth == 1){
						RelayBackMsg(ID,Clist->next[i].Msg[read].Msg,socket);
						Clist->next[i].ClientChan[x].Read++;
						Clist->next[i].ClientChan[x].NonRead--;
						}

					if ((numbytes=recv(socket, &temp, sizeof(ClientID), 0)) == -1) {
						perror("recv");
						continue;
					}
					else if(numbytes > 0){
						if(strstr(temp.Message,"STOP")!=NULL){
							return;
						}
					}
				}	
			}
			}
	}
	ConfirmedChannel(ptr,str,ID,channel,socket,"Not subscribed to channel: ");	
	}
	else{
		InvalidChannel(ptr,str,ID,channel,socket);
	}
}
/* 
	The Send function which takes 2 parameters <Channel ID> <Message>
	this will place the message within the message linked list even if the
	channel is not subscribed.

	Parameters:
	ID:	The Client struct to be sent.
	socket: The client socket information.
	
	Returns: Nothing
 */

void SEND(ClientID ID, int socket)
{
	int channel;
	char message[1024];
	char str[80];
	char num[10];

	if(CheckPara(ID,"SEND",socket) == -1){
		return;
	}
	/*Extract parameter information*/
	char *ptr = strtok(ID.Message," ");
	if(strcmp(ptr,"SEND") > 0 || strcmp(ptr,"SEND") < 0)
	{
		RelayBackMsg(ID, "Unknown Command", socket);
		return;
	}
		strcpy(message,"");
		for(int i = 0; i < 2; i++){//Get number and message
			ptr = strtok(NULL," ");
			
			if(i == 0){// Handle first parameter
				if(ptr == NULL){
				RelayBackMsg(ID, "Invalid channel: NULL", socket);
				return;
				}
				if(checkString(ptr)){
					channel = atoi(ptr);
					if(channel < 0 || channel > 255){
						InvalidChannel(ptr,str,ID,channel,socket);
						return;
					}
				}
				else{
					InvalidChannel(ptr,str,ID,channel,socket);
					return;
				}
			}
			if(i == 1){// Handle second parameter
				if(ptr == NULL){
				ptr = " ";
				strcat(message,ptr);
				}
				while(ptr != NULL){
				strcat(message,ptr);
				strcat(message," ");
				ptr = strtok(NULL, " ");
				}
				
			}
		}
			/*Place message within the channel if it exists */
			for(int i = 0; i < 255; i++){
				if(Clist->next[i].ID == channel){	
					int c = Clist->next[i].TotalMsg;
					if(Clist->next[i].Msg[c].truth == 0){// Create head of message if it never existed
						strcpy(Clist->next[i].Msg[c].Msg,message);
						Clist->next[i].Msg[c].truth = 1;
						Clist->next[i].TotalMsg++;

						for(int x = 0; x < MAXUSER; x++){
							if(Clist->next[i].ClientChan[x].Client.ID != 0){
								printf("nice twice");
								Clist->next[i].ClientChan[x].NonRead++;
							}
						}
					}
					RelayBackMsg(ID, "sent",socket); return;
				}
			}

			/*Create message and new channel if the channels dont currently exist*/
			Channel *new = CreateChannelMessage(channel,message,Channels);
				if(Channels->ID == 256)
			{
				Channels = new;
				ChannelHead = Channels;
				ChannelTail = Channels;
			}
			else
			{
				ChannelTail->next = new;
				ChannelTail = ChannelTail->next;			
			}
		RelayBackMsg(ID, "sent",socket);
}