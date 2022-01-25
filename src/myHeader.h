#include <stdio.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>


// ---------- COSTANTI ----------
#define STDIN 0 // il file descriptor dello stdin ha come indice 0
#define HEADER_LEN 14
#define LOGIN_REQ_LEN 114
#define LOGIN_MSG_SIZE 114
#define USERNAME_LEN 50
#define TIMESTAMP_LEN 26

// -------------------------------
#define SENDING 0 
#define RECEIVING 1

#define RECEIVED 2 
#define ONLY_SENDED 3 

#define NO_MEAN 4
// -------------------------------


// identificatore di un gruppo
struct des_group
{
      int id;
      char name[50];
      char creator[50];
      struct group_peer * peer_list_head;
      struct des_group * pointer;
};

struct group_peer
{
      char username[50];
      struct group_peer * pointer;
}

// identificatore richiesta di notifica da un utente ad un altro
struct notify_queue
{
      char sender[50];
      char target[50];
      struct notify_queue * pointer;
};
// elemento del tipo lista utenti
struct clientList
{
      int socket;
      char username[50];
      struct clientList *pointer;
};
// descrittore di un comando da linea di comando
struct clientcmd
{
      char Command[20];
      char Argument1[50]; // qua prendo un username oppure il nome di un file (viene quindi definita la lunghezza massima che accetto)
};
// descrittore messaggio bufferizzato sul server
struct bufferedMessage
{
      char sender[50];        // mittente del messaggio
      char receiver[50];      // destinatario del messaggio
      char message[1024 * 4]; // prendendo in considerazione che telegram impone una dimensione massima per messaggio di 4096
      time_t timestamp;       // un timestamp che mi dice quando è stato inviato il messaggio
};
// descrittore record di history di un utente (viene utilizzato per tenere traccia dei login e dei logout)
struct HistoryRecord
{
      char Username[50];
      int Port;
      time_t timestamp_in;
      time_t timestamp_out;
};
// descrittore credenziali di un utente username:password
struct Credentials
{
      char Username[50];
      char Password[50];
};
// descrittore di un messaggio di servizio client-server server-client
struct msg_header
{
      char RequestType;
      char Options[7];
      char PortNumber[5];
};


// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////// GENERICHE ///////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void pulisci_buffer(char * buffer, int buf_len) {memset(buffer, 0 , buf_len);}



// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////// FUNZIONI DI STAMPA /////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void stampa_comandi_device()
{
      printf("-------------------- comandi device --------------------\n\n");
      printf("1 <hanging>:\n");
      printf("2 <show> <username>:\n");
      printf("3 <chat> <username>:\n");
      printf("4 <share> <file_name>: \n");
      printf("5 <out>:\n");
      printf("---------------------------------------------------------\n");
}

// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////// GESTIONE FILES  ////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int porta_da_username(char * username)
{
      int port = 0; 
      struct HistoryRecord record;
      FILE * fptr = fopen("Client_History.txt","rb");

      while(fread(&record, sizeof(record), 1 , fptr ))
      {
            if ( strcmp(username, record.Username) == 0 && record.timestamp_out == 0)
                  {
                        // utente trovato, ed è anche online quindi restituisco la porta
                        port = record.Port;
                        break;
                  } 
      }

      return port; 
}

void make_header(struct msg_header *header, char Type, char *Options, char *PortNumber, int size)
{
      memset(header, 0, size);
      header->RequestType = Type;
      strcpy(header->Options, Options);
      strcpy(header->PortNumber, PortNumber);
      return;
}


int check_username_online(char *Username)
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

int aggiorna_history_utente(FILE *fileptr, char *Username, char *port)
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
void stampa_history_utenti()
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
void bufferizza_msg(struct bufferedMessage *msg)
{
      // SCRIVE NEL FILE DEI MESSAGGI BUFFERIZZATI
      time_t rawtime;
      FILE *fileptr = fopen("Chat.txt", "ab+");
      // setto il timestamp del messaggio
      msg->timestamp = time(&rawtime);
      fwrite(msg, sizeof(struct bufferedMessage), 1, fileptr);
      fclose(fileptr);
}
void stampa_msg_bufferizzati()
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

// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////// GESTIONE LISTA /////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int inserisci_utente(struct clientList **head, char *Username, int socket)
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
int rimuovi_utente(struct clientList **head, int todelete, char *usernameToGet)
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
void stampa_lista_utenti(struct clientList *head)
{
      if (head == NULL)
            printf("lista vuota\n");
      else
            for (struct clientList *pun = head; pun != NULL; pun = pun->pointer)
                  printf("socket: %d username: %s\n", pun->socket, pun->username);
}

// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////// GESTIONE LISTA NOTIFY //////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int notify_enqueue(struct notify_queue **head, char *sender, char* target)
{
      struct notify_queue * node = (struct notify_queue *)malloc(sizeof(struct notify_queue));

      strcpy(node->sender, sender);
      strcpy(node->target, target);

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

int notify_dequeue(struct notify_queue **head, char * sender, char * target)
{
      struct notify_queue *pun;

      if (*head == NULL)
            return -1;

      // todelete si trova in testa
      if (strcmp((*head)->sender, sender) == 0 
      && strcmp((*head)->target, target) == 0)
      {
            pun = *head;
            *head = (*head)->pointer;
            free(pun);

            return 0;
      }

      // elemento in mezzo alla lista

      struct notify_queue *temp;
      for (pun = *head; pun != NULL; pun = pun->pointer, temp = pun)
            if    (strcmp((*head)->sender, sender) == 0 
                  && strcmp((*head)->target, target) == 0)
                  break;
            

      // non ho trovato nulla
      if (pun == NULL)
            return -1;

      // l'elemento è stato trovato
      // elimino
      temp->pointer = pun->pointer;
      return 0;
}

// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



// ritorna -1 se non trova user nel log dei registrati del server, 0 altrimenti
int is_client_registered(char *username)
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


// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////// GESTIONE SOCKET ////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// chiude la connessione con tutti i socket degli elementi presenti nella lista
void chiudi_connesioni_attive(struct clientList **head)
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
      FILE * fptr = fopen("Chat.txt","rb+");

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

                  strcpy(msg.sender,"junk");
                  fseek(fptr, -1 * sizeof(struct bufferedMessage), SEEK_CUR);
                  fwrite(&msg, sizeof(struct bufferedMessage), 1 , fptr);
            }
      }

      fclose(fptr);
      return 0;
}     

int invia_messaggio(char *send_buffer, int receiver_socket)
{
      int msg_len = strlen(send_buffer);
      int ret; 

      // dimensione
      ret = send(receiver_socket,(void*)&msg_len, sizeof(int), 0);

      // gestione disconnessione
      if ( ret == 0)
          return ret;

      if ( ret < 0 )
      {
            perror("Non sono riuscito ad inviare la dimensione del messaggio");
            return ret;
      }

      // messaggio
      ret = send(receiver_socket, (void*)send_buffer, msg_len, 0);
      if(ret < 0 )
      {
            perror("Non sono riuscito ad inviare il messaggio");
            return ret;
      }

      return ret;
}

int ricevi_messaggio(char *recv_buffer, int sender_socket)
{
      int msg_len; 
      int ret;

      // dimensione
      ret = recv(sender_socket, (void*)&msg_len, sizeof(int), 0);

      // gestione disconnessione
      if ( ret == 0)
        return ret;

      if (ret < 0)
            {
                  perror("Non sono riuscito a riceve la dimensione del messaggio");
                  return ret;
            }
      
      // messaggio
      ret = recv(sender_socket, (void*)recv_buffer, msg_len, 0);
      if (ret < 0)
            {
                  perror("Non sono riuscito a riceve il messaggio");
                  return ret;
            }

      return ret;
}


// invia un headser che ha la seguente struttura - req_type_options_portnumber
int invia_header(int receiver_socket, char req_type, char* options, char * port_number)
{
      char buf[1024];
      int msg_len;
      int ret;

      memset(buf, 0 , sizeof(buf));
      sprintf(buf,"%c %s %s",req_type, options, port_number);

      // dimensione header
      msg_len = strlen(buf);
      ret = send (receiver_socket, (void*)&msg_len, sizeof(int), 0);

      // gestione disconnessione
      if ( ret == 0)
        return ret;

      if ( ret < 0)
      {
            perror("Non sono riuscito a mandare la lunghezza dell' header: ");
            return ret;
      }        

      // invio header
      ret = send(receiver_socket, (void*)buf, msg_len, 0);
      if ( ret < 0)
      {
            perror("Non sono riuscito a mandare il messaggio di header");
            return ret;
      }

      return ret;
}

