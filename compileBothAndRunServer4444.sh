gcc -o client.exe tris_client.c -lcrypto 
gcc -o server.exe tris_server.c -lcrypto 
./server 127.0.0.1 4444
