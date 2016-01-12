//client
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>

#include "crypto_lib.h"

//===== COSTANTS ==================

#define MAX_DIM_CMD	12	//max dimension for commands
#define N_CMD	    5	//number of available commands
#define MAX_LENGTH 	11	//max length for the username
#define HELP_MENU "The available commands are:\n * !help --> Displays the list of the commands\n * !who --> Displays the list of the connected clients\n * !connect client_name --> Forwards a connection request to the specified client_name\n * !disconnect --> Disconnects the client from the current session\n * !quit --> Disconnects the client from the server\n"

//NOTE: client indicates the other user whom I connect to
//=================================

//===== VARIABLES =================
//Encryption variables
	int secret_size;
	int session_key_size;
	int block_size;
	unsigned char* session_key;
	unsigned char* secret_key;

//socket descriptors
int server_sd,
    client_sd;
		
//config variables
struct sockaddr_in  server_addr,
                    my_addr,
                    client_addr;
int addrlen = sizeof(client_addr);
		
//my data
unsigned char	my_username[MAX_LENGTH];
unsigned long   my_IP;
unsigned short	my_UDP_port;		//from 0 to 65535
unsigned char*  my_nonce;

//other's data
unsigned char	client_username[MAX_LENGTH];
unsigned long	client_IP;
unsigned short	client_UDP_port;	//from 0 to 65535
unsigned char*  client_nonce;

char commands[N_CMD][MAX_DIM_CMD] = {
	"!help",			//prints the menu
	"!who",				//prints list of connected users
	"!quit",			//disconnects from the server
	"!connect",         //starts protocol (ask to connect to another user)
	"!disconnect",      //disconnect from the connected user
};

char 	shell;       	// '>' = command shell, '#' = conversation shell
int		show_shell;		//0 = don't print shell character, 1 = print

//set for select
fd_set	master,			//master file descriptor list
        tmp_fd;         //temporary file descriptor list (for select)
int	    max_fd;					

//=================================

//===== UTILITY FUNCTIONS =========

//------ valid_port ------
int valid_port(int port) {
	if(port<1024 || port>9999)
		return 0;
	return 1;
}

//------ resolve_command ----- returns index in "commands" array associated to inserted command
int resolve_command(char *cmd) {
	int i;
	for(i=0; i<N_CMD; i++) {
        if(strcmp(cmd, commands[i])==0) // 0 if equal
			return i;
	}
	return -1;
}

//----- hexadecimal print ------
void print_hex(unsigned char *text){
	int i;
	int size=strlen(text);
	for (i=0; i< size; i++){
		printf("%02x", text[i]);
	}
	printf("\n");
}

//------ add_padding ------ adds padding '$' to inserted username if < MAX_LENGTH
void add_padding(unsigned char* text) {
	int i;
	for(i=strlen(text); i<MAX_LENGTH-1; i++) {
		text[i] = '$';
	}
}

//------ remove_padding ------ removes padding (in order to print out the username correctly)
unsigned char* remove_padding(unsigned char* text) {
	int i;
	unsigned char* tmp = calloc(MAX_LENGTH, sizeof(unsigned char));
	strcpy(tmp, text);
	for(i=0; i<MAX_LENGTH-1; i++) {
		if(tmp[i]=='$') {
			tmp[i] = '\0';
			break;
		}
	}
	return tmp;
}

//------ first_msg ------ prepares and returns message with the syntax: (userID, userID, nonce)
unsigned char* first_msg(int msg_size, unsigned char* my_ID, unsigned char* other_ID, unsigned char* nonce){
	unsigned char* msg = calloc (msg_size, sizeof(unsigned char));
	memcpy(msg, my_ID, MAX_LENGTH);
	memcpy(msg+MAX_LENGTH, other_ID, MAX_LENGTH);
	memcpy(msg+MAX_LENGTH*2, nonce, NONCE_SIZE);
	return msg;	
}

