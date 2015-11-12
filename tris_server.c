//server
#include<stdlib.h>
#include<string.h>
#include<stdio.h>
#include<unistd.h>

#include<sys/socket.h>
#include<sys/select.h>
#include<sys/time.h>

#include<arpa/inet.h>
#include<netinet/in.h>
#include<errno.h>

#include"crypto_lib.h"

//===== COSTANTS ==================
#define MAX_CONNECTION 	10  //max number of user
#define MAX_LENGTH      10	//max length for the username TODO: move to a common file?
//=================================

//===== VARIABLES =================
//one user
struct user {
	unsigned char   username[MAX_LENGTH];
	short int	    UDP_port;
	int		        socket;
	unsigned long	address;
	unsigned char 	nonce[NONCE_SIZE];
	int 	        status; 	//0=free, 1=busy
	struct user   	*other;		//user user is connected to
	struct user   	*next;      //next user in the list
};

struct user   *users;         //list of the connected users
int             tot_users = 0;

struct user   *client;        //user that is communicating with the server

//TODO make local (?)
//config
struct sockaddr_in 	server_addr,    //only used in main
                    user_addr;    //only used in main and add_user
int	listener;                       //listening socket descriptor (only in main)

//set for select
fd_set 	master,     //master file descriptor list
        tmp_fd;     //temporary file descriptor list for select
int		max_fd;
//=================================

//===== UTILITY FUNCTIONS =========

//----- valid_port ------
int valid_port(int port) {
	if(port<1024 || port>=65536)
		return 0;
	return 1;
}

void regenerate(unsigned char* username){
	    username[MAX_LENGTH] = '\0';
}

//----- add_to_list ------    adds new user (elem) to the users list
void add_to_list(struct user *elem) {
	tot_users++;
	elem->next = users;
	users = elem;
}

//----- remove_from_list ------ removes specified user (elem) from the users list
void remove_from_list(struct user *elem) {
	struct user *tmp = users;
	
	if(!tmp) {      //empty list (tot_users==0)
		return;
	}
	if(tmp==elem) { //elem is first item
		users = users->next;
		free(tmp);
		tot_users--;
		return;
	}
	while(tmp->next!=elem && tmp->next!=NULL)
		tmp = tmp->next;
	if(tmp->next==NULL) //elem not found
		return;
	tot_users--;
	tmp->next = tmp->next->next;
	return;
}

//----- find_user_by_sd --------  returns user with socket = sd, NULL otherwise
struct user* find_user_by_sd(int sd) {
	struct user *tmp = users;
	while(tmp!=NULL) {
		if(tmp->socket==sd)
			return tmp;
		tmp = tmp->next;
	}
	return NULL;
}

//----- find_user_by_username --- returns user with username = name, NULL otherwise
struct user* find_user_by_username(char *name) {
	struct user *tmp = users;
	while(tmp!=NULL) {
		if(strcmp(tmp->username, name)==0)	//equals
			return tmp;
		tmp = tmp->next;
	}
	return NULL;
}

//----- exist -------------	returns 1 if user with username name is already in the list
int exist(char *name) {
	struct user *tmp = users;
	while(tmp!=NULL) {
		if(strcmp(tmp->username, name)==0)	//equals
			return 1;
		tmp = tmp->next;
	}
	return 0;
}

//=================================

//===== FUNCTIONS =================

//----- cmd_who ---------------
void cmd_who() {
	int 	ret,
            length;
	char	cmd = 'w';
	struct user *tmp = users;
	
	//inform client that I'm replying to a who command
	ret = send(client->socket, (void *)&cmd, sizeof(cmd), 0);
	if(ret==-1) {
		printf("cmd_who error: error while sending command\n");
		exit(1);
	}
			
	ret = send(client->socket, (void *)&tot_users, sizeof(tot_users), 0);
	if(ret==-1 || ret<sizeof(tot_users)) {
		printf("cmd_who error: error while sending tot_users\n");
		exit(1);
	}
	while(tmp) {	//until there are users (tot_users times)
		
		//send user name
		ret = send(client->socket, (void *)tmp->username, MAX_LENGTH, 0);
		if(ret==-1 || ret<MAX_LENGTH) {
			printf("cmd_who error: error while sending username\n");
			exit(1);
		} 
		
		//send user status
		ret = send(client->socket, (void *)&tmp->status, sizeof(int), 0);
		if(ret==-1 || ret<sizeof(int)) {
			printf("cmd_who error: error while sending user status\n");
			exit(1);
		}
		
		tmp = tmp->next;
	}
}

