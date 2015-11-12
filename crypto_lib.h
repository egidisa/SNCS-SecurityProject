#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>

//Encryption
#define AES_128_CBC EVP_aes_128_cbc()
#define AES_256_CBC EVP_aes_256_cbc()
#define AES_128_CBC_BIT_MODE 128
#define AES_256_CBC_BIT_MODE 256

#define NONCE_SIZE 4
#define SESSION_KEY_SIZE 32

char * retrieve_key(const int secret_size, unsigned char* filename) {	
	unsigned char* secret = calloc (secret_size, sizeof(unsigned char));
	int ret;
	FILE* file;
	struct stat info;
	ret = stat(filename, &info);
	if (ret != 0)
		return NULL; 	//file not existing
	file = fopen(filename, "r");
	if(!file){
		printf("Cannot open file");
		return NULL;
	}		//Error opening the file
	ret = fread(secret, 1, secret_size, file);
	if(ret != secret_size){
		printf("Wrong secretsize");
		return NULL;
	}
	fclose(file);
    
	return secret;
}

//Initialization of the encryption context
EVP_CIPHER_CTX* ctx_initialization(unsigned char* key){
	EVP_CIPHER_CTX* ctx = calloc(1, sizeof(EVP_CIPHER_CTX));
	EVP_CIPHER_CTX_init(ctx);
	return ctx;
}

//Initialization for secret,key and block sizes
int enc_initialization(int *secret_size,int *key_size, int *block_size) {
	*secret_size = EVP_CIPHER_key_length(AES_256_CBC);
	*key_size = EVP_CIPHER_key_length(AES_128_CBC);
	*block_size = EVP_CIPHER_block_size(AES_256_CBC);
}

//Generates a char nonce
unsigned char* generate_nonce(){
	//the entropy is the malloc, since it leave the memory uninitialized
	unsigned char* nonce = (unsigned char*)malloc(NONCE_SIZE*sizeof(unsigned char));
	RAND_seed(nonce, NONCE_SIZE);
	RAND_bytes(nonce,NONCE_SIZE);
	return nonce;
	}

//Generates the session key
unsigned char* generate_session_key(int session_key_size){
	//the entropy is the malloc, since it leave the memory uninitialized
	unsigned char* session_key=(unsigned char*)malloc(session_key_size*sizeof(unsigned char));
	RAND_seed(session_key,session_key_size);
	RAND_bytes(session_key,session_key_size);
	return session_key;
}
	
	
//Checks the freshness of the nonce
int isFresh(unsigned char* msg, int key_size, unsigned char* my_nonce){
	int lenght = key_size+NONCE_SIZE;
	int tmp = 0;
	int i = 0;
	for (i=key_size; i<lenght;i++){
		if (msg[i] == my_nonce[i-key_size]) tmp++;
	}
	if (tmp == NONCE_SIZE) return 1; 	//nonce is fresh
	else return 0; 						//nonce is not fresh
}

//Checks the freshness of the nonce
int isFreshNokey(unsigned char* msg, unsigned char* my_nonce){
	int lenght = NONCE_SIZE;
	int tmp = 0;
	int i = 0;
	for (i=0; i<lenght;i++){
		if (msg[i] == my_nonce[i]) tmp++;
	}
	if (tmp == NONCE_SIZE) return 1; 	//nonce is fresh
	else return 0; 						//nonce is not fresh
}

//Encrypts a message
unsigned char* encrypt_msg(void *msg, int block_size, unsigned char* key, int key_len, int* cipher_len){
	int outlen = 0;
	int outlen_tot=0;
	size_t msg_len = strlen(msg)+1; 	//to include \n in the calculations
	unsigned char* cipher_text = calloc (msg_len+block_size, sizeof(unsigned char));
	EVP_CIPHER_CTX* ctx = ctx_initialization(key);
	EVP_EncryptInit(ctx, AES_256_CBC, key, NULL);
	EVP_EncryptUpdate(ctx, cipher_text, &outlen, (unsigned char*)msg, msg_len);
	outlen_tot+=outlen;
	EVP_EncryptFinal(ctx,cipher_text+outlen_tot,&outlen); 
	outlen_tot+= outlen;
	*cipher_len = outlen_tot;
	EVP_CIPHER_CTX_cleanup(ctx);
	return cipher_text;
}

//Decrypts a message
unsigned char* decrypt_msg(void *cipher_text, int block_size, int cipher_size, unsigned char* key){
	EVP_CIPHER_CTX* ctx = ctx_initialization(key);
	unsigned char* plaintext = calloc (cipher_size, sizeof(unsigned char));
	int outlen = 0;
	int outlen_tot=0;
	int res=0;
	EVP_DecryptInit(ctx, AES_256_CBC, key, NULL); 	//not sure on this, need to debug
	EVP_DecryptUpdate(ctx, plaintext, &outlen, cipher_text, cipher_size);
	outlen_tot+=outlen;
	res = EVP_DecryptFinal(ctx,plaintext+outlen_tot,&outlen);
	if (res == 0) 
		fprintf(stderr,"\nError while decrypting the message\n");
	outlen_tot+=outlen;
	EVP_CIPHER_CTX_cleanup(ctx);
	return plaintext;
}

//----------------------
//old
int create_enc_context(EVP_CIPHER_CTX *ctx, int* block_size) {
   int key_size; 			// cryptographic key size
   char* key; 				// cryptographic key 
      
   // Context initialization 
   EVP_CIPHER_CTX_init(ctx);
  
   //Context setup for encryption 
   EVP_EncryptInit(ctx, EVP_des_ecb(), NULL, NULL);
  
   key_size = EVP_CIPHER_CTX_key_length(ctx);
   key = malloc(key_size);
   
   // if (strcmp (retrieve_key(4),"fuckdis")) {
      // printf("Error during key retrieval\n");
      // return 1;
   // }
  
   //Cryptographic key setup
   EVP_CIPHER_CTX_set_key_length(ctx, key_size);
   EVP_EncryptInit(ctx, NULL, (unsigned char*) key, NULL);
    
   //Block size retrieval
   *block_size = EVP_CIPHER_CTX_block_size(ctx);
  
   free(key);
   return 0;
}

