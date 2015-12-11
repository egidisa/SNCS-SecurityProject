gcc -o client.exe client.c -lcrypto 
gcc -o server.exe server.c -lcrypto 
./server 127.0.0.1 4444