//----- cmd_disconnect---------
void cmd_disconnect() {
	//check if client is busy done on client side
	//if(client->status==1) {
		printf("%s disconnected from %s\n", client->username, client->other->username);
		client->status = 0;
		client->other->status = 0;
		printf("%s is free\n", client->username);
		printf("%s is free\n", client->other->username);
		client->other=NULL;
	/*}
	else {
		printf("%s is not busy!\n", client->username);
	}*/
}

//------ second msg ------ prepares and returns message with the syntax: (Kab, ip+port, nonce)
unsigned char* second_msg(int msg_size, unsigned char* session_key, unsigned char* portIP, unsigned char* nonce){
	
	int portIPsize = sizeof(short int)+sizeof(unsigned long);
	unsigned char* msg = calloc (msg_size, sizeof(unsigned char));
	memcpy(msg, session_key, SESSION_KEY_SIZE);
	memcpy(msg+SESSION_KEY_SIZE, portIP, portIPsize);
	memcpy(msg+SESSION_KEY_SIZE + portIPsize, nonce, NONCE_SIZE);
	return msg;	
}

void send_second_msg(struct user* msgdest, struct user* other,unsigned char* session_key){
	unsigned char port[10], address[50];
	//port = htons(other->UDP_port);
	sprintf(address, "%lu", other->address);
	printf("IP: %s\n", address);
	sprintf(port, "%d", htons(other->UDP_port));
	printf("port: %s\n", port);
	strcat(port, address);
	printf("complete: %s\n", port);
	//send enc(Kab, ip+port, noncedest)
	unsigned char* secret 	= calloc(32, sizeof(unsigned char));
	secret = retrieve_key(32, "secret_file");
	secret = retrieve_key(32, "secret_file");
	//msg2 = second_msg(SESSION_KEY_SIZE+portIPsize+NONCE_SIZE, session_key, port )	
} 

