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
#define N_CMD	    7	//number of available commands
#define MAX_LENGTH 	10	//max length for the username
#define HELP_MENU "Sono disponibili i seguenti comandi:\n * !help --> mostra l'elenco dei comandi disponibili\n * !who --> mostra l'elenco dei client connessi al server\n * !connect nome_client --> avvia una partita con l'utente nome_client\n * !disconnect --> disconnette il client dall'attuale partita intrapresa con un altro peer\n * !quit --> disconnette il client dal server\n * !show_map --> mostra la mappa di gioco\n * !hit num_cell --> marca la casella num_cell (valido solo quando e' il proprio turno)\n"
//TODO: translate help

//NOTE: client indicates the other user whom I connect to
//=================================

//===== VARIABLES =================
//socket descriptors
int server_sd,
    client_sd;
		
//config variables
struct sockaddr_in  server_addr,
                    my_addr,
                    client_addr;
		
//my data
unsigned char	my_username[MAX_LENGTH];
unsigned long   my_IP;
unsigned short	my_UDP_port;	//from 0 to 65535
//DEchar		    my_mark;
//DEchar 			tmp_pswd[N_CMD];

//other's data
unsigned char	client_username[MAX_LENGTH];
unsigned long	client_IP;
unsigned short	client_UDP_port;	//from 0 to 65535
//DEchar		    client_mark;

//DEint	my_turn = 1;

//TODO modify commands
char commands[N_CMD][MAX_DIM_CMD] = {
	"!help",			//prints the menu
	"!who",				//prints list of connected users
	"!quit",			//disconnects from the server
	"!connect",         //starts protocol (ask to connect to another user)
	"!disconnect",      //disconnect from the connected user
	"!show_map",        //not used, could be used to show history of conversation instead
	"!hit"				//could be used to exchange msgs between users
};

//DEchar	game_grid[9];	//deprecated
//DEint	    empty_cells;	//deprecated

char 	shell;       	// '>' = command shell, '#' = conversation shell
int		show_shell;		//0 = don't print shell character, 1 = print

//set for select
fd_set	master,			//master file descriptor list
        tmp_fd;         //temporary file descriptor list (for select)
int	    max_fd;					

//timer
//DEstruct timeval timer;

//=================================

//===== UTILITY FUNCTIONS =========

//------ valid_port ------
int valid_port(int port) {
	if(port<1024 || port>=65536)
		return 0;
	return 1;
}

//DE------ reset ------ initializes parameters
// void reset() {
	// int i;
	// for(i=0; i<9; i++)
        // game_grid[i] = '-';
	// shell = '#';
	// empty_cells = 9;
	// if(my_mark=='X') 
		// my_turn=1;
	// else 
		// my_turn=0;
	// //aggiorno il timer
	// timer.tv_sec = 60;
	// timer.tv_usec = 0;
	// return;
// }

//------ resolve_command ----- returns index in "commands" array associated to inserted command
int resolve_command(char *cmd) {
	int i;
	for(i=0; i<N_CMD; i++) {
        if(strcmp(cmd, commands[i])==0) // 0 se uguali
			return i;
	}
	return -1;
}

//------ add_padding ------ adds padding '$' to inserted username if < MAX_LENGTH
void add_padding(unsigned char* text) {
	int i;
	for(i=strlen(text); i<MAX_LENGTH; i++) {
		text[i] = '$';
	}
	printf("Padded : %s",text);
}

//------ remove_padding ------ removes padding (in order to print out the username correctly)
void remove_padding(unsigned char* text) {
	int i;
	for(i=0; i<MAX_LENGTH; i++) {
		if(text[i]=='$') {
			text[i] = '\0';
			break;
		}
	}
}

