#include <stdio.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

// ---------- COSTANTI ----------
#define STDIN 0 // il file descriptor dello stdin ha come indice 0
#define HEADER_LEN 14
#define LOGIN_REQ_LEN 114
#define LOGIN_MSG_SIZE 114

struct clientcmd
{
    char Command[20];
    char Argument1[50]; // qua prendo un username oppure il nome di un file (viene quindi definita la lunghezza massima che accetto)
};

struct bufferedMessage
{
    char sender[50];        // mittente del messaggio
    char receiver[50];      // destinatario del messaggio
    char message[1024 * 4]; // prendendo in considerazione che telegram impone una dimensione massima per messaggio di 4096
    time_t timestamp;       // un timestamp che mi dice quando è stato inviato il messaggio
};
struct HistoryRecord
{
    char Username[50];
    int Port;
    time_t timestamp_in;
    time_t timestamp_out;
};
struct Credentials
{
    char Username[50];
    char Password[50];
};
struct msg_header
{
    char RequestType;
    char Options[7];
    char PortNumber[5];
};
struct msg
{
    struct msg_header Header;
    char Payload[1024 * 4];
};
struct chatMsg
{
    char sender[50];
    char receiver[50];
    char message[1024 * 4];
};
struct clientNode
{
    char Username[50];
    int socket;
    struct clientNode *pointer;
};

// ------------------FUNZIONI--------------------------------------
// quando un utente cerca di loggarsi sul server, questa funzione controlla che tale utente sia presente fra gli utenti registrati
int LoginCheck(FILE *fileptr, struct Credentials *cl_credentials, int size)
{
    struct Credentials MyCredentials;
    fileptr = fopen("Log.txt", "rb");
    while (fread(&MyCredentials, size, 1, fileptr))
    {
        if (strcmp(MyCredentials.Username, cl_credentials->Username) == 0 && strcmp(MyCredentials.Password, cl_credentials->Password) == 0)
        {
            fclose(fileptr);
            return 0;
        }
    }
    fclose(fileptr);
    return -1;
}
void make_header(struct msg_header *header, char Type, char *Options, char *PortNumber, int size)
{
    memset(header, 0, size);
    header->RequestType = Type;
    strcpy(header->Options, Options);
    strcpy(header->PortNumber, PortNumber);
    return;
}
int isOnline(char *Username)
{
    // apro il file di history
    FILE *fptr;
    struct HistoryRecord fileRecord;
    fptr = fopen("Client_history.txt", "rb");
    while (fread(&fileRecord, sizeof(fileRecord), 1, fptr))
    {
        if (strcmp(fileRecord.Username, Username) == 0 && (fileRecord.timestamp_out == 0) && (fileRecord.Port != 0))
        {
            printf("sto controllando se un client è online e la sua porta è filerecord.port -> %d\n", fileRecord.Port);
            fclose(fptr);
            return fileRecord.Port;
        }
    }
    fclose(fptr);
    return -1;
}
int historyUpdateLogin(FILE *fileptr, char *Username, char *port)
{
    struct HistoryRecord record;
    time_t rawtime;
    fileptr = fopen("Client_history.txt", "rb+");
    while (fread(&record, sizeof(struct HistoryRecord), 1, fileptr))
    {
        if (strcmp(record.Username, Username) == 0)
        {
            // ho trovato il record che cercavo e quindi ne aggiorno il campo timestamp_in4
            record.timestamp_in = time(&rawtime);
            record.timestamp_out = 0;
            record.Port = atoi(port);
            // stampo i valori che sto per scrivere
            printf("Sto scrivendo sul registro client history i seguenti valori\nUsername: %s\nTimestampIN: %ld\nTimestampOUT: %ld\nPort: %d\n", record.Username, record.timestamp_in, record.timestamp_out, record.Port);
            fseek(fileptr, -1 * sizeof(struct HistoryRecord), SEEK_CUR);
            fwrite(&record, sizeof(struct HistoryRecord), 1, fileptr);
            fclose(fileptr);
            return 0;
        }
    }

    fclose(fileptr);
    return -1;
}
void printHistory()
{
    FILE *fptr;
    struct HistoryRecord record;
    fptr = fopen("Client_History.txt", "rb");
    while (fread(&record, sizeof(struct HistoryRecord), 1, fptr))
    {
        printf("Username: %s|timestampIN: %ld|timestampOUT: %ld|Porta: %d\n", record.Username, record.timestamp_in, record.timestamp_out, record.Port);
    }
    fclose(fptr);
}
void bufferWrite(struct bufferedMessage *msg)
{
    // SCRIVE NEL FILE DEI MESSAGGI BUFFERIZZATI
    time_t rawtime;
    FILE *fileptr = fopen("Chat.txt", "ab+");
    // setto il timestamp del messaggio
    msg->timestamp = time(&rawtime);
    fwrite(msg, sizeof(struct bufferedMessage), 1, fileptr);
    fclose(fileptr);
}
void printfBuffer()
{
    FILE *fptr;
    fptr = fopen("Chat.txt", "rb");

    struct bufferedMessage record;
    while (fread(&record, sizeof(record), 1, fptr))
    {
        printf("sender:%s|receiver: %s|messaggio: %s|timestamp %ld\n", record.sender, record.receiver, record.message, record.timestamp);
    }
}
int countBuffered(char *dest, char *mitt)
{
    int nmsg = 0;
    struct bufferedMessage msg;
    FILE *fptr = fopen("Chat.txt", "rb");
    while (fread(&msg, sizeof(msg), 1, fptr))
    {
        if (strcmp(msg.sender, mitt) == 0 && strcmp(msg.receiver, dest) == 0)
            nmsg++;
    }
    fclose(fptr);
    return nmsg;
}
int check(char *cmd)
{
    if ((strcmp("share", cmd) == 0) || (strcmp("hanging", cmd) == 0) || (strcmp("show", cmd) == 0) || (strcmp("chat", cmd) == 0) || (strcmp("out", cmd) == 0))
        return 0;
    else
        return -1;
}
