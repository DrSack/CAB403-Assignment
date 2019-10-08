#include "Commands.h"

#define BACKLOG 10 
#define MAXUSER 5
#define CNULL 256


ChannelList *Clist;
ClientID totalusers[5];
sem_t *full, *empty;

int numbytes;
int sockfd;
int sigbool = 0;
int shmid;

void InitializeMemory();
void SocketInitialize();
void ConnectAndAssign();
void RunClient();
void SUB();
void CHANNELS();
void UNSUB();
void UNSUB_ALL();
void NEXT();
void LIVEFEED();
void SEND();
void ConfirmedChannel();
void Hold();
void Release();

void close_server()//Close down the server if SIGINT is called
{
	printf("\nServer Closing...\n");
	sem_destroy(empty);
	sem_destroy(full);
	
	CLOSESOCKET(totalusers);
	close(sockfd);
	shutdown(sockfd,SHUT_RDWR);
	shmctl(shmid, IPC_RMID, 0);//detach shared memory
	exit(1);
}

void handler()// This is called when a child process ends.
{
	 pid_t chpid = wait(NULL);//obtain child ID.

	for(int i = 0; i < 5; i++){
		if(totalusers[i].PID==chpid){// deslist the user from the queue.
			totalusers[i].PID = 0;
			totalusers[i].ID = 0;
			break;
		}
	}
}

int main(int argc, char *argv[])
{
	int new_fd, shm_id;  /* listen on sock_fd, new connection on new_fd */
	struct sockaddr_in my_addr;    /* my address information */
	socklen_t sin_size;

	InitializeMemory();
	SocketInitialize(argc, argv, my_addr);
	ConnectAndAssign(sin_size,new_fd,sockfd);

	return EXIT_SUCCESS;
}

/*
	This is the main Server functionality where the main process will assign and fork processes
	to allocate a process for a client, from then on the child process will send, read and recieve
	new messages based on client preferences and update the shared memory

	Parameters:
	siz_size: The socket length size.
	new_fd: The new client socket number.
	sockfd: The server socket number.

	Returns:
	Nothing
*/

