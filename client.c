#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>

#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/rsa.h>
#include <openssl/evp.h>
#include <openssl/bio.h>
#include <openssl/err.h>

#define BUFSIZE 4098
int padding = RSA_PKCS1_PADDING;

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

int private_decrypt(unsigned char *enc_data, int data_len, unsigned char *keyFile, unsigned char *decrypted)
{
    RSA *rsa = createRSAWithFilename(keyFile, 0);
    int result = RSA_private_decrypt(data_len, enc_data, decrypted, rsa, padding);
    return result;
}

void send_recv(int i, int sockfd, int clientNum)
{
    char send_buf[BUFSIZE];
    char recv_buf[BUFSIZE];
    bzero(send_buf, sizeof(send_buf));
    bzero(recv_buf, sizeof(recv_buf));
    int nbyte_recvd;

    if (i == 0)
    {
        printf("Send: ");
        fflush(stdout);
        fgets(send_buf, BUFSIZE, stdin);
        if (strcmp(send_buf, "EXIT\n") == 0)
        {
            exit(0);
        }
        else
        {
            send(sockfd, send_buf, strlen(send_buf), 0);
        }
    }
    else
    {
        nbyte_recvd = recv(sockfd, recv_buf, BUFSIZE, 0);
        printf("%s\n",recv_buf);
        char *privateKeyFile;
        if (clientNum == 1)
        {
            privateKeyFile = "privateKey_client1.txt";
        }
        else
        {
            privateKeyFile = "privateKey_client2.txt";
        }

        unsigned char decrypted[4098] = {};
        int decrypted_length = private_decrypt(recv_buf, 256, privateKeyFile, decrypted);
        if (decrypted_length == -1)
        {
            printf("Private Decrypt failed");
            exit(0);
        }
        printf("\nReceived text decrypted: %s", decrypted);
        printf("Send: ");
        fflush(stdout);
    }
}

void connect_request(int *sockfd, struct sockaddr_in *server_addr, char *IP, int PORT)
{
    if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Socket");
        exit(1);
    }
    server_addr->sin_family = AF_INET;
    server_addr->sin_port = htons(PORT);
    server_addr->sin_addr.s_addr = inet_addr(IP);
    memset(server_addr->sin_zero, '\0', sizeof server_addr->sin_zero);

    if (connect(*sockfd, (struct sockaddr *)server_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("connect");
        exit(1);
    }

    printf("Client connected to the server\n");
    printf("Send: ");
    fflush(stdout);
}

int main(int argc, char *argv[])
{
    int sockfd, fdmax, i;
    struct sockaddr_in server_addr;
    fd_set master;
    fd_set read_fds;

    if (argc != 4)
    {
        printf("Required parameters not provided. Please try again!\n");
        exit(-1);
    }

    char *IP = argv[1];
    int PORT = atoi(argv[2]);
    int clientNum = atoi(argv[3]);

    connect_request(&sockfd, &server_addr, IP, PORT);
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(0, &master);
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
            if (FD_ISSET(i, &read_fds))
                send_recv(i, sockfd, clientNum);
    }
    printf("Client Quited\n");
    close(sockfd);
    return 0;
}