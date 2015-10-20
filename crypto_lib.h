#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <openssl/evp.h>

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

