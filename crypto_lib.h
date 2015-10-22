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

char * retrieve_key() { 		//TODO: reads password from file
 /*int ret;
   FILE* file;
   struct stat info;
   
   ret = stat("key", &info);
   if (ret != 0)
     return 1;
   
   file = fopen("key", "r");
   if(!file)
      return 1;
   
   ret = fread(key, 1, key_size, file);
   if(ret < key_size)
     return 1;
   
   fclose(file);
   
   return 0;*/
   
   return "fuckdis";
  
}
//Initialization for secret,key and block sizes
int enc_initialization(int *secret_size,int *key_size, int *block_size) {
	*secret_size = EVP_CIPHER_key_length(AES_256_CBC);
	*key_size = EVP_CIPHER_key_length(AES_128_CBC);
	*block_size = EVP_CIPHER_block_size(AES_256_CBC);
}

//Generates a char nonce
unsigned char* generate_nonce(){
	unsigned char* nonce = (unsigned char*)malloc(NONCE_SIZE*sizeof(unsigned char));
	RAND_seed(nonce, NONCE_SIZE);
	RAND_bytes(nonce,NONCE_SIZE);
}

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
   
   if (strcmp (retrieve_key(),"fuckdis")) {
      printf("Error during key retrieval\n");
      return 1;
   }
  
   //Cryptographic key setup
   EVP_CIPHER_CTX_set_key_length(ctx, key_size);
   EVP_EncryptInit(ctx, NULL, (unsigned char*) key, NULL);
    
   //Block size retrieval
   *block_size = EVP_CIPHER_CTX_block_size(ctx);
  
   free(key);
   return 0;
}

