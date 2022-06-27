The server file is named as server.c and the client file is named as client.c

Open three different terminals, 2 for the clients and 1 for the server.

----------------------------------------------------------------------------------------------------------------------------------------------------------------
First you need to create 2048 bits long private and public keys.

For creating private keys run the following command : 
> openssl genrsa -out privateKey_client1.txt 2048
> openssl genrsa -out privateKey_client2.txt 2048

Now for creating public keys run the following command : 
>openssl rsa -in privateKey_client1.txt -pubout -out publicKey_client1.txt
>openssl rsa -in privateKey_client2.txt -pubout -out publicKey_client2.txt

-----------------------------------------------------------------------------------------------------------------------------------------------------------------
First, you need to run the server:

To run the server run the following commands:
> gcc server.c -o server -lcrypto -lssl
> ./server [PORT_NUMBER] publicKey_client1.txt publicKey_client2.txt

You need to provide your own [PORT_NUMBER] to the server

-----------------------------------------------------------------------------------------------------------------------------------------------------------------

To run the client run the following commands:
> gcc client.c -o client -lcrypto -lssl
> ./client [IP_ADDRESS] [PORT_NUMBER] [CLIENT_NUMBER] 

You need to provide your own [IP_ADDRESS], [PORT_NUMBER], [CLIENT_NUMBER] to the client. 

[CLIENT_NUMBER] can be either 1 or 2.

Please make sure that the client you run first should have client number as 1

And similarly the client you run second should have client number 2.

The port number and IP address should be that of the server.


-----------------------------------------------------------------------------------------------------------------------------------------------------------------

Once you have run the server and the clients, you can start sending messages.
The server receives the message from a client, stores the message in a file, encrypts the file and then sends the data to the other client.
The other client decrypts the data and displays it on its terminal.

MAKE SURE TO IMPLEMENT THIS IN THE MAIN ROOT USER DIRECTORY. 


