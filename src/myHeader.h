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
#define USERNAME_LEN 50

struct clientList
{
      int socket;
      char username[50];
      struct clientList *pointer;
};
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
                  // ho trovato il record che cercavo e quindi ne aggiorno il campo timestamp_in
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

      printf("sono dentro la count_buffered e mi hanno detto che devo cercare i messaggi inviati dal target %s al richiedente %s\n",mitt,dest);

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
int handlerFriends(char *srcUsername, char *destUsername)
{
      switch (srcUsername[4])
      {
      case '1':
            if (strcmp(destUsername, "user2") == 0 || strcmp(destUsername, "user1") == 0)
                  return 0;
            else
                  return -1;
      case '2':
            if (strcmp(destUsername, "user1") == 0 || strcmp(destUsername, "user3") == 0)
                  return 0;
            else
                  return -1;
      case '3':
            if (strcmp(destUsername, "user2") == 0)
                  return 0;
            else
                  return -1;
            break;
      }
      return -1;
}
// Routine di logout: quando un utente si disconette aggiorna il campo timestamp_out al tempo corrente
void logout(char *user)
{
      // prende in ingresso user e cerca una corrispondenza nel file degli utenti online e ne cambia il ts
      FILE *fptr = fopen("Client_history.txt", "rb+");
      struct HistoryRecord record;
      time_t rawtime;

      while (fread(&record, sizeof(struct HistoryRecord), 1, fptr))
      {
            if (strcmp(record.Username, user) == 0)
            {
                  // ho trovato il record che cercavo e quindi ne aggiorno il campo timestamp_out
                  // modifico solo il timestamp
                  record.timestamp_out = time(&rawtime);
                  // scrivo sul relativo record nel file
                  fseek(fptr, -1 * sizeof(struct HistoryRecord), SEEK_CUR);
                  fwrite(&record, sizeof(struct HistoryRecord), 1, fptr);
                  fclose(fptr);
            }
      }
}

/*
      ================= LISTA =====================
*/

int pushUser(struct clientList **head, char *Username, int socket)
{
      struct clientList *node = (struct clientList *)malloc(sizeof(struct clientList));
      node->pointer = NULL;
      node->socket = socket;
      strcpy(node->username, Username);

      // il nodo puntato da elem è gia inzializzato quando chiamo la routine
      if (*head == NULL)
            *head = node;
      else
      { // piazza elem in testa alla lista
            node->pointer = *head;
            *head = node;
      }

      return 0;
}
// ritorna -1 se l'operazione non ha successo, 0 altrimenti
// se ha successo scrive in un puntatore l'username dell'utente che si è disconnesso
int deleteUser(struct clientList **head, int todelete, char *usernameToGet)
{
      struct clientList *pun;

      if (head == NULL)
            return -1;

      // todelete si trova in testa
      if ((*head)->socket == todelete)
      {
            strcpy(usernameToGet, (*head)->username);
            pun = *head;
            *head = (*head)->pointer;
            free(pun);

            return -1;
      }

      // elemento in mezzo alla lista

      struct clientList *temp;
      for (pun = *head; pun != NULL; pun = pun->pointer, temp = pun)
            if (pun->socket == todelete)
                  break;

      // non ho trovato nulla
      if (pun == NULL)
            return -1;

      // l'elemento è stato trovato
      strcpy(usernameToGet, pun->username);
      // elimino
      temp->pointer = pun->pointer;
      return 0;
}
void printList(struct clientList *head)
{
      if (head == NULL)
            printf("lista vuota\n");
      else
            for (struct clientList *pun = head; pun != NULL; pun = pun->pointer)
                  printf("socket: %d username: %s\n", pun->socket, pun->username);
}

// ritorna -1 se non trova user nel log dei registrati del server, 0 altrimenti
int isClientRegistered(char *username)
{
      FILE *fptr = fopen("Log.txt", "rb");
      struct Credentials myCredentials; // usato per fare il parsing della struttura nel file

      while (fread(&myCredentials, sizeof(myCredentials), 1, fptr))
      {
            if (strcmp(username, myCredentials.Username) == 0)
                  return 0;
      }

      return -1;
}

// chiude la connessione con tutti i socket degli elementi presenti nella lista
void close_communications(struct clientList **head)
{
      struct clientList *pun;
      for (pun = *head; pun != NULL; pun = pun->pointer)
      {
            close(pun->socket);
            printf("ho chiuso il socket: %d\n", pun->socket);
      }
}



// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////// GESTIONE MESSAGGI //////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int invia_messaggi_pendenti(char *richiedente, char* target, int dest_socket)
{
      int msg_num;
      struct bufferedMessage msg; // uso per fare il parsing dei messaggi nel file
      FILE * fptr = fopen("Chat.txt","rb");

      msg_num = countBuffered(richiedente,target);
      printf("countBuffered ha restituito %d messaggi\n", msg_num);

      if (send(dest_socket, (void*)&msg_num, sizeof(int), 0) < 0)
            {
                  perror("Nono sono riuscito a mandare il numero di messaggi pendenti\n");
                  return -1;
            }

      // scorro i messaggi da target verso richiedente e li invio al richiedente
      while (fread(&msg,sizeof(struct bufferedMessage), 1 , fptr))
      {
            if (strcmp(msg.receiver,richiedente) == 0 && strcmp(msg.sender,target) == 0) 
            {            
                  int msg_len;

                  // invio dimensione del msg
                  msg_len = strlen(msg.message);
                  if (send(dest_socket,(void*)&msg_len, sizeof(int), 0) < 0 )
                        perror("Non sono riuscito ad inviare la dimensione del messaggio\n");
                        
                  if (send(dest_socket,(void*)&msg.message,msg_len,0) < 0 )
                        perror("Non sono riuscito ad inviare il messaggio bufferizzato\n");
            }
      }

      fclose(fptr);
      return 0;
}     

int invia_messaggio(char *send_buffer, int receiver_socket)
{
      int msg_len = strlen(send_buffer);

      if (send(receiver_socket,(void*)&msg_len, sizeof(int), 0) < 0 )
      {
            perror("Non sono riuscito ad inviare la dimensione del messaggio");
            return -1;
      }

      if(send(receiver_socket, (void*)send_buffer, msg_len, 0) < 0 )
      {
            perror("Non sono riuscito ad inviare il messaggio");
            return -1;
      }

      return 0;
}

int ricevi_messaggio(char *recv_buffer, int sender_socket)
{
      int msg_len; 

      // ricevo la dimensione
      if (recv(sender_socket, (void*)&msg_len, sizeof(int), 0) < 0)
            {
                  perror("Non sono riuscito a riceve la dimensione del messaggio");
                  return -1;
            }
      if(recv(sender_socket, (void*)recv_buffer, msg_len, 0) < 0)
            {
                  perror("Non sono riuscito a riceve il messaggio");
                  return -1;
            }

      return 0 ;
}


// invia un header che ha la seguente struttura - req_type:options:portnumber
int invia_header(int receiver_socket, char req_type, char* options, char * port_number)
{
      char buf[1024];
      int msg_len;

      memset(buf, 0 , sizeof(buf));
      sprintf(buf,"%c:%s:%s",req_type, options, port_number);

      // mando la dimensione dell'header
      msg_len = strlen(buf);
      if ( send (receiver_socket, (void*)&msg_len, sizeof(int), 0) < 0)
      {
            perror("Non sono riuscito a mandare la lunghezza dell' header: ");
            return -1;
      }        

      if ( send(receiver_socket, (void*)buf, msg_len, 0) < 0) 
      {
            perror("Non sono riuscito a mandare il messaggio di header");
            return -1;
      }

      return 0;
}

// riceve i dati di un header proveniente dal socket sender_socket e ne fa il parsing sulla struttra header
int ricevi_header(int sender_socket, struct msg_header * header)
{     
      int msg_len; // ci salvo la dimensione dell'header che sta arrivando
      char buf[1024]; // buffer su cui ricevo l'header

      memset(buf,0,sizeof(buf));

      if (recv(sender_socket, (void*)&msg_len, sizeof(int), 0) < 0)
      {
            perror("errore nella ricezione della dimensione dell'header");
            return -1;
      }
      
      if (recv(sender_socket,(void*)buf, msg_len, 0) < 0 )
      {
            perror("nono sono riuscito a ricevere l'header");
            return -1;
      }

      // faccio il parsing sulla struttura header
      sscanf(buf,"%c:%s:%s",&header->RequestType,header->Options,header->PortNumber);
      return 0;
}