void ConnectAndAssign(socklen_t sin_size, int new_fd, int sockfd)
{
	printf("server starts listnening ...\n");
	while(1){//RUN THE SERVER
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
					if(natural == 0){// child process runs the background
						RunClient(new_fd);
					}
					else {//Main process adds the user to the queue.
						if(sigbool == 0){
							signal(SIGCHLD, handler);
							signal(SIGINT, close_server);
							sigbool = 1;
						}	
						for(int i = 0; i < 5; i++){//Add new client to queue
						if(totalusers[i].ID==0){
							totalusers[i].ID=i+1;
							totalusers[i].PID = natural;
							totalusers[i].socket = new_fd;
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

/*
	This is the main function to be run by the child process, 
	this takes input commands from the connected client
	and reads and writes depending on the request sent by the client.
	Parameters:
	new_fd: The socket number of the client
	Returns:
	Nothing
*/

void RunClient(int new_fd){// Main client function run by child process
	int destroy = 0;
	while(1){
		fflush(stdin);//Flush input and output
		fflush(stdout);	
		ClientID temp; temp.mode = OFF;// Set enum value to OFF, this makes sure that messages are to be read.
    	while (1) {
	    	if ((numbytes=recv(new_fd, &temp, sizeof(ClientID), 0)) == -1) {
				perror("recv");
        	}
			if (numbytes > 0){
				if(strcmp(temp.Message,"NULL")!=0){
					printf("From Client %d: %s\n",temp.ID,temp.Message);
				}
				if(strstr(temp.Message,"UNSUB")!=NULL){
					UNSUB(temp,new_fd);
					break;
				}
				else if(strstr(temp.Message,"SUB")!=NULL){
					SUB(temp, new_fd);
					break;
				}
				else if(strcmp(temp.Message,"CHANNELS")==0){
					CHANNELS(temp,new_fd);
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
			else{//If recv has an error kill socket and close process.
				UNSUB_ALL(temp);
				destroy = 1;
				close(new_fd);
				break;
			}
		}
		if(destroy == 1){//If BYE is sent kill socket and close process.
			printf("Client %d has disconnected\n", temp.ID);
			UNSUB_ALL(temp);
			close(new_fd);
			break;
		}
	}
	if(shmdt(Clist)==-1){
		perror("shmdt:");
		exit(0);
	}
	exit(0);//Kill child process
}

/* 
	The SUB function takes 1 parameter <Channel ID>
	This function allows the client to subscribe to channels ranging from 0 - 255
	Each channel has its own unique messages that the client can read or send on their own.
	Parameters:
	ID:	The Client struct to be sent.
	socket: The client socket information.
	
	Returns: Nothing
 */

void SUB(ClientID ID, int socket)
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
			if(Clist->next[0].ID == 256){// If the head is 256(null) initiate start of array
				CreateSub(Clist,ID,channel,0);
				ConfirmedChannel(ptr,str,ID,channel,socket,"Subscribed to channel: ");
				return;
			}
				for(int i = 0; i < 255; i++)// Search through all channels
				{					
					if(Clist->next[i].ID == channel){//IF a channel is found with the same parameter
						for(int x = 0; x < MAXUSER; x++){
						if(Clist->next[i].ClientChan[x].Client.ID == 0){//Add id to clientchan.
						Clist->next[i].ClientChan[x].Client = ID;
						Clist->next[i].ClientChan[x].Read = 0;
						Clist->next[i].ClientChan[x].NonRead = Clist->next[i].TotalMsg;
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
					else if (Clist->next[i].ID == 256)//IF the next node is 256(null), then add a new channel
					{	
						CreateSub(Clist,ID,channel,i);
						ConfirmedChannel(ptr,str,ID,channel,socket,"Subscribed to channel: ");
						return;
					}
				}				
		}				
	else{
		InvalidChannel(ptr,str,ID,channel,socket);// If not a valid parameter display error.
		}		
}

/* 
	The CHANNELS function which takes no parameters
	This sends back to the client all the subscribed channels that client has.
	Parameters:
	temp:	The Client struct to be sent.
	socket: The client socket information.
	
	Returns: Nothing
 */

void CHANNELS(ClientID temp, int socket){// Sort and List out the current Subscribed Channels
	Hold();
	RelayBackMsg(temp,"CHANNELS ALL:",socket);
	bubbleSort(Clist);
	for(int i = 0; i < Clist->tail; i++){
				for(int x = 0; x < MAXUSER; x++){//Check if the Client has an subscribed channels
					if(Clist->next[i].ClientChan[x].Client.ID == temp.ID){
					char num[128];
					char nice[256];
					strcpy(nice, "Subscribed to ");
		    		sprintf(num,"%d  \tTotal Messages:%d \nRead Messages:%d \tUnread Messages:%d \n\n", Clist->next[i].ID, 
					Clist->next[i].TotalMsg, Clist->next[i].ClientChan[x].Read, Clist->next[i].ClientChan[x].NonRead);
					strcat(nice,num);
					RelayBackMsg(temp,nice,socket);//Send back information.
					}
				}
		}
		Release();
		temp.mode = PASS;//Send back pass to signal done
		RelayBackMsg(temp,"",socket);
}

/* 
	The UNSUB function takes 1 parameter <Channel ID>
	This function allows the client to unsubscribe from channels ranging from 0 - 255.
	Parameters:
	ID:	The Client struct to be sent.
	socket: The client socket information.
	
	Returns: Nothing
 */

void UNSUB(ClientID ID, int socket){
	char str[80];// Initialize buffer
	char *ptr;// Get pointer to add number.

	if(CheckPara(ID,"UNSUB",socket) == -1)//Check if UNSUB has no parameters
		return;
	if(CheckNumber(socket,ID,"UNSUB") == "\0")//Check if the second parameter is an actual number
		return;
	ptr = malloc(sizeof(CheckNumber(socket,ID,"SUB")));
	strcpy(ptr,CheckNumber(socket,ID,"UNSUB"));
	int channel = atoi(ptr);

	if(checkString(ptr)){
		if(channel < 0 || channel > 255){
			InvalidChannel(ptr,str,ID,channel,socket);
			return;
		}
			if(Clist->next[0].ID == 256){//Throw error if head doesnt exist
			ConfirmedChannel(ptr,str,ID,channel,socket,"Not subscribed to channel: ");
			return;
			}
				for(int i = 0; i < Clist->tail; i++)//Check through the list.
				{	
					if(Clist->next[i].ID == channel){
								for(int x = 0; x < MAXUSER; x++){
								if(Clist->next[i].ClientChan[x].Client.ID == ID.ID){
								sem_wait(empty);
								Clist->next[i].ClientChan[x].Client.ID = 0;
								sem_post(empty);
								ConfirmedChannel(ptr,str,ID,channel,socket,"Unsubscribed from channel: ");
								return;
								}
							}
					}
				}
				ConfirmedChannel(ptr,str,ID,channel,socket,"Not subscribed to channel: ");
		}
else{
	InvalidChannel(ptr,str,ID,channel,socket);//Exeception if below 0 or above 255.
}
}

/* 
	This unsubs all channels that the client is subscribed to
	Parameters:
	temp:	The Client ID needed to unsub the channel from.
	
	Returns: Nothing
 */

void UNSUB_ALL(ClientID temp){//Unsub all channels the client is subbed to.
		for(int i = 0; i < Clist->tail; i++){
				for(int x = 0; x < MAXUSER; x++){
					if(Clist->next[i].ClientChan[x].Client.ID == temp.ID){
					sem_wait(empty);
					Clist->next[i].ClientChan[x].Client.ID = 0;
					sem_post(empty);
				}
			}
		}
}

/* 
	The NEXT function takes 1 parameter <Channel ID>
	This function allows the client to see messages 1 step at a time.
	With no parameter the client will be able to read all messages at
	one step.

	With NEXT channel id the user will see messages for that specific
	channel one step only.
	Parameters:
	ID:	The Client struct to be sent.
	socket: The client socket information.
	
	Returns: Nothing
 */

void NEXT(ClientID ID, int socket)
{
	char msg[1024];// Declare Variables
	char str[80];
	char *ptr;

	if(strcmp(ID.Message,"NEXT")==0){// Single NEXT command
		RelayBackMsg(ID, "NEXT ALL:", socket);
		Hold();
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
		Release();
		if(Subbed == 0){
			RelayBackMsg(ID, "Not subscribed to any channels.", socket);
		}
		RelayBackMsg(ID, "PASS", socket);
		return;
	}

	if(CheckNumber(socket,ID,"NEXT") == "\0")// Check if number is valid
		return;
	ptr = malloc(sizeof(CheckNumber(socket,ID,"NEXT")));
	strcpy(ptr,CheckNumber(socket,ID,"NEXT"));
	int channel = atoi(ptr);//Convert string to int
		
	if(checkString(ptr) && ptr != NULL){
		if(channel < 0 || channel > 255){// check if in range
			InvalidChannel(ptr,str,ID,channel,socket);
			return;
		}
		if(Clist->next[0].ID == 256){//if head is null
		ConfirmedChannel(ptr,str,ID,channel,socket,"Not subscribed to channel: ");
		return;
		}

		for(int i = 0; i < 255; i++){	
			for(int x = 0; x < MAXUSER; x++){
			if(Clist->next[i].ClientChan[x].Client.ID == ID.ID && Clist->next[i].ID == channel){
				Hold();
				int read = Clist->next[i].ClientChan[x].Read;
				if(Clist->next[i].Msg[read].truth == 1){
					ID.mode = OFF;
					RelayBackMsg(ID, Clist->next[i].Msg[read].Msg, socket);
					Clist->next[i].ClientChan[x].Read++;
					Clist->next[i].ClientChan[x].NonRead--;
					Release();
					return;
				}
				Release();
				RelayBackMsg(ID,"\n",socket);
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

/* 
	The LIVEFEED function takes 1 parameter <Channel ID>
	This function allows the client to see messages arriving in real time.
	As well as reading the current messages that have not been read yet.
	Parameters:
	ID:	The Client struct to be sent.
	socket: The client socket information.
	
	Returns: Nothing
 */

void LIVEFEED(ClientID ID, int socket){
	char msg[1024];
	char str[80];
	char *ptr;
	int Subbed = 0;

	if(strcmp(ID.Message,"LIVEFEED")==0){//If LIVEFEED is parameterless
		RelayBackMsg(ID,"LivefeedALL",socket);
		bubbleSort(Clist);
		int i = 0; 
		while(i < Clist->tail){
		for(int x = 0; x < MAXUSER; x++){//Check if any channels exist that the user is subbed to.
			if(Clist->next[i].ClientChan[x].Client.ID == ID.ID && Clist->next[i].ID != 256){
			Subbed = 1;
			while(1){
				Hold();
				int read = Clist->next[i].ClientChan[x].Read;
				ClientID temp; temp.ID = ID.ID; temp.mode = PASS;

				if(Clist->next[i].Msg[read].truth == 0){
					temp.mode = STOP;
					RelayBackMsg(temp," ",socket);
				}

				if(Clist->next[i].Msg[read].truth == 1){
					temp.mode = PASS;
					sprintf(msg,"%d:",Clist->next[i].ID);
					strcat(msg,Clist->next[i].Msg[read].Msg);
					RelayBackMsg(temp,msg,socket);
					Clist->next[i].ClientChan[x].Read+=1;
					Clist->next[i].ClientChan[x].NonRead-=1;
				}
				Release();
				numbytes=recv(socket, &temp, sizeof(ClientID), 0);
				if(numbytes > 0){
					if(temp.mode == BREAK){
						return;
						}
					else if(temp.mode == STOP){
						break;
						}
				}
			}
		}
	}
		if(Clist->next[i+1].ID == 256){//If the next node is invalid
			if(Subbed == 0){//If no channels were open, return.
				ID.mode = NONE;
				RelayBackMsg(ID," ",socket);
				return;
			}
			i = 0;//return to beginning on list.
		}else{
			i++;//Continue
		}
	}

	if(Clist->tail == 0){//If no nodes exist.
		ID.mode = NONE;
		RelayBackMsg(ID," ",socket);
		return;
	}
}

	if(CheckNumber(socket,ID,"LIVEFEED") == "\0")//Check if number is valid
		return;
	ptr = malloc(sizeof(CheckNumber(socket,ID,"LIVEFEED")));
	strcpy(ptr,CheckNumber(socket,ID,"LIVEFEED"));
	int channel = atoi(ptr);//convert string to int	
	if(checkString(ptr) && ptr != NULL){
			if(channel < 0 || channel > 255){//invalid if above or below 0 or 255.
				InvalidChannel(ptr,str,ID,channel,socket);
				return;
			}
				
			if(Clist->next[0].ID == 256){//If head is "empty"
				ConfirmedChannel(ptr,str,ID,channel,socket,"Not subscribed to channel: ");
				return;
			}

			for(int i = 0; i < Clist->tail; i++){
			for(int x = 0; x < MAXUSER; x++){
			if(Clist->next[i].ClientChan[x].Client.ID == ID.ID && Clist->next[i].ID == channel){
				RelayBackMsg(ID,"LivefeedALL",socket);
				while(1){
					Hold();
					int read = Clist->next[i].ClientChan[x].Read;
					ClientID temp; temp.ID = ID.ID; temp.mode = PASS;

					if(Clist->next[i].Msg[read].truth == 0){
						temp.mode = STOP;
						RelayBackMsg(temp," ",socket);//Parse pass to confirm no msg.
					}

					if(Clist->next[i].Msg[read].truth == 1){
						temp.mode = PASS;
						RelayBackMsg(temp,Clist->next[i].Msg[read].Msg,socket);//send msg
						Clist->next[i].ClientChan[x].Read+=1;
						Clist->next[i].ClientChan[x].NonRead-=1;//Increment and Decrement read and non read values
						}

					Release();
					numbytes=recv(socket, &temp, sizeof(ClientID), 0);
					if(numbytes > 0){
						if(temp.mode == BREAK){
							return;//If client has exit then stop loop.
						}
						else if(temp.mode == STOP){
							continue;//continue the loop
						}
						
					}
				}	
			}
			}
	}//If none are hit from the forloop then Not Subscribed
	ConfirmedChannel(ptr,str,ID,channel,socket,"Not subscribed to channel: ");	
	}
	else{
		InvalidChannel(ptr,str,ID,channel,socket);// Catch any other errors that slip
	}
}

/* 
	The Send function which takes 2 parameters to be sent by the client <Channel ID> <Message>
	this will place the message within the message array even if the
	channel is not subscribed.
	Parameters:
	ID:	The Client struct to be sent.
	socket: The client socket information.
	
	Returns: Nothing
 */

void SEND(ClientID ID, int socket)
{
	int channel;
	char str[80];
	char num[10];
	char message[1024];
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
				if(ptr == NULL){//If nothing exists for 1st parameter
				RelayBackMsg(ID, "Invalid channel: NULL", socket);
				return;
				}
				if(checkString(ptr)){
					channel = atoi(ptr);
					if(channel < 0 || channel > 255){//check if number valid.
						InvalidChannel(ptr,str,ID,channel,socket);
						return;
					}
				}
				else{//Return anyother value that isnt an int.
					InvalidChannel(ptr,str,ID,channel,socket);
					return;
				}
			}
			if(i == 1){// Handle second parameter
				if(ptr == NULL){//If null add an empty space.
				ptr = " ";
				strcat(message,ptr);
				}
				while(ptr != NULL){//Add message to ptr
				strcat(message,ptr);
				strcat(message," ");
				ptr = strtok(NULL, " ");
				}			
			}
		}
			/*Place message within the channel if it exists */
			for(int i = 0; i < Clist->tail; i++){
				if(Clist->next[i].ID == channel){
					int c = Clist->next[i].TotalMsg;
					if(Clist->next[i].Msg[c].truth == 0){// Create head of message if it never existed
						sem_wait(empty);
						strcpy(Clist->next[i].Msg[c].Msg,message);
						Clist->next[i].Msg[c].truth = 1;//Mark as checked
						Clist->next[i].Msg[c+1].truth = 0;//Mark as null for next message.
						Clist->next[i].TotalMsg++;
						for(int x = 0; x < MAXUSER; x++){
							if(Clist->next[i].ClientChan[x].Client.ID != 0){
								Clist->next[i].ClientChan[x].NonRead++;//Increment non-read msg's to all clients.
							}
						}
					}
					sem_post(empty);
					RelayBackMsg(ID, "sent",socket); return;
				}
			}
			if(Clist->next[Clist->tail].ID == 256)
				sem_wait(empty);
				CreateChannelMessage(channel,message,Clist);
				sem_post(empty);
			RelayBackMsg(ID, "sent",socket); return;	
}



/* 
	This is responsible for initializing the shared memory.
	This also sets the Channel ID's to 256 which is an
	invalid channel number. As NULL cant be used for non-pointers.
	This also sets the default user ID to 0.
	Parameters: Nothing
	
	Returns: Nothing
 */

void InitializeMemory()
{	/*Initialize semaphores for synchronization */
	if ((full = sem_open("this is the semaphore2", O_CREAT, 0644, 1)) == SEM_FAILED) {
    perror("semaphore initilization");
    exit(1);
 	}

	if ((empty = sem_open("this is the semaphore", O_CREAT, 0644, 1)) == SEM_FAILED) {
    perror("semaphore initilization");
    exit(1);
 	}

	shmid = shmget(IPC_PRIVATE, sizeof(ChannelList), IPC_CREAT | 0666);// setup shared memory
	Clist = shmat(shmid, 0, 0);
	Clist->tail = 0;
	Clist->readCount = 0;
	for(int i = 0; i < 255; i++){// Initialize all structs
		Clist->next[i].ID = 256;
		for(int x = 0; x < MAXUSER; x++){
			Clist->next[i].ClientChan[x].Client.ID = 0;
		}
		Clist->next[i].Msg[0].truth = 0;// declare first msg of every channel 0(NULL);
	}

	for(int i = 0; i < 5; i++){// Default value of ID
		totalusers[i].ID=0;
	}
}

/* 
	This is responsible initializing the sockets 
	and binding and listening for other client sockets
	Parameters:
	argc: The total amount of parameters from main
	argv: The string value of those parameters from main 
	my_addr: The socket address information.
	
	Returns: Nothing
 */

void SocketInitialize(int argc, char *argv[],struct sockaddr_in my_addr)
{
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
}
/* This is the reader semaphore hold function */
void Hold()
{
	sem_wait(full);
	Clist->readCount+=1;
	if(Clist->readCount == 1)
		sem_wait(empty);
	sem_post(full);
}

/*This function releases the semaphore after the process has read the data */
void Release()
{
	sem_wait(full);
	Clist->readCount-=1;
	if(Clist->readCount==0)
		sem_post(empty);
	sem_post(full);
}