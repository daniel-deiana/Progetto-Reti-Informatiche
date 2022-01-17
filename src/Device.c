#include "myHeader.h"
// =========== CODICE CLIENT =========
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


      // faccio la connect al server
      if (connect(sv_communicate, (struct sockaddr *)&server_addr, sizeof(server_addr)))
            exit(1);

      // comandi <signup> e <in>
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
                  // make_header(&header, 'A', "reg", Port, HEADER_LEN);
                  invia_header(sv_communicate,'A',"reg",Port);
                  sprintf(sendbuffer, "%s %s", first, second);
            }
            else if (strcmp(cmd, "in") == 0)
            {
                  // make_header(&header, 'B', "login", Port, HEADER_LEN);
                  invia_header(sv_communicate,'B',"login",Port);
                  strcpy(my_credentials.Username, second);
                  strcpy(my_credentials.Password, third);
                  sprintf(sendbuffer, "%s %s", second, third);
            }

            // invio credenziali utente
            printf("prima di invia_msg: sto mandando: %s", sendbuffer);
            invia_messaggio(sendbuffer, sv_communicate);
            
            // risposta del server
            ricevi_header(sv_communicate,&header);


            if (header.RequestType == 'A')
            {
                  // il server mi ha risposto in merito ad una richiesta di signup
                  if (strcmp(header.Options, "first") == 0)
                        printf("Client registrato correttamente al servizio\n");
                  else
                        printf("Client gia registrato al servizio\n");
            }
            else
            {
                  // controllo messaggio di login al server
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
                                    // ------------------ comandi chat ----------------------

                                    if (inputstring[0] != '/')
                                    {
                                          // allora input string è un messaggio
                                          if (isDestOnline == 0)
                                          {
                                                // invio messaggio direttamente al dest
                                                invia_messaggio(inputstring,cl_socket);
                                          }
                                          else
                                          {
                                                // devo mandare il messaggio al server perchè il destinatario è offline
                                                char premessage[1024] = "";
                                                
                                                invia_header(sv_communicate,'E',"tosend",portChat);
                                                
                                                // invio sender e receiver del msg
                                                sprintf(premessage, "%s %s %s", my_credentials.Username, destUsername, inputstring);
                                                printf("sto inviando il premessage che contiene: %s\n", premessage);
                                                invia_messaggio(premessage,sv_communicate);
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
                                          int num_msg;
                        
                                          invia_header(sv_communicate,'D',"toreq","0000");                  
                                          
                                          sprintf(buffer, "%s %s", my_credentials.Username, cmd.Argument1);
                                          printf("il buffer di invio della show è %s\n", buffer);
                                          
                                          invia_messaggio(buffer,sv_communicate);
                                         
                                          ret = recv(sv_communicate, (void*)&num_msg, sizeof(int), 0);
                                          printf("il server mi ha detto che %s mi ha inviato %d messsaggi\n", cmd.Argument1, num_msg);

                                          for(int i = 0; i < num_msg; i++)
                                                {
                                                      char recvbuffer[4096] = "";
                                                      ricevi_messaggio(recvbuffer,sv_communicate);
                                                      printf("messaggio pendente ricevuto: %s", recvbuffer);
                                                }
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

                                          stampa_lista_utenti(active_sockets_list_head);
                                          chiudi_connesioni_attive(&active_sockets_list_head);
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

                        
                                          invia_header(sv_communicate,'C',optionString,"0000");

                                          strcpy(destUsername, cmd.Argument1);                                    
                                          sprintf(sendbuffer, "%s",cmd.Argument1);
                                          
                                          // invio destinatario della chat al server
                                          invia_messaggio(sendbuffer,sv_communicate);                                     
                        
                                          printf("sto mandando la richiesta di chat che è: %s \n", sendbuffer);
                                          
                                          
                                          memset(&buffer, 0, sizeof(buffer));
                                          
                                          ricevi_header(sv_communicate, &header);


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
                                                inserisci_utente(&active_sockets_list_head, cmd.Argument1, cl_socket);
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
                                    inserisci_utente(&active_sockets_list_head, "non_saved_name", i);

                                    printf("Un client sta provando a connettersi\n");
                              }
                              else
                              {
                                    char bufferChatMessage[1024] = "";
                                   
                                     ricevi_messaggio(bufferChatMessage,i);
                                    
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
                                          rimuovi_utente(&active_sockets_list_head, i, User_logged_out);

                                          continue;
                                    }

                                    printf("Ho ricevuto %d byte da un altro utente ---> messaggio: %s\n", ret, bufferChatMessage);
                              }
                        }
                  }
            }
      }
}