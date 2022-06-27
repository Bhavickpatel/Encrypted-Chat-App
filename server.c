
#include <stdio.h> 
#include <string.h>   
#include <stdlib.h> 
#include <errno.h> 
#include <dirent.h>
#include <unistd.h>    
#include <arpa/inet.h>    
#include <sys/types.h> 
#include <sys/socket.h> 
#include <netinet/in.h> 
#include <sys/time.h> 
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <pthread.h>
#include <sys/stat.h>
#include <fcntl.h>

// defining max_size for character arrays
#define MAX_SIZE 2048
#define clientNum 5

//creating an array of threads
pthread_t clientThreads[clientNum+1];

//Array for storing client socket fds
int clientSockId[clientNum+1];

//At any time the number of clients served is given by clientCount
int clientCount=0;

//Struct formed for thread function to send client-sockfd and index
//in clientThreads[] where the respective clientthread id is stored
struct threadargs{
    int clientsock;
    int i;
};

//Thread function
void* communication(void *arguments){
    struct threadargs *args1 = (struct threadargs*)arguments;
    char buffer[MAX_SIZE]; //to transfer data between client and server
    bzero(buffer,sizeof(buffer));
    //Getting data from Client 
    printf("New Client Connected , thread id : %ld\n",pthread_self());
    while(1){
        bzero(buffer,sizeof(buffer));
        //reading from client
        recv(args1->clientsock,buffer,sizeof(buffer),0);
        //read(args1->clientsock,&buffer,sizeof(buffer));
        //Comparing buffer with "quit"
        if(strcmp(buffer,"quit\n")==0){
            printf("Thread [%ld] says %s",pthread_self(),buffer);
            break;
        }
        //Printing thread_id and filename requested by client
        printf("Thread [%ld] says %s\n",pthread_self(),buffer);

        // Defining name[] for storing the filename
        char name[sizeof(buffer)];
        strcpy(name,buffer);
        bzero(buffer,sizeof(buffer));

        //Using stat() function to get status info about a specific file
        struct stat sb;
        if(stat(name,&sb)==-1){
            //perror("stat");
            bzero(buffer,sizeof(buffer));
            //Notifying client that no such file exists for the given filename
            strcpy(buffer,"No Such File\n");
            send(args1->clientsock,buffer,sizeof(buffer),0);
            continue;
        }
        else{
            //Notifying client that file exists for the given filename
            bzero(buffer,sizeof(buffer));
            strcpy(buffer,"File Exists\n");
            send(args1->clientsock,buffer,sizeof(buffer),0);
        }
        
        //Opening(Reading) the binary file requested
        FILE *p;
        p = fopen(name,"rb");

        //Implementing the transfer of file
        if(p!=NULL){
            char* path = realpath(name,NULL);
            if(path==NULL){
                printf("cannot find file with name[%s]\n",name);
            }  

            //getting struct stat for the file from stat library
            // and then sending the struct to client 
            struct stat ret;
            stat(path,&ret);
            send(args1->clientsock,(const struct stat *)&ret,sizeof(ret),0);

            int len=sizeof(buffer);
            int bsize; //block size
            while((bsize = fread(buffer,sizeof(char),len,p))>0){
                if(send(args1->clientsock,buffer,bsize,0)<0){
                    break;
                }
            }

            //Closing the file 
            fclose(p);
        }
        
    }
    printf("Thread %ld ended connection\n",pthread_self());
    //Closing the socket
    close(args1->clientsock);
    clientCount--; //reducing the number of clients server
    clientThreads[args1->i] = 0; //Initialising clintThreads[] array
    clientSockId[args1->i] = 0; //Initialising sockid[] array
    free(args1); //freeing the struct used for thread
    pthread_exit(NULL); //Exiting the thread and returning the status 
}   


int main(int argc,char *argv[]){

    //initialization of clientThreads[] and clientSocid[] arrays
    memset(clientThreads,0,sizeof(clientThreads));
    memset(clientSockId,0,sizeof(clientSockId));

    if(argc != 3){
        printf("Enter IP address and port as arguement\n");
        return 1;
    }  
    int port = atoi(argv[2]); //port number
    char *ip = argv[1];     //Hosting the server on localhost, since clients and server are suppose to run on same computer
    
    int server_sock,new_sock;
    struct sockaddr_in server_addr , client_addr;
    socklen_t addr_size;
    char *buffer[MAX_SIZE];

    //creating server socket
    server_sock = socket(AF_INET,SOCK_STREAM,0);
    //SOCK_STREAM for TCP
    if(server_sock < 0){
        perror("[-] Socket Error\n");
        exit(1);
    }
    printf("[+] TCP socket created\n");

    //clearing space
    memset(&server_addr, '\0' , sizeof(server_addr));

    //setting up values for socket
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = port;
    server_addr.sin_addr.s_addr = inet_addr(ip);

    //Binding the Server process to port
    int n = bind(server_sock, (struct sockaddr*)&server_addr , sizeof(server_addr) );
    if(n < 0){
        perror("[-] Bind Error\n");
        exit(1);
    }
    printf("Server %s binded on port : %d\n",argv[1],port);

    //Listening for client
    if (listen(server_sock, clientNum) == 0)
        printf("[+] Listening\n");
    else
        printf("Error while listening\n");

    char buff[MAX_SIZE];    
    while(1){
        addr_size = sizeof(client_addr);
        // Extract the first connection in the queue
        new_sock = accept(server_sock,(struct sockaddr*)&client_addr,&addr_size);
        //Case-1 Is Server is serving 5 clients and can't serve any more clients 
        if(clientCount >= clientNum){
            bzero(buff,sizeof(buff));
            //Sending "500" string as to say server has reached Max Limit
            strcpy(buff,"500");
            send(new_sock,&buff,sizeof(buff),0);
            printf("[-] Max Client Limit Reached\n");
            close(new_sock);
        }
        //Case-2 Server can serve the client
        else{
            bzero(buff,sizeof(buff));
            //Sending "200" as to say client can be served
            strncpy(buff,"200",sizeof("200"));
            send(new_sock,&buff,sizeof(buff),0);
            //for loop is used to fill clientThread[] and clientSockid[] wherever
            //it is possible(value is '0')(done once and then create thread)
            for(int i=1; i<=clientNum; i++){
                if(clientThreads[i] == 0){
                    struct threadargs *args = malloc(sizeof(struct threadargs));
                    args->i=i;
                    args->clientsock=new_sock;
                    clientSockId[i] = new_sock;
                    clientCount++; //Incrementing the clients-served count
                    //Creating the thread and calling 'communication' function
                    pthread_create(&clientThreads[i],NULL,communication,(void *)args);
                    break;
                }
            }
        }
    }
    return 0;
}