// riceve i dati di un header proveniente dal socket sender_socket e ne fa il parsing sulla struttra header
int ricevi_header(int sender_socket, struct msg_header * header)
{     
      int msg_len; // ci salvo la dimensione dell'header che sta arrivando
      char buf[1024]; // buffer su cui ricevo l'header
      int ret;

      memset(buf,0,sizeof(buf));

      // dimensione
      ret = recv(sender_socket, (void*)&msg_len, sizeof(int), 0); 

      // gestione disconnessione
      if ( ret == 0)
        return ret;

      if (ret < 0)
      {
            perror("errore nella ricezione della dimensione dell'header");
            return ret;
      }
      
      ret = recv(sender_socket,(void*)buf, msg_len, 0);
      if ( ret < 0 )
      {
            perror("nono sono riuscito a ricevere l'header");
            return ret;
      }

      // faccio il parsing sulla struttura header
      sscanf(buf,"%c %s %s",&header->RequestType,header->Options,header->PortNumber);
      return ret;
}

// copia nella stringa "buffer" gli username degli utenti online separati dal carattere '\n'
int copia_username_utenti_online(char *buffer)
{
      FILE * fptr = fopen("Client_History.txt","rb");
      struct HistoryRecord record;
      // tengo il conto di dove mi trovo nella stringa 
      int buf_index = 0; 

      while(fread(&record,sizeof(record),1,fptr))
      {     
            if (record.timestamp_out == 0)
                  {
                        // copia della stringa nel record in quella da spedire
                        for ( int i = 0; i < strlen(record.Username); i++, buf_index++)
                              buffer[buf_index] = record.Username[i];
                        // aggiungo il carattere di newline per distinguere le stringhe 
                        buffer[buf_index++] = '\n';
                  }
      }

      fclose(fptr);
      return buf_index;
}

// invia il messaggio contenuto nel buffer a tutti i socket presenti nella lista puntata da head
int invia_messaggio_gruppo(char *buffer, struct clientList * head)
{
      struct clientList * pun;
      int ret; 

      for ( pun = head; pun != NULL; pun = pun->pointer)
            {     
                  ret = invia_messaggio(buffer, pun->socket);
                  if ( ret < 0 )
                        return ret;
            }

      return ret;
}

// scrive il messaggio contenuto nella stringa "messaggio" nel file della chat di un utente my_username con dest_username
int scrivi_file_chat(char * my_username, char *dest_username, char * messaggio, int user_side, int msg_state)
{
      FILE * fptr;
      int ret;
      char file_path[100];


      /* 
      check di correttezza sulla variabile msg_state 
      (che mi dice se devo scrivere riguardo a messaggi 
      che sto mandado quando il dest è sicuramente online oppure quando lo sto bufferizzando e basta)
      */     
      
      if (msg_state != RECEIVED && msg_state != ONLY_SENDED && msg_state != NO_MEAN)
            return 0;
      /* 
      check di correttezza sulla variabile user_dest 
      (che mi dice se devo scrivere riguardo a messaggi 
      che sto mandado oppure rispetto a quelli che sto ricevendo)
      */
      
      if (user_side != RECEIVING && user_side != SENDING)
            return 0;

      mkdir(my_username,0777);

      pulisci_buffer(file_path, sizeof(file_path));
      sprintf(file_path,"%s//%s.txt", my_username,dest_username);

      fptr = fopen(file_path,"a+");
      if (fptr == NULL)
            {
                  printf("LOG: errore nell'apertura del file di chat\n");
                  return -1;
            }

      if (user_side == SENDING)
      {
            if (msg_state == RECEIVED)
                  fprintf(fptr,"--> %s > **\n",messaggio);
            else if (msg_state == ONLY_SENDED)
                  fprintf(fptr,"--> %s <*\n", messaggio);
      }     
      else
            fprintf(fptr,"          <-- %s\n", messaggio);

      fclose(fptr);
      return 0;
}