//------ send_first_msg ------ sends the first message
void send_first_msg(unsigned char* source, unsigned char* dest) {
	int ret;	
	int msg_size = MAX_LENGTH*2+NONCE_SIZE; 	//A,B,Na
	unsigned char* msg 			= calloc(msg_size, sizeof(unsigned char));				
	
	printf("\nSending first message...\n");
	
	my_nonce = calloc(NONCE_SIZE, sizeof(unsigned char));
    my_nonce = generate_nonce();
    msg = first_msg(msg_size, source, dest, my_nonce);
	
	//send first msg to server
	ret = send(server_sd, (void *)msg, msg_size, 0);
	if (ret==-1 || ret<MAX_LENGTH)
	{
		printf("cmd_connect error: error while sending first message to the server\n");
		exit(1);
	}
}

//------ send_third_msg ------ sends the third message containing (nonce, nonce) encrypted with the session key
void send_third_msg(unsigned char* own_nonce, unsigned char* other_nonce){
	int ret;	
	int ct_size = 0;
	int msg_size = NONCE_SIZE*2; 	
	unsigned char* msg = calloc (NONCE_SIZE*2, sizeof(unsigned char));
	unsigned char* ct = NULL;
	
	printf("\nSending third message...\n");
	
	//prepare the message
	memcpy(msg, own_nonce, NONCE_SIZE);
	memcpy(msg+NONCE_SIZE, other_nonce, NONCE_SIZE);
	
	//encrypt and send message
	ct=encrypt_msg(msg, block_size, session_key, session_key_size, &ct_size);
	ret = sendto(client_sd, (void *)&ct_size, sizeof(int), 0, (struct sockaddr *)&client_addr, (socklen_t)sizeof(client_addr));
	if(ret==-1) {
		printf("Error: Error while sending third_msg\n");
		exit(1);
	}	
	ret = sendto(client_sd, (void *)ct, ct_size, 0, (struct sockaddr *)&client_addr, (socklen_t)sizeof(client_addr));
	if(ret==-1) {
		printf("Error: Error while sending third_msg\n");
		exit(1);
	}	
	printf("Plaintext : \n");
	print_hex(msg);
	printf("Sending ciphertext: \n");
	print_hex(ct);
}

//------ key_confirmation ------ receives the third message and checks then nonces freshness
int key_confirmation() {
	int ct_size=0;
	int ret=0;
	ret = recvfrom(client_sd, (void *)&ct_size, sizeof(int), 0, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen);
	if (ret==-1){
		printf("Error while receiving ct_size");
	}
	unsigned char ct[ct_size];
	ret = recvfrom(client_sd, (void *)ct, ct_size, 0, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen);
	
	if(ret==-1) {
		printf("Error while receiving ciphertext\n");
	exit(1);
	}
	printf("\nCiphertext received: \n");
	print_hex(ct);
	//decrypt
	unsigned char* pt = NULL;
	pt=decrypt_msg(ct,block_size, ct_size, session_key);
	printf("Plaintext : \n");
	print_hex(pt);
	
	//check freshness
	unsigned char* my_new_nonce = calloc(NONCE_SIZE, sizeof(unsigned char));
	unsigned char* other_nonce = calloc(NONCE_SIZE, sizeof(unsigned char));
	
	memcpy(other_nonce, pt, NONCE_SIZE);
	printf("OtherNonce: \n");	
	print_hex(other_nonce);
	memcpy(my_new_nonce, pt+NONCE_SIZE, NONCE_SIZE);
	printf("MyNonce: \n");
	print_hex(my_new_nonce);
	//Crypto memcmp to avoid timing attack
	int memcmp_res = CRYPTO_memcmp(my_nonce, my_new_nonce, NONCE_SIZE);
	if(memcmp_res!=0){
		printf("Nonce not fresh\n");
		return 0;
	}
	memcmp_res = CRYPTO_memcmp(client_nonce, other_nonce, NONCE_SIZE);
	if(memcmp_res!=0){
		printf("Other's nonce not fresh\n");
		return 0;
	}

	return 1;
}
//=================================

//===== FUNCTIONS =================

//------ cmd_who ----- connected users list is on the server, here we send the request
void cmd_who() {
	int ret;
	char cmd = 'w';		//to be sent to the server
	
	ret = send(server_sd, (void *)&cmd, sizeof(char), 0);
	if(ret==-1) {
		printf("cmd_who error: error while sending command\n");
		exit(1);
	}
	
	//receive is in get_from_server()
	return;
}