//----- cmd_connect------------
void cmd_connect() {
	int 	ret,
            length;
    char	cmd;
    short int port;
    struct user   *client2;
	unsigned char othersname[MAX_LENGTH];
    unsigned char msg1[MAX_LENGTH*2+NONCE_SIZE];
	
	//receive first message from A (A->S : A, B, Na)
	ret = recv(client->socket, (void *)msg1, (MAX_LENGTH*2+NONCE_SIZE), 0);
	if(ret==-1) {
		printf("cmd_connect error: error while receiving first message\n");
		exit(1);
	}
	
	//retrieve B and Na from msg1
	strncpy(othersname, msg1+MAX_LENGTH, MAX_LENGTH);
	printf("othersname %s\n", othersname);
	strncpy(client->nonce, msg1+MAX_LENGTH*2, NONCE_SIZE);
	//TODO check nonce freshness
	
    //debug
	printf("othersname: %s\n", othersname);
	
	//SaRa - tmp
	printf("msg1 %s\n",msg1);
	//if (isFreshNokey(msg1,nonce)==1) printf("it's fresh!\n");
	//else  printf("it's not fresh!\n");
	//
	
	//inform client (A) that I'm replying to his connect request
	cmd = 'c'; //connect
	ret = send(client->socket, (void *)&cmd, sizeof(char), 0);
	if(ret==-1) {
		printf("cmd_connect error: error while sending connect reply to client\n");
		exit(1);
	}
	
	if(!exist(othersname)) {    //B is not a connected user
		cmd = 'i';
		ret = send(client->socket, (void *)&cmd, sizeof(char), 0);
		if(ret==-1) {
			printf("cmd_connect error: error while sending user not existent\n");
			exit(1);
		}
		return;
	}
	
	client2 = find_user_by_username(othersname);
	//forward connection request to client2 (B)
    //send command identifying request
    cmd = 'o';
	ret = send(client2->socket, (void *)&cmd, sizeof(char), 0);
	if(ret==-1) {
		printf("cmd_connect error: error while sending connection request to client2\n");
		exit(1);
	}
	
	//send client (A) username to client2 (B) (S->B : A)
	ret = send(client2->socket, (void *)client->username, MAX_LENGTH, 0);
	if(ret==-1) {
		printf("cmd_connect error: error while sending client username to client2\n");
		exit(1);
	}
	
	//receive client2 reply
	ret = recv(client2->socket, (void *)&cmd, sizeof(char), 0);
	if(ret==-1) {
		printf("cmd_connect error: error while receiving client2 reply\n");
		exit(1);
	}
	
	switch(cmd) {
		case 'b': {	//client2 is busy
                    //send busy reply to client
                    ret = send(client->socket, (void *)&cmd, sizeof(char), 0);
                    if(ret==-1) {
                        printf("cmd_connect error: error while sending client2 is busy to client\n");
                        exit(1);
                    }
                    //client2->status = 1; //busy
                    client->status = 0; //free
                    client->other = NULL;
                    break;
		}
		case 'r': {	//client2 refused
                    //send refused reply to client
                    ret = send(client->socket, (void *)&cmd, sizeof(char), 0);
                    if(ret==-1) {
                        printf("cmd_connect error: error while sending client2 refused to client\n");
                        exit(1);
                    }
                    client->status = 0; //free
                    client->other = NULL;
                    break;
		}
		case 'a': {	//client2 accepted
					//receive client2 (B) reply (B->S : A, B, Nb)
					ret = recv(client2->socket,(void *)msg1, (MAX_LENGTH*2+NONCE_SIZE), 0);
					if(ret==-1) {
						printf("cmd_connect error: error while receiving client2 first message\n");
						exit(1);
					}
					//retrieve nonce B from msg1
					strncpy(client2->nonce, msg1+MAX_LENGTH*2, NONCE_SIZE);
					//TODO check nonce freshness
					
					regenerate(othersname);
					
					//debug
					printf("otheruser: %s\n", othersname);
					printf("nonce: %s\n", client2->nonce);
						
					//Generate session key Kab
					unsigned char* session_key=generate_session_key(128); //128 tmp per debug, key_size ora inizializzata su enc_init
					
					//TODO invia chiave ad A (crittata con Ka) invia anche porta e IP di B e nonce
					send_second_msg(client, client2, session_key);
					
					//notes: ricorda lunghezza che deve includere ip e porta, A deve controllare nonce
					
					//TODO invia chiave a B (crittata con Kb) (invia porta e IP di A?)
		
                    //send to client (A) that client2 (B) accepted
                    ret = send(client->socket, (void *)&cmd, sizeof(char), 0);
                    if(ret==-1) {
                        printf("cmd_connect error: error while sending client2 accepted to client\n");
                        exit(1);
                    }
                    //invio porta di client2 a client
                    port = htons(client2->UDP_port);
                    ret = send(client->socket, (void *)&port, sizeof(port), 0);
                    if(ret==-1) {
                        printf("cmd_connect error: errore in invio porta client2 a client\n");
                        exit(1);
                    }
                    //invio IP di client2 a client
                    ret = send(client->socket, (void *)&client2->address, sizeof(client2->address), 0);
                    if(ret==-1) {
                        printf("cmd_connect error: errore in invio IP client2 a client\n");
                        exit(1);
                    }

                    client2->status = 1;    //busy
                    client->status = 1;     //busy
                    client->other = client2;
                    client2->other = client;
                    printf("%s connected to %s\n", client->username, client->other->username);
                    break;
		}
		default:  {	//incomprehensible reply, should not happen!
                    cmd = '$';	//whatever char not recognized by "get_from_server()" client side
                    ret = send(client->socket, (void *)&cmd, sizeof(char), 0);
                    if(ret==-1) {
                        printf("cmd_connect error: error while sending incomprehensible reply to client\n");
                        exit(1);
                    }
                    break;
		}
	}
}

//----- quit ------------------
void cmd_quit(int x) {
	if(x==0)
		printf("User %s disconnected from the server\n", client->username);
	else
		printf("User inserted a non valid username!\n");
	close(client->socket);
	FD_CLR(client->socket, &master);
	remove_from_list(client);
}

//----- manage_user ---------
void manage_user(int sd) {
	int     ret;
	char    cmd;
	
	client = find_user_by_sd(sd);
	if(!client) {
		printf("manage_user error: user not found!\n");
		exit(1);
	}
	
	ret = recv(sd, (void *)&cmd, sizeof(cmd), 0);
	if(ret==-1) {
		printf("manage_user error: error while receiving command\n");
		exit(1);
	}
	if(ret==0) {    //client disconnected
		cmd_quit(0);
		return;
	}
	switch(cmd) {
		case 'w': { //who
                    cmd_who(); //send users list
                    break;
		}
		case 'd': {	//disconnect
                    cmd_disconnect();
                    break;
		}
		case 'c': { //connect
                    cmd_connect();
                    break;
		}
		case 'q': { //quit
                    cmd_quit(0);
                    break;
		}
	}
}