// carica la chat di my_username con l'utente user_target
// chiamo la carica_chat quando inizio una chat con l'utente user_target per caricare la cronologia
// ritorna il numero di byte letti
int carica_chat(char * my_username, char * dest_username)
{
      FILE * fptr;
      int ret = 0; 
      char toread;

      char file_path[100];

      mkdir(my_username,0777);

      pulisci_buffer(file_path, sizeof(file_path));
      sprintf(file_path,"%s//%s.txt", my_username,dest_username);
      
      fptr = fopen(file_path,"r+");
      if (fptr == NULL) 
      {
            printf("LOG: Errore nell'apertura del file\n");
            return ret;
      }
      // leggo il file 

      printf("////////////////////// chat con %s ////////////////////////\n",dest_username);
      printf("--------------- inizio messaggi precedenti ----------------\n");
      toread = fgetc(fptr);
      for (; toread != EOF; )
      {
            ret++;
            printf("%c",toread);
            toread = fgetc(fptr);
      } 
      printf("\n---------------- fine messaggi precedenti -----------------\n");

      return ret;
}

// prende in ingresso il socket di un utente e ne restituisce l'username associato 
// copiandolo nella variabile passata in ingresso
void  username_da_socket(int sender_socket, struct clientList * head, char * sender_username)
{

      struct clientList * pun;

      for ( pun = head; pun != NULL; pun = pun->pointer)
      {
            if ( pun->socket == sender_socket)
                  {
                        strcpy(sender_username, pun->username);
                        return;
                  }
      }
return;
}


// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////// CLIENT HANDLERS  ///////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


int handler_comand_show(char my_username [],char target_username[], int server_socket)
{
	int num_msg, ret;
	char buf[512];

	// invio richiesta di show al server
	invia_header(server_socket,'D',"toreq","0000");
	
	// serializzo buffer per l'invio
	pulisci_buffer(buf, sizeof(buf));
	sprintf(buf,"%s %s", my_username, target_username);
	// invio al server
	invia_messaggio(buf, server_socket);

	// ricevo il numero di messaggi pendenti da aspettarmi
	ret = recv(server_socket, (void*)&num_msg, sizeof(int), 0);
	if(ret < 0)
	{
		printf("LOG: Non sono riuscito ad ottenere il numero di messaggi pendenti\n");
		return ret;
	}
	printf("LOG: il server mi ha detto che %s mi ha inviato %d messaggi pendenti\n", target_username, num_msg);

	for (int i = 0; i < num_msg; i++)
	{
		char recvbuf[1024];

		// ricevo il messaggio pendente
		pulisci_buffer(recvbuf,sizeof(recvbuf));
		ricevi_messaggio(recvbuf, server_socket);
		// scrivo il messaggio sul file della chat dell'utente my_username
		scrivi_file_chat(my_username, target_username, recvbuf, RECEIVING, NO_MEAN);
		printf("messaggio pendente ricevuto: %s\n", recvbuf);
	}

	return num_msg;
}