//------ cmd_connect ------ send request to connect
void cmd_connect() {
	int		ret,
            length;
	char 	cmd = 'c';
	
	//check if the inserted username is the same as the active client
	if( strcmp (my_username, client_username)==0) { //connecting to myself
		printf("You can't chat with yourself!\nWell actually you can but it's kind of sad isn't it?");
		return;
	}
	
	//check if I'm already connected to someone else
	if(shell=='#') {
		cmd = 'b';
		ret = send(server_sd, (void *)&cmd, sizeof(cmd), 0);
		if (ret==-1)
		{
			printf("cmd_connect error: error while sending busy command to server\n");
			exit(1);
		}
		return;
	}
	
	//send command identifying "connect" to the server
	ret = send(server_sd, (void *)&cmd, sizeof(cmd), 0);
	if (ret==-1)
	{
		printf("cmd_connect error: error while sending command to the server\n");
		exit(1);
	}
	
	//send first message (A->S : A, B, Na)
	send_first_msg(my_username,client_username);
	//server reply is in get_from_server()
	return;
}
//TODO rivedere cmd disconnect
//------ cmd_disconnect ------ end=1 game is over (still useful?), end=0 I disconnected, end=2 no need to notify disconnection to the server, already done by other client
void cmd_disconnect(int end) {
	int ret;
	char cmd = 'd';	//to be sent to both server and client
	
	if(end==0) {		//I quit, need to inform client
		ret = sendto(client_sd, (void *)&cmd, sizeof(char), 0, (struct sockaddr *)&client_addr, (socklen_t)sizeof(client_addr)); 
		if(ret==-1) {
			printf("cmd_disconnect error: error while notifying disconnection do client\n");
			exit(1);
		}
		printf("You disconnected succesfully from %s!\n", client_username);
	}
	
	if(end==1) //game is over
		printf("Disconnecting from %s...\n", client_username);
	
	if(end!=2) {
		ret = send(server_sd, (void *)&cmd, sizeof(char), 0); //notify server
		if(ret==-1) {
			printf("cmd_disconnect error: error while notifying server\n");
			exit(1);
		}
	}
	
	shell = '>';
	
	//reset client parameters
	memset(&client_addr, 0, sizeof(client_addr));
	
	return;
}

//------ cmd_quit ------ 
void cmd_quit() {
	int 	ret;
	char	cmd;
	
	cmd = 'q';
	
	ret = send(server_sd, (void *)&cmd, sizeof(char), 0);
	if(ret==-1) {
		printf("cmd_quit error: error while sending\n");
		exit(1);
	}
	if(shell=='#') 	//still connected
		cmd_disconnect(0);
		
	close(client_sd);
	close(server_sd);
	printf("Disconnection from server successfull!\n");
	exit(0);
}
 
//------ get_input ----- discriminates the command inserted by the user (keyboard)
void get_input() {
	char cmd[MAX_DIM_CMD];

	scanf("%s", cmd);
	fflush(stdin);		//to empty buffer from eventual remaining characters
	switch(resolve_command(cmd)) {
		case 0:	{ //help
                    printf("%s", HELP_MENU);
                    break;
		}
		case 1:	{ //who
                    cmd_who();
                    break;
		}
		case 2:	{ //quit
                    cmd_quit();
                    break;
		}
		case 3:	{ //connect username
                    if(shell=='#')
                        printf("You are already connected!\n");
                    else {
                        scanf("%s", client_username);
												add_padding(client_username);
												//other checking done in cmd_connect()..
                        cmd_connect();
                    }
                    break;
		}
		case 4:	{ //disconnect
                    //check if I'm truly connected
                    if(shell=='#')
                        cmd_disconnect(0);
                    else
                        printf("you're not connected to anyone!\n");
                    break;
		}
		default: printf("non valid command! digit \"!help\" to check the commands list.\n");
	}
	return;
}

