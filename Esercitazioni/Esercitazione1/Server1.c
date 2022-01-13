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

    int ret, sd, new_sd, len;
    struct sockaddr_in my_addr, cl_addr;
    char buffer[1024];

    sd = socket(AF_INET, SOCK_STREAM, 0);

    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(4242);
    inet_pton(AF_INET, "127.0.0.1", &my_addr.sin_addr);

    ret = bind(sd, (struct sockaddr *)&my_addr, sizeof(my_addr));

    ret = listen(sd, 10);

    if (ret < 0)
    {
        perror("errore in fase di bind: \n");
        exit(-1);
    }

    while (1)
    {
        len = sizeof(cl_addr);

        new_sd = accept(sd, (struct sockaddr *)&cl_addr, &len);

        strcpy(buffer, "Hello!");
        len = strlen(buffer);

        ret = send(new_sd, (void *)buffer, len, 0);

        if (ret < 0)
        {
            perror("errore in fase di invio \n");
            exit(-1);
        }

        close(new_sd);
    }
}