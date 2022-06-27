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


int main(int argc ,char *argv[]){
    if(argc != 3){
        printf("Enter IP address and port as argument\n");
        return 1;
    }
    
    char *ip = argv[1]; //localhost , since the client and server are suppose to run on same computer
    int port = atoi(argv[2]); //port number

    int sock;
    struct sockaddr_in addr;
    socklen_t addr_size;
    char buffer[MAX_SIZE]; //to transfer data between client and server   
    

    //Creating a socket
    sock = socket(AF_INET,SOCK_STREAM,0);
    if(sock < 0) {
        perror("[-] Socket error");
        exit(1);
    }
    printf("[+] TCP server socket created \n");

    memset(&addr, '\0', sizeof(addr)); //clearing space

    //configuring socket
    addr.sin_family = AF_INET;
    addr.sin_port = port;
    addr.sin_addr.s_addr = inet_addr(ip);

    //connecting with the server using the socket 
    int res = connect(sock , (struct sockaddr*)&addr , sizeof(addr));
    if(res < 0){
        perror("[-] Connection Error");
        exit(1);
    }
    printf("[+] Connected to Server\n");
    //Reading the response from the server regarding the establishment of the connection
    bzero(buffer,sizeof(buffer));
    read(sock,buffer,sizeof(buffer));
    int response = atoi(buffer);
    //500 is the response given by server is unable to establish the connection
    //200 is the response given by the server if connection is established
    if(response == 500){
        printf("Max Client Limit Reached,Try Again later\n");
        close(sock);
        return 1;
    }
    
    // Defining name[] for storing the filename
    char name[MAX_SIZE];
    bzero(name,sizeof(name));
    name[0]='\0'; //Just initialising

    //Comparing input string from user wiht "quit" and thus taking decision
    //of ending the connection or sending the input string to server
    while(strcmp(name,"quit\n") != 0){
        printf("Client : ");
        //Taking input from user
        bzero(name,sizeof(name));
        fgets(name,sizeof(name),stdin);
        if(strlen(name)==0){
            fgets(name,sizeof(name),stdin);
        }
        if(strcmp(name,"quit\n")==0){
            bzero(buffer,sizeof(buffer));
            strcpy(buffer,name);
            //Notifying server that client is quitting 
            send(sock,buffer,sizeof(buffer),0);
            break;
        } 

        int z=strlen(name);
        name[z-1]='\0'; //Validating filename

        //Sending the filename to server
        bzero(buffer,sizeof(buffer));
        strcpy(buffer,name);
        send(sock,buffer,sizeof(buffer),0);

        //Reading the notification regarding file's existence
        bzero(buffer,sizeof(buffer));
        recv(sock,buffer,sizeof(buffer),0);

        // No such file exists hence requesting for another file
        if(strcmp(buffer,"No Such File\n")==0){
            printf("Such file does not exist\n");
            continue;
        }

        //Opening(Writing) the binary file requested
        //If file already exists(on client side) then file is modified else new file is formed
        FILE *p;
        p=fopen(name,"wb");   
        char* path = realpath(name,NULL);

        //receiving struct of the file from the server and storing it in the client
        struct stat sb;
        recv(sock,(struct stat *)&sb,sizeof(sb),0);

        int len = strlen(buffer);
        bzero(buffer,sizeof(buffer));
        len = recv(sock,buffer,sizeof(buffer),0);
        while(1){
            
            if(len<MAX_SIZE){
                //it is the case where file transfer is completed
                if(len!=0){
                    //buffer[] is not empty the write in the file p
                    fwrite(buffer,sizeof(char),len,p);
                } 
                break;
            }
            //write the buffer in the file p
            fwrite(buffer,sizeof(char),len,p);
            bzero(buffer,sizeof(buffer));
            //receiving from the server for the next block
            len = recv(sock,buffer,sizeof(buffer),0);
        }
        
        //Assigning permissions to the file modified/created in the client
        // to keep permissions consistent
        mode_t new_mode = sb.st_mode;
        chmod(path,new_mode);

        //Closing the file
        fclose(p);
    }
    //Closing the socket as user wants to quit 
    close(sock);
    printf("[+] Connection Closed\n");
    return 0;
}