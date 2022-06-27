#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#include <assert.h>
#include <time.h>

#define BUFSIZE 4098
int padding = RSA_PKCS1_PADDING;
int clientID = 0;
int numOfClients = 0;
int client1Socket;
int client2Socket;
struct sockaddr_in client_addr1, client_addr2;

RSA *createRSAWithFilename(char *filename, int public)
{
    FILE *fp = fopen(filename, "rb");

    if (fp == NULL)
    {
        printf("Unable to open file %s \n", filename);
        return NULL;
    }
    RSA *rsa = RSA_new();

    if (public)
    {
        rsa = PEM_read_RSA_PUBKEY(fp, &rsa, NULL, NULL);
    }
    else
    {
        rsa = PEM_read_RSAPrivateKey(fp, &rsa, NULL, NULL);
    }

    return rsa;
}

int public_encrypt(unsigned char *data, int data_len, unsigned char *keyFile, unsigned char *encrypted)
{
    RSA *rsa = createRSAWithFilename(keyFile, 1);
    int result = RSA_public_encrypt(data_len, data, encrypted, rsa, padding);
    return result;
}

void send_to_all(int j, int i, int sockfd, int nbytes_recvd, char *recv_buf, fd_set *master)
{
    if (FD_ISSET(j, master))
    {
        if (j != sockfd && j != i)
        {
            if (send(j, recv_buf, nbytes_recvd, 0) == -1)
            {
                perror("send");
            }
            if (j == client1Socket)
            {
                char addrbuff[BUFSIZE];
                printf("MESSAGE SENT TO %s:%d\n", inet_ntop(AF_INET, &client_addr1.sin_addr, addrbuff, sizeof(addrbuff)), ntohs(client_addr1.sin_port));
            }
            else if (j == client2Socket)
            {
                char addrbuff[BUFSIZE];
                printf("MESSAGE SENT TO %s:%d\n", inet_ntop(AF_INET, &client_addr2.sin_addr, addrbuff, sizeof(addrbuff)), ntohs(client_addr2.sin_port));
            }
        }
    }
}

void send_recv(int i, fd_set *master, int sockfd, int fdmax, char *publicClient1, char *publicClient2)
{
    int nbytes_recvd = 0, j;
    char recv_buf[BUFSIZE], buf[BUFSIZE];
    bzero(buf, sizeof(buf));
    bzero(recv_buf, sizeof(recv_buf));

    if ((nbytes_recvd = recv(i, recv_buf, BUFSIZE, 0)) <= 0)
    {
        if (nbytes_recvd == 0)
        {
            printf("Socket %d hung up\n", i);
            numOfClients--;
        }
        else
        {
            perror("recv");
        }
        close(i);
        FD_CLR(i, master);
    }
    else
    {

        FILE *fptr;

        time_t t = time(NULL);
        struct tm *tm = localtime(&t);
        char s[64];
        assert(strftime(s, sizeof(s), "%c", tm));

        char *publicKeyFile;
        if (i == client1Socket)
        {
            char dest[] = "client1_";
            strcat(dest, s);
            strcat(dest, ".txt");
            fptr = fopen(dest, "w");
            publicKeyFile = publicClient2;
        }
        else
        {
            char dest[] = "client2_";
            strcat(dest, s);
            strcat(dest, ".txt");
            fptr = fopen(dest, "w");
            publicKeyFile = publicClient1;
        }

        if (fptr == NULL)
        {
            printf("Error in opening file!");
            exit(1);
        }

        fprintf(fptr, "%s", recv_buf);

        unsigned char encrypted[4098] = {};

        int encrypted_length = public_encrypt(recv_buf, strlen(recv_buf), publicKeyFile, encrypted);
        if (encrypted_length == -1)
        {
            printf("Public Encrypt failed ");
            fclose(fptr);
            exit(0);
        }

        fptr = freopen(NULL, "w", fptr);
        fprintf(fptr, "%s", encrypted);
        fclose(fptr);

        for (j = 0; j <= fdmax; j++)
        {

            send_to_all(j, i, sockfd, encrypted_length, encrypted, master);
        }
    }
}

void connection_accept(fd_set *master, int *fdmax, int sockfd, struct sockaddr_in *client_addr, int clientNumb)
{
    socklen_t addrlen;
    int newsockfd;

    addrlen = sizeof(struct sockaddr_in);
    if ((newsockfd = accept(sockfd, (struct sockaddr *)client_addr, &addrlen)) == -1)
    {
        perror("accept");
        exit(1);
    }
    else
    {
        FD_SET(newsockfd, master);
        if (newsockfd > *fdmax)
        {
            *fdmax = newsockfd;
        }
        if (clientNumb == 0)
        {
            client1Socket = newsockfd;
        }
        else
        {
            client2Socket = newsockfd;
        }
        printf("New connection from %s on port %d \n", inet_ntoa(client_addr->sin_addr), ntohs(client_addr->sin_port));
        numOfClients++;
    }
}

void connect_request(int *sockfd, struct sockaddr_in *my_addr, int PORT)
{
    int yes = 1;

    if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket");
        exit(1);
    }

    my_addr->sin_family = AF_INET;
    my_addr->sin_port = htons(PORT);
    my_addr->sin_addr.s_addr = INADDR_ANY;
    memset(my_addr->sin_zero, '\0', sizeof my_addr->sin_zero);

    if (setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
    {
        perror("setsockopt");
        exit(1);
    }

    if (bind(*sockfd, (struct sockaddr *)my_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("Unable to bind");
        exit(1);
    }
    if (listen(*sockfd, 10) == -1)
    {
        perror("listen");
        exit(1);
    }
    printf("\nTCPServer Waiting for client on port %d\n", PORT);
    fflush(stdout);
}
int main(int argc, char *argv[])
{
    fd_set master;
    fd_set read_fds;
    int fdmax, i;
    int sockfd = 0;
    struct sockaddr_in my_addr;

    if (argc != 4)
    {
        printf("Required parameters not provided");
    }

    int PORT = atoi(argv[1]);
    char *publicClient1 = argv[2];
    char *publicCLient2 = argv[3];

    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    connect_request(&sockfd, &my_addr, PORT);
    FD_SET(sockfd, &master);

    fdmax = sockfd;
    while (1)
    {
        read_fds = master;
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1)
        {
            perror("select");
            exit(4);
        }

        for (i = 0; i <= fdmax; i++)
        {
            if (FD_ISSET(i, &read_fds))
            {
                if (i == sockfd && clientID == 0 && numOfClients < 2)
                {
                    connection_accept(&master, &fdmax, sockfd, &client_addr1, 0);
                    clientID = 1;
                }
                else if (i == sockfd && clientID == 1 && numOfClients < 2)
                {
                    connection_accept(&master, &fdmax, sockfd, &client_addr2, 1);
                    clientID = 0;
                }
                else if (i == sockfd && numOfClients == 2)
                {
                    printf("Cant connect to more clients");
                }
                else
                    send_recv(i, &master, sockfd, fdmax, publicClient1, publicCLient2);
            }
        }
    }
    return 0;
}