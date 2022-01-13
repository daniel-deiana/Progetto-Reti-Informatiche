#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int ret, sd, len;
    struct sockaddr_in srv_addr;
    char buffer[1024];

    sd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&srv_addr, 0, sizeof(srv_addr));
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(4242);
    inet_pton(AF_INET, "127.0.0.1", &srv_addr.sin_addr);

    ret = connect(sd, (struct sockaddr *)&srv_addr, sizeof(srv_addr));

    if (ret < 0)
    {
        perror("errore in fase di bind: \n");
        exit(-1);
    }

    while (1)
    {
        len = 6;
        ret = recv(sd, (void *)buffer, len, 0);

        if (ret < 0)
        {
            perror("errore in fase di ricezione: \n");
            exit(-1);
        }

        buffer[6] = '\0';
        printf("%s\n", buffer);
    }

    close(sd);
}