//------ connect_to_server -----	connect to the specified server
void connect_to_server(char *addr, int port) {
	int ret;
	memset(&server_addr, 0, sizeof(server_addr));
	
	//check address
	ret = inet_pton(AF_INET, addr, &server_addr.sin_addr.s_addr);
	if(ret==0) {
		printf("connect_to_server error: address not valid!\n");
		exit(1);
	}
	//check port
	if(!valid_port(port)) {
		printf("connect_to_server error: port not valid! (must be in [1025, 9999])\n");
		exit(1);
	}	
	server_addr.sin_port = htons(port);
	server_sd = socket(AF_INET, SOCK_STREAM, 0);
	if(server_sd==-1) {
		printf("connect_to_server error: error while creating socket\n");
		exit(1);
	}
	server_addr.sin_family = AF_INET;
	ret = connect(server_sd, (struct sockaddr *)&server_addr, sizeof(server_addr));
	if(ret==-1) {
		printf("connect_to_server error: error while connecting\n");
		exit(1);
	}
	return;
}

//------- print_client_list ----
void print_client_list(int tot) {
	int 	i,
        ret,
        length,
        status;		//remember 0=free 1=busy
	unsigned char	client_name[MAX_LENGTH],
								stat[5];
	
	for(i=0; i<tot; i++) {
		memset(client_name, 0, MAX_LENGTH);	//reset client_name
		
		//receive client name
		ret = recv(server_sd, (void *)client_name, MAX_LENGTH, 0);
		if(ret==-1) {
			printf("get_from_server error: error while receiving client name from server!\n");
			exit(1);
		}
		client_name[MAX_LENGTH-1] = '\0';
		
		//receive client status
		ret = recv(server_sd, (void *)&status, sizeof(int), 0);
		if(ret==-1) {
			printf("get_from_server error: error while receiving client status from server!\n");
			exit(1);
		}
		
		//prepare text to be shown
		if(status==0)
			strncpy(stat, "free", sizeof(stat)-1);
		else
			strncpy(stat, "busy", sizeof(stat)-1);
		stat[4] = '\0';
		printf("\t%d)  %s (%s)\n", i, remove_padding(client_name), stat);
	}
	return;
}

//------- log_in_server ------
void log_in_server() {
	int 	length,
        ret;
	char	cmd;
	char	UDP[5];
	unsigned short port_tmp;
	unsigned char filename[MAX_LENGTH];
	
	memset(UDP, 0, 5);
		
	printf("\nInsert username (max %d characters): ", MAX_LENGTH-1);
	scanf("%s", my_username);
	strcpy(filename, my_username);
	add_padding(my_username);
	
	do {
		printf("Insert listening UDP port: ");
		scanf("%s", UDP);
		my_UDP_port = atoi(UDP);
		if(!valid_port(my_UDP_port))
			printf("port not valid! (must be in [1025, 9999])\n");
	}while(!valid_port(my_UDP_port));
	
	//send (to server): username
	ret = send(server_sd, (void *)my_username, MAX_LENGTH, 0);
	if(ret==-1 || ret<MAX_LENGTH) {
		printf("log_in_server error: error in sending username\n");
		exit(1);
	}
	
	//send (to server): UDP port
	port_tmp = htons(my_UDP_port);
	ret = send(server_sd, (void *)&port_tmp, sizeof(port_tmp), 0);
	if(ret==-1 || ret<sizeof(port_tmp)) {
		printf("log_in_server error: error in sending UDP port\n");
		exit(1);
	}
	
	//check server reply
	ret = recv(server_sd, (void *)&cmd, sizeof(cmd), 0);
	if(ret==-1) {
		printf("log_in_server error: error in reception\n");
		exit(1);
	}
	if(cmd=='e') {	//username already exists
		printf("chosen username already existent!\n");
		exit(2);	//exit instead of repeating login procedure
	}
	
	//HYP: users are already registered, filename is username
	secret_key = calloc(secret_size, sizeof(unsigned char));
	secret_key = retrieve_key(secret_size, filename);
	
	//print list of connected users right after logging in
	cmd_who();
	
	return;
}
	
