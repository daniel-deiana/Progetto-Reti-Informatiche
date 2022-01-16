#include "myHeader.h"
// =========== CODICE CLIENT ===========
int main(int argc, const char **argv)
{
      // lista socket attivi
      struct clientList *active_sockets_list_head = NULL;
      // strutture dati definite ad hoc
      struct msg ClientMessage;
      struct Credentials my_credentials;
      // strutture dati indirizzo
      struct sockaddr_in server_addr, cl_addr, cl_listen_addr, gp_addr;
      // per la creazione dei socket e i valori di return delle system call di rete
      int sv_communicate, communicate, cl_socket, listener, ret, msglen, fdmax = 0;
      // flag per la chat con un destinatario
      int isDestOnline = -1, isChatting = -1;
      // buffer che vengono usati per inviare i messaggi oppure per prendere in input dei comandi da terminale
      char buffer[1024 * 4 + HEADER_LEN], LogInCommand[20];
      // Rubrica
      char rubrica[3][50];
      strcpy(rubrica[0], "user1");
      strcpy(rubrica[1], "user2");
      strcpy(rubrica[2], "user3");
      // stringa per le conversioni dei numeri di porta
      char Port[5];
      char portChat[5];
      char headerChat_string[HEADER_LEN] = "";
      char string[HEADER_LEN];
      char destUsername[50] = "";

      fd_set master, readfds;
      FILE *RegistrationLog, *friends;
      // inizializzo i file descriptor
      FD_ZERO(&master);
      FD_ZERO(&readfds);

      // ------ Creazione indirizzo del server ------
      memset(&server_addr, 0, sizeof(server_addr));
      server_addr.sin_family = AF_INET;
      server_addr.sin_port = htons(4242);
      inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

      // CREAZIONE DEL SOCKET COMUNICAZIONI CLIENT-SERVER
      sv_communicate = socket(AF_INET, SOCK_STREAM, 0);
      // aggiungo il socket di comunincazione con il server tra i monitorati dall select
      FD_SET(STDIN, &master);

      if (argc != 2)
      {
            perror("Il numero di parametri che ho inserito all'avvio è sbagliato");
            exit(1);
      }

      strcpy(Port, argv[1]);
      printf("\n%s\n", Port);

      // indirizzo socket di listen
      memset(&cl_addr, 0, sizeof(cl_addr));
      cl_listen_addr.sin_family = AF_INET;
      cl_listen_addr.sin_port = htons(atoi(Port));
      inet_pton(AF_INET, "127.0.0.1", &cl_listen_addr.sin_addr);

      // CREAZIONE SOCKET DI ASCOLTO PER COMUNICAZIONI CLIENT-CLIENT / SERVER/CLIENT
      listener = socket(AF_INET, SOCK_STREAM, 0);
      bind(listener, (struct sockaddr *)&cl_listen_addr, sizeof(cl_listen_addr));
      listen(listener, 50);

      FD_SET(listener, &master);
      fdmax = listener;

      // ho due comandi disponibili che sono signup ed in, posso eseguirli tutti e 2
      // se eseguo signup essendo gia registrato allora il server mi avvisa
      // se provo a fare in senza essere registrato allora

      // faccio la connect al server
      if (connect(sv_communicate, (struct sockaddr *)&server_addr, sizeof(server_addr)))
            exit(1);

      for (;;)
      {
            char commandString[1024], headerBuffer[1024], sendbuffer[1024];
            char cmd[20], first[50], second[50], third[50];
            struct msg_header header;
            int ret;

            // parsing comandi
            fgets(commandString, 1024 - 1, stdin);
            sscanf(commandString, "%s %s %s %s", cmd, first, second, third);

            printf("stampo i comandi passati in input: first: %s, second: %s, third %s\n", first, second, third);
            if (strcmp(cmd, "signup") == 0)
            {
                  make_header(&header, 'A', "reg", Port, HEADER_LEN);
                  sprintf(sendbuffer, "%s %s", first, second);
            }
            else if (strcmp(cmd, "in") == 0)
            {
                  make_header(&header, 'B', "login", Port, HEADER_LEN);
                  strcpy(my_credentials.Username, second);
                  strcpy(my_credentials.Password, third);
                  sprintf(sendbuffer, "%s %s", second, third);
            }

            // invio header
            sprintf(headerBuffer, "%c%7s%5s", header.RequestType, header.Options, header.PortNumber);
            printf("L'header di richiesta di signup/in è %s\n", headerBuffer);

            ret = send(sv_communicate, (void *)headerBuffer, HEADER_LEN, 0);
            if (ret < 0)
                  printf("Non sono riuscito a mandare l'header\n");

            printf("send buffer -->%s\n", sendbuffer);
            ret = send(sv_communicate, (void *)sendbuffer, strlen(sendbuffer), 0);
            if (ret < 0)
                  printf("Non sono riuscito a mandare il buffer\n");

            // gestisco la risposta del server
            ret = recv(sv_communicate, (void *)headerBuffer, HEADER_LEN, 0);
            sscanf(headerBuffer, "%c%7s%5s", &header.RequestType, header.Options, header.PortNumber);

            if (header.RequestType == 'A')
            {
                  // il server mi ha risposto in merito ad una richiesta di signup
                  // controllo il campo option e vedo
                  if (strcmp(header.Options, "first") == 0)
                        printf("Client registrato correttamente al servizio\n");
                  else
                        printf("Client gia registrato al servizio\n");
            }
            else
            {
                  // controllo messaggio di login al server
                  // se non precedentemente registrato allora chiedo di registrarsi
                  if (strcmp(header.Options, "ok") == 0)
                  {
                        printf("Utente loggato\n");
                        break;
                  }
            }
      }

      // STAMPO I COMANDI DISPONIBILI DOPO IL LOGIN
      printf("        ======== COMANDI DEVICE ========\n");
      printf("I comandi disponibili per il client sono i seguenti:\n");
      printf("1-->hanging: Mostra a video i client che mi hanno mandato messaggi mentre ero offline.\n");
      printf("2-->show <username>: Riceve i messaggi pendenti dall'utente <username> \n");
      printf("3-->chat <username>: Avvia una chat con il client <username> dove viene mostrata la cronologia dei messaggi precedenti.\n");
      printf("4-->share <file_name>: Permette di condividere con l'utente/i con cui si sta chattando un file presente nella current directory.\n");
      printf("5-->out: Il client disconnette dal servizio.\n");

      fflush(stdin);
      // GESTIONE DEGLI INPUT DA STDIN E DEI SOCKET TRAMITE SELECT
      for (;;)
      {
            // ri-inizalizzo la lista dei descrittori in lettura
            readfds = master;
            printf("sto per chiamare select\n");
            select(fdmax + 1, &readfds, NULL, NULL, NULL);
            for (int i = 0; i <= fdmax; i++)
            {
                  // Devo ciclare fra i descrittori per servire quelli pronti
                  if (FD_ISSET(i, &readfds))
                  {
                        // Ho trovato un descrittore pronto
                        // Controllo il tipo del descrittore
                        if (i == STDIN)
                        {
                              // HO UN NUOVO COMANDO NELLO STDIN
                              struct clientcmd cmd;
                              char inputstring[1024];

                              memset(&cmd, 0, sizeof(cmd));
                              if (fgets(inputstring, 1024 - 1, stdin) == NULL)
                                    perror("Errore in lettura del comando\n");
                              fflush(stdin);

                              if (isChatting == 0)
                              {
                                    // Allora vuol dire che prima ho eseguito il comando chat <username>
                                    // quindi sono dentro la chat, ogni cosa che scrivo verrà inviata al/i destinatario/i
                                    // ---------- COMANDI DELLA CHAT -------------------

                                    if (inputstring[0] != '/')
                                    {
                                          // allora input string è un messaggio
                                          if (isDestOnline == 0)
                                          {
                                                // mando il messaggio al client che corrisponde al socket cl_socket
                                                ret = send(cl_socket, (void *)inputstring, strlen(inputstring), 0);
                                                printf("Sto inviando %d byte al destinatario che è connesso sulla porta %s\n", ret, portChat);
                                          }
                                          else
                                          {
                                                // devo mandare il messaggio al server perchè il destinatario è offline
                                                // devo incapsulare il messaggio dentro l'header
                                                struct msg_header headerChat;
                                                char packet[4096] = "";
                                                char premessage[1024] = "";
                                                // INVIO HEADER
                                                make_header(&headerChat, 'E', "tosend", portChat, HEADER_LEN);
                                                memset(headerChat_string, 0, sizeof(headerChat_string));
                                                sprintf(headerChat_string, "%c%7s%5s", headerChat.RequestType, headerChat.Options, headerChat.PortNumber);
                                                ret = send(sv_communicate, (void *)headerChat_string, HEADER_LEN, 0);
                                                // INVIO DEST E RECEIVER
                                                sprintf(premessage, "%s %s %s", my_credentials.Username, destUsername, inputstring);
                                                printf("sto inviando il premessage che contiene: %s\n", premessage);
                                                ret = send(sv_communicate, (void *)premessage, strlen(my_credentials.Username) + strlen(destUsername) + strlen(inputstring) + 1, 0);
                                          }
                                    }
                                    else
                                    {
                                          // ------------ COMANDI \q + INVIO --------------------
                                          isChatting = -1;
                                          break;
                                    }
                              }
                              else
                              {
                                    // ----------- COMANDI DEL MENU ----------------------
                                    fflush(stdin);
                                    sscanf(inputstring, "%20s %50s", cmd.Command, cmd.Argument1);
                                    if (check(cmd.Command) == -1)
                                    {
                                          printf("Comando non valido:\n");
                                          break; // Ho passato un comando non valido
                                    }

                                    // ------------ SWITCHING DEI COMANDI ----------------

                                    switch (cmd.Command[0])
                                    {
                                    case 's':
                                    {
                                          // comando show username
                                          // io devo mandare al server una richiesta con un header del tipo "richiesta dei messaggi" dopo di che devo mandare un messaggio dove dico il mio username e l'username del tizio di cui voglio ricevere i messaggi
                                          struct msg_header header;
                                          char headerstring[HEADER_LEN];
                                          int msglen;
                                          int nmsg;

                                          make_header(&header, 'D', "toreq", "0000", HEADER_LEN);
                                          // spedisco l'header
                                          sprintf(headerstring, "%c%7s%5s", header.RequestType, header.Options, header.PortNumber);
                                          ret = send(sv_communicate, (void *)headerstring, HEADER_LEN, 0);
                                          // devo mandare mittente e destinatario della richiesta
                                          msglen = strlen(my_credentials.Username) + strlen(cmd.Argument1);
                                          sprintf(buffer, "%s %s", my_credentials.Username, cmd.Argument1);
                                          ret = send(sv_communicate, (void *)buffer, msglen, 0);
                                          // ora mi aspetto il numero di messaggi da leggere dal server
                                    }
                                    break;

                                    case 'h':
                                    {
                                          // Comando hanging
                                          printf("primo comando in lista\n");
                                    }
                                    break;

                                    case 'o':
                                    {
                                          // comando out

                                          // chiudo le comunicazioni con tutti i socket
                                          close(sv_communicate);

                                          printList(active_sockets_list_head);
                                          close_communications(&active_sockets_list_head);

                                          exit(1);
                                    }
                                    break;

                                    case 'c':
                                    {
                                          // comando chat username
                                          struct msg_header header;
                                          char optionString[8] = "NULLO";
                                          char sendbuffer[1024];

                                          // controllo se possiedo il destinatario nella mia rubrica personale

                                          if (handlerFriends(my_credentials.Username, cmd.Argument1) == -1)
                                          {
                                                printf("Non ho il destinatario nella rubrica, non posso inziare la chat:\n");
                                                break;
                                          }

                                          // richiesta di chat al server per vedere se il destinatario è online
                                          strcpy(destUsername, cmd.Argument1);
                                          make_header(&header, 'C', optionString, "0000", HEADER_LEN);
                                          sprintf(sendbuffer, "%c%8s%5s%s", header.RequestType, header.Options, header.PortNumber, cmd.Argument1);
                                          printf("sto mandando la richiesta di chat che è: %s \n", sendbuffer);

                                          ret = send(sv_communicate, (void *)sendbuffer, 56, 0);
                                          if (ret < 0)
                                                perror("Non sono riuscito ad avviare una chat con l'user\n");

                                          // risposta del server
                                          memset(&buffer, 0, sizeof(buffer));
                                          ret = recv(sv_communicate, (void *)buffer, HEADER_LEN, 0);
                                          if (ret < 0)
                                                printf("Il server non mi ha detto nulla\n");

                                          sscanf(buffer, "%c%8s%5s", &header.RequestType, header.Options, header.PortNumber);
                                          printf("il server mi ha mandato un header con la porta del dest che è la seguente\nPorta: %s\n", header.PortNumber);
                                          strcpy(portChat, header.PortNumber);

                                          // destinatario online
                                          if (strcmp("ON", header.Options) == 0)
                                          {
                                                // creazione indirizzo e socket e connessione al destinatario
                                                memset(&cl_addr, 0, sizeof(cl_addr));

                                                cl_addr.sin_family = AF_INET;
                                                cl_addr.sin_port = htons(atoi(portChat));
                                                inet_pton(AF_INET, "127.0.0.1", &cl_listen_addr.sin_addr);
                                                cl_socket = socket(AF_INET, SOCK_STREAM, 0);

                                                ret = connect(cl_socket, (struct sockaddr *)&cl_addr, sizeof(cl_addr));
                                                if (ret < 0)
                                                      printf("Non sono riuscito a iniziare la chat con il destinatario\n");

                                                // aggiorno descriptor list
                                                FD_SET(cl_socket, &master);
                                                fdmax = (cl_socket > fdmax)? cl_socket : fdmax;

                                                isDestOnline = 0;
                                                // gestione lista socket attivi
                                                pushUser(&active_sockets_list_head, cmd.Argument1, cl_socket);
                                          }
                                          isChatting = 0;
                                          printf("=========== CHAT ===========\n");
                                          printf("Ogni messaggio che viene digitato su terminale e seguito dal tasto INVIO viene inviato al destinatario della chat\n");
                                    }
                                    }
                              }
                        }
                        else
                        {
                              // SOCKET DI LISTEN, COMUNICAZIONE
                              if (i == listener)
                              {
                                    int addrlen = sizeof(gp_addr);

                                    communicate = accept(i, (struct sockaddr *)&gp_addr, (socklen_t *)&addrlen);
                                    fdmax = (communicate > fdmax) ? communicate : fdmax;
                                    FD_SET(communicate, &master);
                                    pushUser(&active_sockets_list_head, "non_saved_name", i);

                                    printf("Un client sta provando a connettersi\n");
                              }
                              else
                              {
                                    char bufferChatMessage[1024] = "";
                                    ret = recv(i, (void *)bufferChatMessage, 1024, 0);

                                    if (ret == 0)
                                    {
                                          char User_logged_out[50];
                                          // gestione disconnesioni altri clients
                                          printf("Connessione chiusa con il socket: %d\n", i);

                                          // check se è l'utente con cui sto chattando
                                          isDestOnline = (i == cl_socket) ? -1 : 0;
                                          FD_CLR(i, &master);
                                          close(i);

                                          // gestione lista
                                          deleteUser(&active_sockets_list_head, i, User_logged_out);

                                          continue;
                                    }

                                    printf("Ho ricevuto %d byte da un altro utente ---> messaggio: %s\n", ret, bufferChatMessage);
                              }
                        }
                  }
            }
      }
}