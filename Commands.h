#include <sys/shm.h>
#ifndef COMMANDS_H_
#define COMMANDS_H_


#define MAXDATASIZE 100 /* max number of bytes we can get at once */
#define QUERY_LENGTH 50

typedef enum {PASS, OFF} flags;


typedef struct ClientID_t {
   char Message[1024];
   int ID;
   pid_t PID;
   int BREAKALL;
   flags mode;
   struct ClientID_t *next;
}ClientID;

typedef struct ChannelClient_t {
   ClientID Client;
   int Read;
   int NonRead;
   struct ChannelClient_t  *next;
}ChannelClient;

typedef struct Messages_t {
   char Msg[1024];
   struct Messages_t  *next;
   struct Messages_t  *tail;
}Messages;

typedef struct Channel_t {
   int ID;
   int TotalMsg;
   struct ChannelClient_t ClientChan[5];
   struct Channel_t *next;
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

Channel *CreateChannelMessage(int channel, char *message , Channel *new)
{
    /* 
    new->ClientChan = NULL;
    new->TotalMsg = 0;
    new->ID = channel;
    new->next = NULL;
    
    new->Msg = malloc(sizeof(Messages));
    new->Msg->tail = new->Msg;
    new->Msg->next = NULL;
    strcpy(new->Msg->Msg,message);
    new->TotalMsg++;
    return new;
    */
}

ChannelClient *CreateNewCClient(ClientID ID, int NonRead, int shmemid)
{
    /*
    ChannelClient *new;
    new =(ChannelClient *)shmat(shmemid,NULL,0);
	new->Client = ID;
	new->NonRead = NonRead;
	new->Read = 0;
    new->next = NULL;
    return new;
    */
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

void trimTrailing(char * str)
{
    int index, i;
    index = -1;

    i = 0;
    while(str[i] != '\0')
    {
        if(str[i] != ' ' && str[i] != '\t' && str[i] != '\n')
        {
            index= i;
        }

        i++;
    }

    str[index + 1] = '\0';
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