//------- start_conversation ------ sets up the conversation between the two client
void start_conversation() {
	int ret;
	int msglen;

	printf("%s accepted your request\n", remove_padding(client_username));
	//receive server reply (S->A : (Kab, B, Na, Nb)Ka)
	ret = recv(server_sd, (void *)&msglen, sizeof(int), 0);
	if(ret==-1) {
		printf("start_game error: error receiving server response (msglen)\n");
		exit(1);
	}	
	unsigned char* ct = calloc(msglen, sizeof(unsigned char*));
	ret = recv(server_sd, (void *)ct, msglen, 0);
	if(ret==-1) {
		printf("start_game error: error receiving server response (ct)\n");
		exit(1);
	}

	
	printf("\nSecond message received!\n");
	
	unsigned char* pt = NULL;
	pt=decrypt_msg(ct,block_size, msglen, secret_key);
	printf("Ciphertext: \n");
	print_hex(ct);
	printf("Plaintext: \n");
	print_hex(pt);
 	//retrieve session_key, port, ip, Na, Nb
	session_key = calloc(session_key_size, sizeof(unsigned char*));
	unsigned char* address = calloc(8, sizeof(unsigned char*));
	unsigned char* port = calloc(4, sizeof(unsigned char*));
	unsigned char* my_new_nonce = calloc(NONCE_SIZE, sizeof(unsigned char*));
	client_nonce = calloc(NONCE_SIZE, sizeof(unsigned char*));
	
	memcpy(session_key, pt, session_key_size);
	printf("Session_key: \n");
	print_hex(session_key);
	memcpy(port, pt+session_key_size, 4);
	port[4] = '\0';
	printf("port %s\n", port);
	memcpy(address, pt+session_key_size+4, 8);
	address[8]= '\0';
	//printf("address %s\n", address);
	
	memcpy(my_new_nonce, pt+session_key_size+4+8, NONCE_SIZE);
	printf("MyNonce: \n");
	print_hex(my_new_nonce);
	memcpy(client_nonce, pt+session_key_size+4+8+NONCE_SIZE, NONCE_SIZE);
	printf("OtherNonce: \n");
	print_hex(client_nonce);
	//check my_new_nonce freshness with secure memcmp
	if (CRYPTO_memcmp(my_nonce, my_new_nonce, NONCE_SIZE)== 0 ) printf("Nonce is fresh!\n");
	else printf("Nonce not fresh\n"); 
	//TODO disconnect
	
	memset(&client_addr, 0, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = htons(atoi(port));
	client_addr.sin_addr.s_addr = atoi(address);

	client_UDP_port = atoi(port);
	printf("my_port: %d\nclient_port: %d\n", my_UDP_port, client_UDP_port);
	
	//send command for third message
	char cmd = 't';
		ret = sendto(client_sd, (void *)&cmd, sizeof(char), 0, (struct sockaddr *)&client_addr, (socklen_t)sizeof(client_addr));
		if(ret==-1) {
			printf("Error while sending t command\n");
			exit(1);
		}
	//send A->B : (Na, Nb)Kab
	send_third_msg(my_new_nonce, client_nonce);
	
	//receive other's reply
	if(key_confirmation()) {
		printf("Protocol succesfull\n");
	}
	else{
		printf("Protocol aborted\n");
	}
	
	return;
}

//------- manage_request ------- connection request received from client
void manage_request() {
	int ret, length;
	char cmd, res;
	int msglen;
	memset(client_username, 0, MAX_LENGTH);
	//receive client username
	ret = recv(server_sd, (void *)client_username, MAX_LENGTH, 0);
	if(ret==-1) { 					//error while receiving username
		printf("manage_request error: error while receiving client username!\n");
		exit(1);
	}
	client_username[MAX_LENGTH-1] = '\0';
	if(shell=='#') { 				//this client is busy, notify server
		cmd = 'b';
		ret = send(server_sd, (void *)&cmd, sizeof(cmd), 0);
		if(ret==-1) {				//error sending notification to server
			printf("manage_request error: error while sending busy state to server!\n");
			exit(1);
		}
		return;
	}
	printf("%s asked to connect with you!\n", remove_padding(client_username));
	do {
		scanf("%c", &res);			//to avoid double print
		printf("accept request? (y|n): ");
		fflush(stdin);
		scanf("%c", &res);
	}while(res!='y' && res!='Y' && res!='n' && res!= 'N');		
	if(res=='y' || res=='Y') {
		printf("Request accepted!\n");
		cmd = 'a';
		ret = send(server_sd, (void *)&cmd, sizeof(cmd), 0);
		if(ret==-1) {
			printf("manage_request error: error while sending request accepted to server!\n");
			exit(1);
		}
		//send first message (B->S : A, B, Nb)
		send_first_msg(client_username, my_username);
	}
	else {							//client refused the request
		printf("Connection request refused\n");
		cmd = 'r';
		ret = send(server_sd, (void *)&cmd, sizeof(cmd), 0);
		if(ret==-1) {
			printf("manage_request error: error while sending refused request to server!\n");
			exit(1);
		}
	}
	
	//receive second message with Kab (S->B : (Kab, portIPA, Nb, Na)kb)
	ret = recv(server_sd, (void *)&msglen, sizeof(int), 0);
	if(ret==-1) {					//error while receiving the other client's port
		printf("error: error while receiving the other client's port\n");
		exit(1);
	}	
	
	unsigned char* ct = calloc(msglen, sizeof(unsigned char*));
	//receive msg2
	ret = recv(server_sd, (void *)ct, msglen, 0);
	if(ret==-1) {					//error while receiving the other client's port
		printf("error:error while receiving the other client's port\n");
		exit(1);
	}

	printf("\nSecond message received!\n");
	
	unsigned char* pt = NULL;
	pt=decrypt_msg(ct,block_size ,msglen,secret_key);
	
	//debug prints
	printf("cipher text: \n");
	print_hex(ct);
	printf("plain text: \n");
	print_hex(pt);
	
	//retrieve session_key, port, ip, Na, Nb from plaintext
	session_key = calloc(session_key_size, sizeof(unsigned char*));
	unsigned char* address = calloc(8, sizeof(unsigned char*));
	unsigned char* port = calloc(4, sizeof(unsigned char*));
	unsigned char* my_new_nonce = calloc(NONCE_SIZE, sizeof(unsigned char*));
	client_nonce = calloc(NONCE_SIZE, sizeof(unsigned char*));
	
	memcpy(session_key, pt, session_key_size);
	printf("Session_key: \n");
	print_hex(session_key);
	memcpy(port, pt+session_key_size, 4);
	printf("port %s\n", port);
	printf("port (atoi) %d\n", atoi(port));
	
	memcpy(address, pt+session_key_size+4, 8);
	address[8]= '\0';	
	//printf("address %s\n", address);
	
	memcpy(my_new_nonce, pt+session_key_size+4+8, NONCE_SIZE);
	printf("MyNonce: \n");
	print_hex(my_new_nonce);
	memcpy(client_nonce, pt+session_key_size+4+8+NONCE_SIZE, NONCE_SIZE);
	printf("OtherNonce: \n");
	print_hex(client_nonce);	
	

	//check my_new_nonce freshness with secure memcmp
	if (CRYPTO_memcmp(my_nonce, my_new_nonce, NONCE_SIZE)== 0 ) printf("Nonce is fresh!\n");
	else printf("Nonce not fresh\n"); 
	//TODO disconnect
	
	return;
}
//------- get_from_server ------ server reply received, check for case
void get_from_server() {
	int ret, tot_users;
	char	cmd;
	//receive from server
	ret = recv(server_sd, (void *)&cmd, sizeof(char), 0);
	if(ret==-1) {					//error receiving command
		printf("get_from_server error: error while receiving command from server\n");
		exit(1);
	}
	if(ret==0) { 					//server disconnected
		printf("Server disconnected!\n");
		fflush(stdout);
		exit(2);
	}
	switch(cmd) {
		case 'w': {					//who - returns list of online users
				printf("Online users:\n");
                //receive number of connected clients
                ret = recv(server_sd, (void *)&tot_users, sizeof(int), 0);
                if(ret==-1) {		//error
                    printf("get_from_server error: error while receiving number of users!\n");
                    exit(1);
                }
                print_client_list(tot_users);
                break;
		}
		case 'c': {					//connect
                //receive client's reply from server
                ret = recv(server_sd, (void *)&cmd, sizeof(char), 0);
                if(ret==-1) {		//error in receiving from server
                	printf("get_from_server error: error while receiving \"accepted\" from server!\n");
                  exit(1);
                }
                switch(cmd) {
                  case 'i': {		//client_username does not exist
                              printf("Impossible to connect to %s: username not existent!\n", client_username);
                  						break;
                  }
                  case 'a': {		//client accepted request
                              start_conversation();
                              break;
                  }
                  case 'r': {		//client refused connection
                              printf("Impossible to connect to %s: user refused!\n", client_username);
                              break;
                  }
                  case 'b': {		//client is busy
                              printf("Impossible to connect to%s: user already busy!\n", client_username);
                              break;
                  }
                  default : {		//default option, should not happen anyways
                              printf("Server reply incomprehensible!\n");
                              break;
                  }
            		}
                break;
		}	
		case 'o': {					//connection request
                manage_request();
                break;
		}
		default : { 
		}
	}
	return;
}

//------- get_from_client ------  client sent something
void get_from_client() {
	int 	ret;
	char	cmd;
	
	//receive from client
	ret = recvfrom(client_sd, (void *)&cmd, sizeof(cmd), 0, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen);
	if(ret==-1) {					// error while receiving command from client
		printf("get_from_client error: error while receiving command from client!\n");
		exit(1);
	}
	if(ret==0) {					//no data received
		printf("get_from_client error: no data received!\n");
		return;
	}
	
	switch(cmd) {
		case 'd' :  {				//disconnect
			printf("%s disconnected!\n", client_username);
			cmd_disconnect(2);
			break;
		}
		case 't' : {				//third message
			if(key_confirmation())
				//reply with Nb, Na
				send_third_msg(my_nonce, client_nonce);
			else
				printf("Key not confirmed\n");
			break;
		} 
		default	:	{
			break;
		}
	}
	return;
}

//=================================

//------- main ------
int main(int num, char* args[]) {   		//remember: 1st arg is ./client
	//encryption context initialization
	enc_initialization(&secret_size, &session_key_size, &block_size);
	int ret,
      i;
	  
	if(num!=3) {					//check number of parameters
		printf("Wrong number of parameter!\n");
		return -1;
	}
	
	//Server connection
	connect_to_server(args[1], atoi(args[2]));
	printf("Connection with the server successfully established %s on port (port %s)", args[1], args[2]);
	
	//Display help
	printf("\n%s\n", HELP_MENU);
	
	//Server login
	log_in_server();
	
	//Opens UDP socket
	client_sd = socket(AF_INET, SOCK_DGRAM, 0);
	if(client_sd==-1) {				//error while creating UDP socket
		printf("main: error while creating UDP socket\n");
		exit(1);
	}
	
	//INET configuration
	memset(&my_addr, 0, sizeof(my_addr));
	my_addr.sin_family = AF_INET;
	my_addr.sin_port = htons(my_UDP_port);
	my_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	
	//Client bind
	ret = bind(client_sd, (struct sockaddr*)&my_addr, sizeof(my_addr));
	if(ret==-1){
		printf("error in bind\n");
		exit(1);
	}
	
	//Set
	FD_ZERO(&master);
	FD_SET(server_sd, &master);
	FD_SET(client_sd, &master);
	FD_SET(0, &master);  //std in
	max_fd = (server_sd>client_sd)?server_sd:client_sd;

	shell = '>';
	show_shell = 1;
	
	while(1) {
		tmp_fd = master;

		if(show_shell) {
			printf("%c", shell);
			fflush(stdout);
		}
		
		ret = select(max_fd+1, &tmp_fd, NULL, NULL, 0); 
		if(ret==-1) {
			printf("Select error\n");
			exit(1);
		}

		for(i=0; i<=max_fd; i++) {
			if(FD_ISSET(i, &tmp_fd)) { 			//there is a ready descriptor
			
				if(i==0) {						//ready descriptor is STDIN
					//read input (command)
					get_input();
				}
				
				if(i==server_sd) {				//ready descriptor is server
					//receive from server
					get_from_server();			
				}
				
				if(i==client_sd) {				//ready descriptor is client
					//receive from peer
					printf("get from client\n");
					get_from_client();	
				}
			}
		}
	}
	return 1;
}
