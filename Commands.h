#include <stdio.h> 
#include <stdlib.h> 
#include <errno.h> 
#include <string.h> 
#include <netdb.h> 
#include <sys/types.h> 
#include <netinet/in.h> 
#include <sys/socket.h> 
#include <sys/shm.h>
#include <sys/time.h>
#include <sys/wait.h> 
#include <sys/ipc.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdbool.h>
#include <semaphore.h>

#ifndef COMMANDS_H_
#define COMMANDS_H_


#define MAXDATASIZE 100 /* max number of bytes we can get at once */
#define QUERY_LENGTH 50
#define MAXUSER 5

typedef enum {PASS, OFF, SHUTDOWN, STOP, BREAK} flags;


typedef struct ClientID_t {
   char Message[1024];
   int ID;
   int socket;
   pid_t PID;
   flags mode;
}ClientID;

typedef struct ChannelClient_t {
   ClientID Client;
   int Read;
   int NonRead;
}ChannelClient;

typedef struct Messages_t {
   char Msg[1024];
   int truth;
}Messages;

typedef struct Channel_t {
   int ID;
   int TotalMsg;
   struct ChannelClient_t ClientChan[5];
   struct Messages_t Msg[1000];
}Channel;

typedef struct ChannelList_t {
   struct Channel_t next[255];
   int tail;
}ChannelList;

void ssend(int sock, char* message) {
    size_t len = strlen(message);
    if (send(sock, (char*)message, len, 0) == -1)
        perror("send");
}

void ssendClientID(int sock, ClientID Client){
    if (send(sock, &Client, sizeof(ClientID), 0) == -1)
        perror("send");
}

void ExistChannel(char ptr[],char str[], ClientID ID, int channel, int socket){
	strcpy(str,"Not Subscribed to Channel: ");
	strcat(str,ptr);
	strcpy(ID.Message,str);
	ssendClientID(socket, ID);
}

void ConfirmedChannel(char ptr[],char str[], ClientID ID, int channel, int socket, char Message[]){
	strcpy(str,Message);
	strcat(str,ptr);
	strcpy(ID.Message, str);
	ssendClientID(socket,ID);
}

void InvalidChannel(char ptr[],char str[], ClientID ID, int channel, int socket){
    strcpy(str,"Invalid Channel: ");
	strcat(str,ptr);
	strcpy(ID.Message, str);
	ssendClientID(socket,ID);
}

void RelayBackMsg(ClientID ID, char* clientmsg, int socket){
	fflush(stdin);
	strcpy(ID.Message, clientmsg);
	ssendClientID(socket,ID);
	fflush(stdout);
}

void CreateChannelMessage(int channel, char *message , ChannelList *Clist)
{
    int c = Clist->tail;
    for(int i = 0; i < MAXUSER; i++){Clist->next[c].ClientChan[i].Client.ID = 0;}// Initialize all to 0.
    Clist->next[c].ID = channel;
    Clist->next[c].TotalMsg = 0;
    Clist->tail++;

    int x = Clist->next[c].TotalMsg;
    strcpy(Clist->next[c].Msg[x].Msg,message);
    Clist->next[c].Msg[x].truth = 1;
    Clist->next[c].Msg[x+1].truth = 0;
    Clist->next[c].TotalMsg++;
}

void CreateSub(ChannelList *Clist,ClientID ID,int channel, int idx, int NONREAD, int init)
{
    if(init = 1)
        for(int i = 0; i < MAXUSER; i++){Clist->next[idx].ClientChan[idx].Client.ID = 0;}// Initialize all to 0.
        Clist->next[idx].ID = channel;
        Clist->next[idx].TotalMsg = 0;
        Clist->tail += 1;

    for(int x = 0; x < MAXUSER; x++){
        if(Clist->next[idx].ClientChan[x].Client.ID == 0){
            Clist->next[idx].ClientChan[x].Client = ID;
            Clist->next[idx].ClientChan[x].Read = 0;
            Clist->next[idx].ClientChan[x].NonRead = NONREAD;
            break;
        }
    }
    
}

void CLOSESOCKET(ClientID Clients[])
{
    for(int i = 0; i < 5; i++){
		if(Clients[i].ID!=0){// remove sockets
			Clients[i].PID = 0;
			Clients[i].ID = 0;
			close(Clients[i].socket);
			shutdown(Clients[i].socket,SHUT_RDWR);
		}
	}
}

int CheckPara(ClientID ID, char* clientmsg, int socket){
	if(strcmp(ID.Message,clientmsg)==0){
	RelayBackMsg(ID, "Invalid channel: NULL", socket);
	return -1;
	}
}

char *CheckNumber(int socket, ClientID ID, char *message){
    char *ptr = strtok(ID.Message," ");
    if(strcmp(ptr,message) > 0 || strcmp(ptr,message) < 0 )
		{
			RelayBackMsg(ID, "Unknown Command", socket);
			return "\0";
		}
			for(int i = 0; i < 1; i++){//Get number
				ptr = strtok(NULL," ");
			}
		if(ptr == NULL){
			RelayBackMsg(ID, "Invalid channel: NULL", socket);
			return "\0";
		}
        return ptr;
}

int checkString(char *Str)
{
    char *ptr = Str;

    while( *ptr )
    {

        if( ! (*ptr >= 0x30 && *ptr <= 0x39 ) )
        {
            return 0;
        }
        ptr++;
    }

    return 1;
}

void swap(Channel *a, Channel *b) 
{ 
    Channel temp = *a; 
    *a = *b; 
    *b = *a;

}

void bubbleSort(ChannelList *stacker) 
{ 
    /* Checking for empty list */
    if (stacker->next[0].ID == 256) 
        return; 
        
	for (int i = 0; i < stacker->tail; i++)                    
	{
		for (int j = 0; j < stacker->tail; j++)             
		{
			if (stacker->next[i].ID < stacker->next[j].ID)                
			{
				Channel tmp = stacker->next[i];         
				stacker->next[i] = stacker->next[j];           
				stacker->next[j] = tmp;             
		}
	}
    
}
}
#endif 