//------ first_msg ------ prepares and returns message with the syntax: (userID, userID, nonce)
unsigned char* first_msg(int msg_size, unsigned char* my_ID, unsigned char* other_ID, unsigned char* nonce){
	unsigned char* msg = calloc (msg_size, sizeof(unsigned char));
	memcpy(msg, my_ID, MAX_LENGTH);
	memcpy(msg+MAX_LENGTH, other_ID, MAX_LENGTH);
	memcpy(msg+MAX_LENGTH*2, nonce, NONCE_SIZE);
	return msg;	
}

void send_first_msg(unsigned char* source, unsigned char* dest) {
	int secret_size;
	int key_size;
	int block_size;
	int ret;	
	int msg_size = MAX_LENGTH*2+NONCE_SIZE; 	//A,B,Na
	unsigned char* secret 	= calloc(secret_size, sizeof(unsigned char));
	unsigned char* msg 			= calloc(msg_size, sizeof(unsigned char));
	//TODO make it global? 
	unsigned char *my_nonce = calloc(NONCE_SIZE, sizeof(unsigned char));
	
	enc_initialization(&secret_size, &key_size, &block_size);
	
	//TODO change, now it returns "fuckdis"
	secret = retrieve_key(4,"secret_file"); 				
	
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

//=================================

//===== FUNCTIONS =================

//------ cmd_who ----- connected users list is on the server, here we send the request
void cmd_who() {
	int ret;
	char cmd = 'w';	//to be sent to the server
	
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
	
	//TODO invia il first message (A->S : A, B, Na)
	send_first_msg(my_username,client_username);
	//server reply is in get_from_server()
	return;
}

//------ cmd_disconnect ------ end=1 game is over (still useful?), end=0 I disconnected, end=2 no need to notify disconnection to the server, already done by other client
void cmd_disconnect(int end) {
	int ret;
	char cmd = 'd';	//to be sent to both server and client
	
	if(end==0) {		//I quit, need to inform client
		ret = sendto(client_sd, (void *)&cmd, sizeof(char), 0, (struct sockaddr *)&client_addr, (socklen_t)sizeof(client_addr)); //informo l'avversario
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
	if(shell=='#') //still connected
		cmd_disconnect(0);
		
	close(client_sd);
	close(server_sd);
	printf("Disconnection from server successfull!\n");
	exit(0);
}

/* //TODO to be redefined, maybe send msgs
//------ cmd_hit -----
 void cmd_hit(int cell) {
	int		ret;
	char	cmd = 'h';

	if(shell=='>') {
		printf("comando valido solo in partita!\n");
		return;
	}
	if(!my_turn) {
		printf("comando valido solo durante il proprio turno!\n");
		return;
	}
	if(game_grid[cell] != '-') {	//cella non vuota
		printf("la cella %d e' gia' occupata!\n", cell+1);
		return;
	}
	
	//da qui in poi significa che e' tutto ok
	game_grid[cell] = my_mark;	//segno sul campo di gioco
	empty_cells --;
	my_turn = 0;				//il turno passa all'avversario
	
	//informo l'avversario che gli mando la coordinata
	ret = sendto(client_sd, (void *)&cmd, sizeof(char), 0, (struct sockaddr *)&client_addr, (socklen_t)sizeof(client_addr));
	if(ret==-1) {
		printf("cmd_hit error: errore nell'invio all'avversario che ho fatto hit\n");
		exit(1);
	}
	//mando coordinate all'avversario
	cell = htonl(cell); //converto in formato di rete
	ret = sendto(client_sd, (void *)&cell, sizeof(int), 0, (struct sockaddr *)&client_addr, (socklen_t)sizeof(client_addr));
	if(ret==-1) {
		printf("cmd_hit error: errore nell'invio delle coordinate all'avversario\n");
		exit(1);
	}
	
	//aggiorno //timer
	//timer.tv_sec = 60;
	//timer.tv_usec = 0;
	
	//controllo se la partita e' finita
	if(check_win()==my_mark) { //ho vinto
		printf("HAI VINTO!!\n");
		cmd_disconnect(1);	//1 per indicare che non ho abbandonato la partita
	}
	if(check_win()!=my_mark && check_win()!=client_mark && empty_cells==0) { //pareggio
		printf("PAREGGIO!!\n");
		cmd_disconnect(1);  //1 per indicare che non ho abbandonato la partita
	}
	
	printf("E' il turno di %s\n", client_username);
	return;
}

//TODO to be redefined, maybe receive msgs
//------ cmd_hit_received ----
void cmd_hit_received(int cell) {
	game_grid[cell] = client_mark;
	empty_cells--;
	my_turn = 1;
	printf("%s ha marcato la cella numero %d\n", client_username, cell+1);
	cmd_show_map(); //stampo il campo di gioco
	printf("E' il tuo turno:\n");
	
	//controllo se la partita e' finita
	if(check_win()==client_mark) { //ho perso
		printf("HAI PERSO!!\n");
		cmd_disconnect(1);	//1 per indicare che non ho abbandonato la partita
	}
	if(check_win()!=my_mark && check_win()!=client_mark && empty_cells==0) { //pareggio
		printf("PAREGGIO!!\n");
		cmd_disconnect(1);  //1 per indicare che non ho abbandonato la partita
	}
	return;
} */
 
//------ get_input ----- discriminates the command inserted by the user (keyboard)
void get_input() {
	char cmd[MAX_DIM_CMD];
	int cell;
	
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
		case 5:	{ //show_map
										//nothing to be done yet
                    break;
		}
		/* case 6:	{ //hit num_cella
                    scanf("%d", &cell);
                    if(cell<1 || cell>9) {
                        printf("numero cella non valido! must be in [1, 9]\n");
                        return;
                    }
                    //controllo fatto in cmd_hit
                    //cmd_hit(cell-1);
                    break;
		} */
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
		printf("connect_to_server error: port not valid! (must be in [1025, 65535])\n");
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
		client_name[MAX_LENGTH] = '\0';
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
		remove_padding(client_name);
		printf("\t%d)  %s (%s)\n", i, client_name, stat);
	}
	return;
}

//------- log_in_server ------
void log_in_server() {
	int 	length,
        ret;
	char	cmd;
	char	UDP[7];
	unsigned short port_tmp;
	
	memset(UDP, 0, 7);
		
	printf("\nInsert username (max %d characters): ", MAX_LENGTH);
	scanf("%s", my_username);
	add_padding(my_username);
	
	do {
		printf("Insert listening UDP port: ");
		scanf("%s", UDP);
		my_UDP_port = atoi(UDP);
		if(!valid_port(my_UDP_port))
			printf("port not valid! (must be in [1025, 65535])\n");
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
	if(cmd=='e') {	//username already existent
		printf("chosen username already existent!\n");
		exit(2);	//exit instead of repeating login procedure TODO: add while loop?
	}
	/*
	if(cmd=='@') //connection ok!
	*/
	
	//print list of connected users right after logging in
	cmd_who();
	
	return;
}
	
//------- start_conversation ------ sets up the conversation between the two client
void start_conversation() {
	int ret;
	
	//my_mark = 'X';
	//client_mark = 'O';
	printf("%s ha accettato la partita\n", client_username);
	printf("Partita avviata con %s\n", client_username);
	//printf("Il tuo simbolo e': %c\n", my_mark);
	printf("E' il tuo turno:\n");
	
	//TODO togli receive, parametri non più in chiaro ma in messaggio crittato
	//ricevo dal server la porta di enemy
	ret = recv(server_sd, (void *)&client_UDP_port, sizeof(client_UDP_port), 0);
	if(ret==-1) {
		printf("start_game error: errore nel ricevere la porta su cui e' in ascolto l'avversario!\n");
		exit(1);
	}

	//ricevo dal server l'IP di enemy
	ret = recv(server_sd, (void *)&client_IP, sizeof(client_IP), 0);
	if(ret==-1) {
		printf("start_game error: errore nel ricevere l'indirizzo dell'avversario!\n");
		exit(1);
	}
	
	//inizializzo parametri avversario
	memset(&client_addr, 0, sizeof(client_addr));
	client_addr.sin_family = AF_INET;
	client_addr.sin_port = client_UDP_port;
	client_addr.sin_addr.s_addr = client_IP;
	
	client_UDP_port = ntohs(client_UDP_port);
	
		
	//aggiorno //timer
	//timer.tv_sec = 60;
	//timer.tv_usec = 0;
	
	//reset();
	
	//TODO inviare messaggio crittato con Kab a B, con nonce
	
	return;
}

//------- manage_request ------- connection request received from client
void manage_request() {
	int 	ret,
        length;
	char	cmd,
        res;
	
	
	memset(client_username, 0, MAX_LENGTH);
	//receive client username
	ret = recv(server_sd, (void *)client_username, MAX_LENGTH, 0);
	if(ret==-1) {
		printf("manage_request error: error while receiving client username!\n");
		exit(1);
	}
	client_username[MAX_LENGTH] = '\0';

	if(shell=='#') { //busy
		//notify server that I'm already busy
		cmd = 'b';
		ret = send(server_sd, (void *)&cmd, sizeof(cmd), 0);
		if(ret==-1) {
			printf("manage_request error: error while sending busy state to server!\n");
			exit(1);
		}
		return;
	}
	
	printf("%s asked to connect with you!\n", client_username);
	
	do {
		scanf("%c", &res);	//metto questo perche' se no stampa 2 volte "accettare..." perche' legge senza aspettare che digiti la prima volta!
		printf("accept request? (y|n): ");	//PERCHE' LA PRIMA VOLTA LEGGE SENZA ASPETTARE CHE DIGITI?? (risolto con scanf sopra)
		fflush(stdin);
		scanf("%c", &res);
	}while(res!='y' && res!='Y' && res!='n' && res!= 'N');	
	
	if(res=='y' || res=='Y') {
		//my_mark = 'O';
		//client_mark = 'X';
		//my_turn = 0;
		printf("Request accepted!\n");
		//printf("Il tuo simbolo e': %s\n", &my_mark);
		//printf("E' il turno di %s\n", client_username);
		cmd = 'a';
		ret = send(server_sd, (void *)&cmd, sizeof(cmd), 0);
		if(ret==-1) {
			printf("manage_request error: error while sending request accepted to server!\n");
			exit(1);
		}
		//TODO send messaggio in cui accetto con nonce (B->S : A, B, Nb)
		send_first_msg(client_username, my_username);
		//reset();
	}
	else {	//request refused
		printf("Connection request refused\n");
		cmd = 'r';
		ret = send(server_sd, (void *)&cmd, sizeof(cmd), 0);
		if(ret==-1) {
			printf("manage_request error: error while sending refused request to server!\n");
			exit(1);
		}
	}
	return;
}
//------- get_from_server ------ server reply received, need to check in response to what
void get_from_server() {
	int 	ret,
        tot_users;
	char	cmd;
	
	ret = recv(server_sd, (void *)&cmd, sizeof(char), 0);
	if(ret==-1) {
		printf("get_from_server error: error while receiving command from server\n");
		exit(1);
	}
		
	if(ret==0) { //server disconnected
		printf("Server disconnected!\n");
		fflush(stdout);
		exit(2);
	}

	switch(cmd) {
		case 'w': {	//who
        				printf("Online users:\n");
                //receive number of connected clients
                ret = recv(server_sd, (void *)&tot_users, sizeof(int), 0);
                if(ret==-1) {
                    printf("get_from_server error: error while receiving number of users!\n");
                    exit(1);
                }
                print_client_list(tot_users);
                break;
		}
		
		case 'c': {	//connect
                //receive client's reply from server
                ret = recv(server_sd, (void *)&cmd, sizeof(char), 0);
                if(ret==-1) {
                	printf("get_from_server error: error while receiving \"accepted\" from server!\n");
                  exit(1);
                }
                switch(cmd) {
                  case 'i': {	//client_username not existent!
                              printf("Impossible to connect to %s: username not existent!\n", client_username);
                  						break;
                  }
                  case 'a': {	//client accepted request
															//TODO ricevi messaggio crittato
															//TODO decritta/scomponi messaggio, retrieve Kab
															//TODO inizializza parametri B (IP e port)
                              start_conversation();
                              break;
                  }
                  case 'r': {	//client refused connection
                              printf("Impossible to connect to %s: user refused!\n", client_username);
                              break;
                  }
                  case 'b': { //client is busy
                              printf("Impossible to connect to%s: user already busy!\n", client_username);
                              break;
                  }
                  default : {	//should not happen :)
                              printf("Server reply incomprehensible!\n");
                              break;
                  }
            		}
                break;
		}
		
		case 'o': {	//connection request
                manage_request();
                break;
		}
		
		default : { //nothing to do
		}
	}
	return;
}

//------- get_from_client ------  client sent something
void get_from_client() {
	int 	ret,
        addrlen,
        cell;
	char	cmd;
	
	addrlen = sizeof(client_addr);
	
	//TODO lo prendo da qui IP/porta di A? o lo recupero prima?
	
	ret = recvfrom(client_sd, (void *)&cmd, sizeof(cmd), 0, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen);
	if(ret==-1) {
		printf("get_from_client error: error while receiving command from client!\n");
		exit(1);
	}
	if(ret==0) {
		printf("get_from_client error: no data received!\n");
		return;
	}
	
	switch(cmd) {
		case 'd' :  {	//disconnect
                 	printf("%s disconnected!\n", client_username);
                  cmd_disconnect(2);
                  break;
		}
		/* case 'h' :  {	//hit TODO: redefine
                 	ret = recvfrom(client_sd, (void *)&cell, sizeof(int), 0, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen);
                  if(ret==-1) {
                  	printf("get_from_client error: errore in ricezione coordinate dal client!\n");
                    exit(1);
                  }
                  cell = ntohl(cell);	//cell e' gia' decrementata di 1!
                  //cmd_hit_received(cell);
                  break;
		} */
		default	:	{
                  break;
		}
	}
	return;
}

//=================================

//------- main ------
int main(int num, char* args[]) {   		//remember: 1st arg is ./client
	int ret,
      i;
//DE	struct timeval *time;

	//check number of parameters
	if(num!=3) {
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
	if(client_sd==-1) {
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
	
	//reset();
	shell = '>';
	//DEtimer.tv_sec = 60;
	//DEtimer.tv_usec = 0;
	show_shell = 1;
	
	while(1) {
		tmp_fd = master;
		
		//DEtime = &timer;
		
		//DEif(shell=='>')
		//DE	time = NULL;
		
		if(show_shell) {
			printf("%c", shell);
			fflush(stdout);
		}
			
		//TODO: remove time from select
		ret = select(max_fd+1, &tmp_fd, NULL, NULL, 0); 	//attivare timer
		if(ret==-1) {
			printf("Select error\n");
			exit(1);
		}
		/* DE
		if(ret==0) { 										//timer expired
			if(my_turn) 
				printf("il tempo e' scaduto! HAI PERSO!\n");
			else
				printf("il tempo dell'avversario e' scaduto! HAI VINTO!\n");
			cmd_disconnect(1);								//fine partita
			continue;
		}
		*/
		for(i=0; i<=max_fd; i++) {
			if(FD_ISSET(i, &tmp_fd)) { 	//there is a ready descriptor
			
				if(i==0) {								//ready descriptor is STDIN
					//read input (command)
					get_input();
					//update timer
					//DEtimer.tv_sec = 60;
					//DEtimer.tv_usec = 0;
				}
				
				if(i==server_sd) {				//ready descriptor is server
					//receive from server
					get_from_server();			
				}
				
				if(i==client_sd) {				//ready descriptor is client
					//receive from peer
					get_from_client();			
					//update timer
					//DEtimer.tv_sec = 60;
					//DEtimer.tv_usec = 0;
				}
			}
		}
	}
	return 1;
}