int aggiorna_stato_messaggi(char * my_username, char * dest_username)
{ 
 
      char file_path[100];
      struct stat file;

      mkdir(my_username,0777);

      pulisci_buffer(file_path, sizeof(file_path));
      sprintf(file_path,"%s//%s.txt", my_username,dest_username);
      
      // soluzione con mapping dei file in memoria
      int fd = open(file_path, O_RDWR, S_IRUSR | S_IWUSR);

      // mi prendo la dimensione del file
      if (fstat(fd,&file) == -1)
      {
            perror("LOG: Non sono riuscito ad ottenere la dimensione del file");
            return -1;
      }

      // mapping      
      char * stringa_file =  (char *)mmap(NULL, file.st_size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
      if (stringa_file == NULL)
            return -1;

      for ( int i = 0 ; i < file.st_size; i++)
            if (stringa_file[i] == '<')
                        stringa_file[i] = '*';
                  

      munmap(stringa_file,file.st_size);
      close(fd);

      return 0;
}

void salva_disconnessione(char * my_username)
{
      FILE * fptr; 
      time_t rawtime;
      char buf[100];
      char * time_string  = (char *) malloc(sizeof(char) * 26);

      
      // buf in questo caso viene inizializzato con il path relativo
      pulisci_buffer(buf, sizeof(buf));
      sprintf(buf,"%s//%s_logout_info.txt",my_username,my_username);
      mkdir(my_username,0777);
      fptr = fopen(buf,"w+");
      // inizializzo timestamp
      time(&rawtime);
      time_string = ctime(&rawtime);
      // debug
      printf("%s %s",my_username, time_string);
      // scrivo la coppia di stringhe: username_timestamp_logout
      fprintf(fptr,"%s",time_string);
      fclose(fptr);
}

void prendi_istante_disconnessione(char * my_username, char * timestamp_to_get)
{
      char buf[100];
      FILE * fptr; 

      // inizializzo buf con il path del file da aprire (quello del log)
      pulisci_buffer(buf, sizeof(buf));
      sprintf(buf, "%s//%s_logout_info.txt", my_username,my_username);      
      // apro il file e prendo il timestamp
      fptr = fopen(buf, "r+");      
      fscanf(fptr, "%[^\n]",timestamp_to_get);
      fclose(fptr);
}

// ritorna 0 se il comando passato in ingresso ha la struttura share <filename>, -1 altrimenti
int check_share_command(char * my_username, char * command_string)
{
      char cmd[20];
      char file_name[50];
      char file_path[100];
      FILE * fptr;

      pulisci_buffer(cmd,sizeof(cmd));
      pulisci_buffer(file_name,sizeof(file_name));

      sscanf(command_string,"%s %s\n",cmd,file_name);
      printf("qua dentro");

      if (strcmp(cmd,"share") != 0)
            return -1;
      
      // controllo se il file passato in ingresso esiste
      pulisci_buffer(file_path,sizeof(file_path));
      sprintf(file_path,"%s//%s",my_username, file_name);
      
      fptr = fopen(file_path,"r");
      if (fptr == NULL)
            return -1;

      fclose(fptr);
      return 0;
}








// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ////////////////////////////////  FUNZIONI PER INVIO/RICEZIONE FILE  /////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////



/*
prende in ingresso il nome di un file (che deve essere presente nella directory "src" ) ed il socket a 
cui lo si vuole inviare, e lo invia al destinatario frammentandolo in diversi segmenti TCP se necessario
*/
int invia_file(char * my_username,char *file_name, int dest_socket)
{
      char file_path[100];
      struct stat file;
      char * file_send_buffer;

      // path del file
      sprintf(file_path,"%s//%s",my_username,file_name);


      if (invia_messaggio("***FILE***",dest_socket) < 0)
      {
            perror("LOG: dest non raggiungibile per l'invio del file");
            return -1;
      }

      // se il mesaggio ****FILE**** è stato inviato con successo allora allora il destinatario è online

      // ------------------ invio nome file ----------------------
      invia_messaggio(file_name, dest_socket);

      // soluzione mediante mapping del file in memoria
      // uso come buffer di invio la porzione di memoria dove viene mappato il file della mmap 

      int fd = open(file_path, O_RDWR, S_IRUSR | S_IWUSR);

      if (fstat(fd,&file) == -1)
      {
            perror("LOG: non sono riuscito ad ottenere la dimensione del file per l'invio");
            return -1;
      }

      int dim_file = file.st_size;
      
      // -------------- invio dimensione file ---------------
      if (send(dest_socket, (void*)&dim_file, sizeof(int), 0) < 0)
      {
            perror("LOG: Errore nell'invio della dimensione del file");
            return -1;
      }
      printf("LOG: la dimensione del file che sto mandando è %d\n", dim_file);      

      file_send_buffer = (char *) mmap(NULL, file.st_size, PROT_WRITE | PROT_READ, MAP_SHARED, fd, 0);
      
      // ----------------- invio del file -------------------
      int bytes_send, send_after;
      
      bytes_send = send(dest_socket, (void*)file_send_buffer, file.st_size, 0);
      if (bytes_send < 0)
      {
            perror("LOG: errore all'inizio dell' invio del file");      
            return bytes_send;
      }
      printf("LOG: il numero di byte mandati al primo invio è %d\n", bytes_send);      
      

      while( bytes_send < file.st_size )
      {
            send_after = send(dest_socket, (void*)&file_send_buffer[bytes_send], file.st_size - bytes_send, 0);
            if (send_after < 0)
            {
                  perror("LOG: errore DURANTE l'invio del file");
                  return send_after;
            }
            bytes_send += send_after;
      }

      munmap(file_send_buffer,file.st_size);
      close(fd);

      printf("********** fine invio file **********\n");

      return bytes_send;
}

void alloca_dim_file(char * file_path, int dim_file)
{
      FILE * ptr = fopen(file_path,"w");
      for(int i = 0 ; i < dim_file; i++) { fputc(' ',ptr); }
      fclose(ptr);
}
// questa funzione riceve un file inviato dal device connesso sul socket sender_socket e lo salva nella directory personale del device
int ricevi_file(char *my_username,int sender_socket)
{
      int dim_file;
      int ret;
      char file_name[50];
      char file_path[100];
      char * file_receive_buffer;
      char init = 'C';

      pulisci_buffer(file_name,sizeof(file_name));
      
      // ----------- ricezione nome file ---------------
      if (ricevi_messaggio(file_name, sender_socket) < 0)
      {
            perror("LOG: erorre nella ricezione del file_name");
            return -1;
      }
      

      // ------------- ricezione dimensione file ------------
      
      ret = recv(sender_socket, (void*)&dim_file, sizeof(int), 0);
      if (ret < 0)
      {
            perror("LOG: Errore nella ricezione della dimensione del file");
            return ret; 
      }
      printf("LOG: La dimensione del file che sto ricevendo è di %d byte\n", dim_file);
      
      // --- creazione file sulla macchina del destinatario -------
      
      pulisci_buffer(file_path,sizeof(file_path));
      sprintf(file_path,"%s//%s", my_username,file_name);
      
      alloca_dim_file(file_path, dim_file);

      int fd = open(file_path, O_RDWR | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);

      // mapping 
      file_receive_buffer = (char*) mmap(NULL, dim_file, PROT_WRITE | PROT_READ, MAP_SHARED , fd , 0);
      if (file_receive_buffer == NULL)
            {
                  perror("LOG: errore con la map");
                  return -1;
            }
      

      // ------------- ricezione file ---------------
      int bytes_read , read_after; 
      bytes_read = recv(sender_socket, (void *)file_receive_buffer, 25, 0 );
      
      if (bytes_read < 0)
      {
            perror("LOG: errore durante l'inzio della ricezione del file");
            return bytes_read;
      }
      
      while(bytes_read < dim_file)
      {
            read_after = recv(sender_socket, (void*)&file_receive_buffer[bytes_read], dim_file - bytes_read, 0);
            if (read_after < 0)
            {
                  perror("LOG: errore DURANTE la ricezione del file");
                  return read_after;
            }

            bytes_read += read_after;
      }
      
      munmap(file_receive_buffer, dim_file);
      close(fd);

      return bytes_read;
}



// restituisce un socket associato all'username di un elemento in una clientList
int socket_da_username(struct clientList * head, char * username)
{
      if (head == NULL)
            return -1; 

      for ( struct clientList * pun = head; pun != NULL; pun = pun->pointer)
            if (strcmp(username, pun->username) == 0)
                  return pun->socket;
      
      return -1;
}

// ritorna il numero di elementi presenti nella lista del tipo clientlist
int conta_utenti_chat(struct clientList * head)
{     
      int i = 0;

      if (head == NULL)
            return i;
      
      for ( struct clientList * pun = head; pun != NULL; pun = pun->pointer, i++){};      

      return i; 
}

// elimina tutti gli utenti dalla lista del tipo clientList, ritorna il numero di utenti eliminati
int elimina_utenti_lista(struct clientList ** head)
{
      struct clientList * temp = *head;
      int i = 0;

      if (*head == NULL)
            return 0;

      while (*head != NULL)
      {
            *head = (*head)->pointer;
            free(temp);
            temp = *head;
            i++;
      }

      return i;
}

// invia all'utente connesso su dest socket i nomi degli utenti della sua chat di gruppo
int invia_nomi_utenti(struct clientList * head, int dest_socket)
{
      int i = 0;

      if (head == NULL)
            return i;

      for (struct clientList * pun = head; pun != NULL; pun = pun->pointer, i++)
      {     
            // invio il nome dell'utente presente nel gruppo al nuovo utente
            invia_messaggio(pun->username,dest_socket);
      }

      return i;     
}