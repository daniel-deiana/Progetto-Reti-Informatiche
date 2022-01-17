#include "myHeader.h"
// ---------- CODICE SERVER ----------
int main(int argc, const char **argv)
{

      struct sockaddr_in server_addr, client_addr;
      struct Credentials MyCredentials, cl_credentials;
      struct msg sv_message;

      // lista utenti online
      struct clientList *head = NULL;

      int Listener, communicate, ret, msglen, addrlen, fdmax, value;
      int client_ret;

      int Port;
      char buffer[1024 * 4 + HEADER_LEN];
      char ServerPort[5] = "4242";
      char Command[1024];
      char Options[8];

      fd_set master, readfds;
      FILE *LogPointer, *ChatBuffer, *HistoryPointer;

      // parsing degli argomenti da linea di comando
      if (argc != 2)
      {
            perror("Sto passando piu di un argomento in linea di comando");
            exit(1);
      }

      // Assegno il valore passato a ServerPort
      strcpy(ServerPort, argv[1]);
      printf("La porta selezionata è %s \n", ServerPort);

      // Stampo la parte iniziale
      printf("**************SERVER STARTED**************\n");
      printf("Digita un comando: \n\n");
      printf("1--> HELP: Mostra una breve descrizione dei comandi a schermo\n2--> LIST: Mostro la lista degli utenti connessi\n3--> ESC: Chiusura del Server\n");

      memset(&cl_credentials, 0, sizeof(MyCredentials));
      memset(&MyCredentials, 0, sizeof(MyCredentials));
      FD_ZERO(&master);
      FD_ZERO(&readfds);
      FD_SET(0, &master);

      // Devo inizializzare l'indirizzo del server
      memset(&server_addr, 0, sizeof(server_addr));
      server_addr.sin_family = AF_INET;
      server_addr.sin_port = htons(4242);
      inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr); // Metto l'indirizzo del localhost

      // Apro il socket di Listener del server, le comunicazioni fra client e server seguono il protoccolo di trasposto TCP
      Listener = socket(AF_INET, SOCK_STREAM, 0);
      ret = bind(Listener, (struct sockaddr *)&server_addr, sizeof(server_addr));
      ret = listen(Listener, 50); // Metto una coda di 50 possibili richieste di connesione al server

      FD_SET(Listener, &master);
      fdmax = Listener;

      // Gestione degli input con la select()
      for (;;)
      {
            readfds = master;
            fflush(stdin);
            select(fdmax + 1, &readfds, NULL, NULL, NULL);

            for (int i = 0; i <= fdmax; i++)
            {
                  // ho trovato un descrittor
                  if (FD_ISSET(i, &readfds))
                  {
                        // Qualcuno ha digitato un comando da terminale
                        if (i == STDIN)
                        {
                              // Check della correttezza del comando sullo stdin
                              fscanf(stdin, "%s", Command);
                              if (strlen(Command) > 1)
                              {
                                    printf("Comando non esistente");
                                    break;
                              }
                              if (Command[0] < '1' || Command[0] > '3')
                              {
                                    printf("Hai inserito un indice relativo ad un comando non esistente/valido\n");
                                    break;
                              }
                              // Ora devo gestire i comandi che il server mette a disposizione
                              switch (Command[0])
                              {
                              case '1':
                                    // Stampo una breve descrizione dei comandi
                                    break;
                              case '2':
                                    // Devo stampare la lista dei dispositivi connessi al server con numero di messs bufferizzati e timestamp del mess più recente
                                    break;
                              case '3':
                                    // Chiudo il server
                                    printf("***********CHIUSURA DEL SERVER***********");
                                    close(Listener);
                                    exit(1);
                                    break;
                              }
                        }
                        else
                        {
                              if (i == Listener)
                              {
                                    // socket di listen
            
                                    addrlen = sizeof(client_addr);
                                    communicate = accept(i, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen);
                                    fdmax = (communicate > fdmax) ? communicate : fdmax;
                                    FD_SET(communicate, &master);                                   
                                    printf("Accettata richiesta di connesione da un client: \n");
                              }
                              else
                              {
                                    // Variabili per la ricezione del messaggio dal client
            
                                    int client_ret;
                                    struct msg_header masterHeader;      // master header di ogni richiesta che arriva al server
                                    struct msg_header gpHeader;          // header che il server usa per inviare/ricevere dentro le richieste
                                    char masterHeader_string[1024] = ""; // stringa su cui ricevo il master header
                                    char gpHeader_string[1024] = "";     // stringa su cui ricevo il general purpose header
                                    char portString[5];                  // stringa che uso per conversione da int a char* del numero di porta

                                    
                                    // client mi sta inviando l'header
                                    if (ricevi_header(i,&masterHeader) < 0 )
                                    {
                                          char logout_user[50];

                                          close(i);
                                          FD_CLR(i, &master);

                                          rimuovi_utente(&head, i, logout_user);
                                          stampa_lista_utenti(head);
                                    
                                          logout(logout_user);
                                          stampa_history_utenti();
                  
                                          printf("Ho chiuso la comunicazione con il socket: %d\n", i);
                                          continue;
                                    }
                 
                                    printf("Il tipo della richiesta è: %c\n", masterHeader.RequestType);
                                    printf("Il contenuto del campo options è: %s\n", masterHeader.Options);
                                    printf("Il contenuto del campo PortNumber è: %s\n", masterHeader.PortNumber);

                                    // pulizia buffer messaggi
                                    memset(buffer, 0, sizeof(buffer));

                                    switch (masterHeader.RequestType)
                                    {
                                    case 'A':
                                    {
                                          // ================ REGISTRAZIONE ================

                                          struct HistoryRecord record;
                                          
                                          ricevi_messaggio(buffer,i);

                                          printf("Sto per registrare un utente che mi ha passato il seguente buffer: %s\n", buffer);
                                    
                                          sscanf(buffer, "%s %s", MyCredentials.Username, MyCredentials.Password);

                                          if (isClientRegistered(MyCredentials.Username) == 0)
                                                invia_header(i,'A',"before","0000");
                                          else
                                          {
                                                invia_header(i,'A',"first","0000");

                                                // gestione log e history utenti
                                                LogPointer = fopen("Log.txt", "ab");
                                                fwrite(&MyCredentials, sizeof(MyCredentials), 1, LogPointer);
                                                fclose(LogPointer);
                                                
                                                HistoryPointer = fopen("Client_History.txt", "ab"); // modalità append per non sovrascrivere i record precedenti
                                                
                                                strcpy(record.Username, MyCredentials.Username);
                                                record.Port = 0;
                                                record.timestamp_in = 0;
                                                record.timestamp_out = 0;
                                                fwrite(&record, sizeof(struct HistoryRecord), 1, HistoryPointer);
                                                fclose(HistoryPointer);
                                          }
                                    }
                                    break;

                                    case 'B':
                                    { 
                                          ricevi_messaggio(buffer,i);
                                          printf("SONO NEL CASO B: CREDENZIALI RICEVUTE -> %s\n",buffer);
                                          
                        
                                          sscanf(buffer, "%s %s", cl_credentials.Username, cl_credentials.Password);

                                          if (LoginCheck(LogPointer, &cl_credentials, sizeof(cl_credentials)) == 0)
                                          {
                                                printf("Sto mandando l'ack positivo\n");

                                                invia_header(i, 'B', "ok","8000");
                                                
                                                if (historyUpdateLogin(HistoryPointer, cl_credentials.Username, masterHeader.PortNumber) == 0)
                                                      printf("Sono riuscito ad aggiornare con successo i campi history di un utente\n");
                                                
                                                stampa_history_utenti();

                                                // gestione lista utenti online
                                                inserisci_utente(&head, cl_credentials.Username, i);
                                                stampa_lista_utenti(head);
                                          }
                                          else
                                          {
                                                printf("Non ho trovato le credenziali nel registro del server\n");
                                                invia_header(i,'B',"noreg","8000");
                                          }
                                    }
                                    break;

                                    case 'C':
                                    {
                                          ricevi_messaggio(buffer, i);
                                          printf("Ho ricevuto la richiesta di una chat con l'utente <%s>\n", buffer);

                                          Port = isOnline(buffer);
                                          if (Port == 0)
                                                printf("Port sta a zero\n");
                                          sprintf(portString, "%d", Port); // converto da intero a stringa per passare la porta nell'header
                                          if (Port == -1)
                                                invia_header(i, 'C' , "OFF", portString);
                                          else
                                                invia_header(i,'C',"ON",portString);

                                          printf("Ho inviato al client informazioni sul destinatario\n");
                                    }
                                    break;

                                    case 'E':
                                    {
                                          struct bufferedMessage tobuffer;
                                          int numbyte;
                                          char messagebuffer[4096 + 100];
                                          memset(&tobuffer, 0, sizeof(tobuffer));

                                          // device mi sta mandando il suo nome e il nome del destinatario
                                          memset(messagebuffer, 0, sizeof(messagebuffer));
                                          ricevi_messaggio(messagebuffer, i);

                                          printf("prima della de-serializzazione: %s\n", messagebuffer);

                                          sscanf(messagebuffer, "%s %s %[^\t]", tobuffer.sender, tobuffer.receiver, tobuffer.message);
                                          printf("ho ricevuto %d byte, sender: %s\nreceiver:%s \n", numbyte, tobuffer.sender, tobuffer.receiver);
                                          printf("Il messaggio che ho ricevuto dal client è:%s \n", tobuffer.message);
                                          
                                          bufferizza_msg(&tobuffer);
                                          stampa_msg_bufferizzati();
                                    }
                                    break;

                                    case 'D':
                                    {     
                                          char userTarget[50];     // argomento della show eseguita dal client
                                          char userRequesting[50]; // client che ha fatto la richiesta
                                          
                                          ricevi_messaggio(buffer, i);
                                          sscanf(buffer, "%s %s", userRequesting, userTarget);
                                          invia_messaggi_pendenti(userRequesting, userTarget, i);                                          
                                    }
                                    break;
                                    }
                              } 
                        }       
                  }             
            }                   
      }                         
}







