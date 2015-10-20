#include<stdio.h> 			//printf
#include<string.h>	    	//strlen
#include<sys/socket.h>    	//socket
#include<arpa/inet.h> 		//inet_addr
#include<openssl/bn.h>
#include<openssl/dh.h>
#include<unistd.h>
#include<openssl/evp.h>
#include<openssl/rand.h>
#include<openssl/aes.h>

#ifndef ENC_LIB 
#define ENC_LIB 1212

#define NONCE_SIZE 4

#define DES_ECB EVP_des_ecb() //Not advisable to use. Deprecated
#define AES_256_CBC EVP_aes_256_cbc() //Symmetric cipher mode used in the secret
#define AES_128_CBC EVP_aes_128_cbc() //Symmetric cipher mode used in the session

#define AES_128_BIT_MODE 128
#define AES_256_BIT_MODE 256
void prn_hex(unsigned char* msg, int msg_size);

//Initialization of context encryption.
EVP_CIPHER_CTX* encryption_init(unsigned char* key){
	EVP_CIPHER_CTX* ctx=calloc(1,sizeof(EVP_CIPHER_CTX));
	EVP_CIPHER_CTX_init(ctx);
	return ctx;
}



//This function generates the session key between A and B. Its length is session_key_size
unsigned char* generate_session_key(int session_key_size){
	//the entropy is the malloc itslelf
	unsigned char* session_key=(unsigned char*)malloc(session_key_size*sizeof(unsigned char));
	RAND_seed(session_key,session_key_size);
	RAND_bytes(session_key,session_key_size);
	return session_key;
}

//This function initializes the size of: secret, key, and block
void enc_inizialization(int *secret_size, int *key_size, int *block_size){
	*secret_size=EVP_CIPHER_key_length(AES_256_CBC);
	*key_size=EVP_CIPHER_key_length(AES_128_CBC);
	*block_size=EVP_CIPHER_block_size(AES_256_CBC);
}

//Encryption function. 
//It encrypts input generic message msg, with the key knowing its length key_len and block_size.
//If the mode is AES_128_BIT_MODE encrypt with AES 128bit
//If the mode is AES_256_BIT_MODE encrypt with AES 256bit
unsigned char* enc_msg(void *msg, int block_size ,unsigned char * key, int key_len, int* cipher_len, int mode){
	int outlen=0;
	int outlen_tot=0;
	size_t msg_len=strlen(msg)+1;//If I add +1 I consider also the termination character
	unsigned char *cipher_text=calloc(msg_len+block_size, sizeof(unsigned char));
	EVP_CIPHER_CTX* ctx=enc_initialization(key);
	
	if(mode==AES_128_BIT_MODE) EVP_EncryptInit(ctx,AES_128_CBC, key, NULL);
	else if(mode==AES_256_BIT_MODE) EVP_EncryptInit(ctx,AES_256_CBC, key, NULL);
	else {
		printf("Error: choose 128 or 256 in encryption mode\n");
		return NULL;
	}

	EVP_EncryptUpdate(ctx,cipher_text, &outlen, (unsigned char*)msg, msg_len);
	outlen_tot+=outlen;
	EVP_EncryptFinal(ctx, cipher_text+outlen_tot, &outlen);//Add the padding
	outlen_tot+=outlen;
	*cipher_len=outlen_tot;
	EVP_CIPHER_CTX_cleanup(ctx);
	return cipher_text;
}


//This function decrypts the cipher text with the key
//It decrypts input generic message msg, with the key knowing its length key_len and block_size.
//If the mode is AES_128_BIT_MODE decrypt with AES 128bit
//If the mode is AES_256_BIT_MODE decrypt with AES 256bit
unsigned char* dec_msg(void* cipher_text, int block_size, int cipher_size, unsigned char* key, int mode){
	//When the message is decrypted, it is decrypted also the termination characater. Ma porco 
	EVP_CIPHER_CTX* ctx=enc_initialization(key);
	unsigned char* pt=calloc(cipher_size,sizeof(unsigned char));
	int outlen=0;
	int outlen_tot=0;
	int res=0;
	
	if(mode==AES_128_BIT_MODE) EVP_DecryptInit(ctx,EVP_aes_128_cbc(), key, NULL);
	else if(mode==AES_256_BIT_MODE) EVP_DecryptInit(ctx,EVP_aes_256_cbc(), key, NULL);
	else {
		fprintf(stderr,"Error: choose 128 or 256 in decryption mode\n");
		return NULL;
	}
	EVP_DecryptUpdate(ctx, pt, &outlen, cipher_text, cipher_size);
	outlen_tot+=outlen;	
	res=EVP_DecryptFinal(ctx,pt+outlen_tot, &outlen);
	if(res==0){
		fprintf(stderr,"Error in decrypting\n");
		//return NULL;
	}
	outlen_tot+=outlen;
	fprintf(stderr,"After DEC\t");prn_hex(pt,outlen);
	EVP_CIPHER_CTX_cleanup(ctx);
	return pt;
	}

//This function generates the nonce
unsigned char* generate_nonce(){
	unsigned char* nonce=(unsigned char*)malloc(NONCE_SIZE*sizeof(unsigned char));
	RAND_seed(nonce,NONCE_SIZE);
	RAND_bytes(nonce,NONCE_SIZE);
	return nonce;
	}
#endif