//----- add_user --------- add a user to the connected users
//TODO add parameter user_addr in order to make it main-local
int add_user(int sd) {
	int 	ret,
            length;
	char 	cmd;
			
	struct user *new_user = malloc(sizeof(struct user));
	
	length = sizeof(user_addr);
	memset(&user_addr, 0, length);
	getpeername(sd, (struct sockaddr *)&user_addr, (socklen_t *)&length); //find new connected user's address
	
	new_user->address = user_addr.sin_addr.s_addr;
	new_user->socket  = sd;
	new_user->status  = 0;
	new_user->other   = NULL;
	
	//receive: username
	ret = recv(sd, (void *)new_user->username, MAX_LENGTH, 0);
	if(ret==-1) {
		printf("add_user error: error while receiving username\n");
		exit(1);
	}
	//receive: UDP port
	ret = recv(sd, (void *)&new_user->UDP_port, sizeof(int), 0);
	
	if(ret==-1) {
		printf("add_user error: error while receiving UDP port\n");
		exit(1);
	}
	
	//while working in local I should check that users don't connect on the same UDP port
	
	new_user->UDP_port = ntohs(new_user->UDP_port);
	
	if(exist(new_user->username)) {
		cmd = 'e'; //exists
		ret = send(sd, (void *)&cmd, sizeof(cmd), 0);
		if(ret==-1) {
			printf("add_user error: error in reply\n");
			exit(1);
		}
		//close connection with client
		client = new_user;
		cmd_quit(1);
		return 0;
	}
	//connection accepted, need to send something? send @ to be sure
	cmd = '@';
	ret = send(sd, (void *)&cmd, sizeof(cmd), 0);
	if(ret==-1) {
        printf("add_user error: error in sending connection ok\n");
        exit(1);
    }
	regenerate(new_user->username);
	//add connected user to the list
	add_to_list(new_user);
	
	printf("%s is connected\n", new_user->username);
	printf("%s is free\n", new_user->username);
	
	return 1;
}

//=================================

//------- main ------
int main(int num, char* args[]) {   //remember: 1st arg is ./server

	int ret,
        i,
        addrlen,
        new_user_sd;	//new user socket descriptor
	
	//check number of parameters
	if(num!=3) {
		printf("number of parameters is wrong!\n");
		return -1;
	}
	
	//check address
	ret = inet_pton(AF_INET, args[1], &server_addr.sin_addr.s_addr);
	if(ret==0) {
		printf("address not valid!\n");
		exit(1);
	}
	//check port
	ret = atoi(args[2]);
	if(!valid_port(ret)) {
		printf("port not valid! (must be in [1025, 65535])\n");
		exit(1);
	}	
	
	//config
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(ret);
	inet_pton(AF_INET, args[1], &server_addr.sin_addr.s_addr);  //to be redone, server_addr has been initialized after IP checking
	
	printf("Address: %s (Port: %s)\n", args[1], args[2]);
	
	//open TCP socket
	listener = socket(AF_INET, SOCK_STREAM, 0);
	if(listener==-1) {
		printf("error while creating socket\n");
		exit(1);
	}
	
	//--> se voglio mettere opt: ret = setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
	
	//bind
	ret = bind(listener, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if(ret==-1){
		printf("bind error\n");
		exit(1);
	}
	
	//listen
	ret = listen(listener, MAX_CONNECTION);
	if(ret==-1){
		printf("listen error\n");
		exit(1);
	}
	
	//set
	FD_ZERO(&master);
	FD_ZERO(&tmp_fd);
	FD_SET(listener, &master);
	max_fd = listener;					//it is the max_fd at the beginning
	
	while(1) {
		tmp_fd = master;
		ret = select(max_fd+1, &tmp_fd, NULL, NULL, NULL);
		if(ret==-1) {
			printf("select error\n");
			exit(1);
		}
		for(i=0; i<=max_fd; i++) {
			if(FD_ISSET(i, &tmp_fd)) {      //there's a ready descriptor
				if(i==listener) {			//a new user wants to connect
					addrlen = sizeof(user_addr);
					new_user_sd = accept(listener, (struct sockaddr *)&user_addr, (socklen_t *)&addrlen);
					if(new_user_sd==-1) {
						printf("accept error\n");
						exit(1);
					}
					if(add_user(new_user_sd)) { 	//add user to the list
						printf("Connection established with the new user\n");
						FD_SET(new_user_sd, &master);   //add new user's sd to the descriptors to be checked
						if(new_user_sd>max_fd)
							max_fd = new_user_sd;
					}
				}
				else {		//it is a user that is already connected
					manage_user(i);	//manage data received from the user
				}
			}
		}
	}
	return 0;
}
