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
#include "myHeader.h"

// ---------- CODICE SERVER ----------
int main(int argc, const char **argv)
{

    struct sockaddr_in server_addr, client_addr;
    struct Credentials MyCredentials, cl_credentials;
    struct msg sv_message;

    int Listener, communicate, ret, msglen, addrlen, fdmax, value;
    int client_ret;

    int Port;
    char buffer[1024 * 4 + HEADER_LEN];
    char ServerPort[5] = "4242";
    char Command[1024];
    char Options[8];

    fd_set master, readfds;
    FILE *LogPointer, *ChatBuffer, *HistoryPointer;

    // Come prima cosa devo prendere dallo stdin un comando che faccia partire il server
    // Il formato del comando è del tipo ./serv [port] dove port è la porta del server

    // argc deve essere 2 perchè il programma all'avvio prende 2 ingressi in linea di comando
    // il primo è il nome del programma mentre il secondo è il valore della porta passato in ingresso
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
                    // Se sono qui vuol dire fondamentalmente ho un socket pronto in lettura (dei client hanno quindi inviato dei messaggi al server)
                    // Devo discriminare se i client mi hanno contattato sul socket di comunicazione oppure su quello di ascolto
                    if (i == Listener)
                    {
                        // SOCKET DI ASCOLTO
                        //  Sono stato contattato sul socket di ascolto, devo aprire una nuova connessione con i clients
                        addrlen = sizeof(client_addr);
                        communicate = accept(i, (struct sockaddr *)&client_addr, (socklen_t *)&addrlen);
                        fdmax = (communicate > fdmax) ? communicate : fdmax;
                        FD_SET(communicate, &master);
                        // quando accetto una connessione aggiungo un elemento alla lista clients
                        // dato che quando l'utente farà logout dovrò settare il ts di uscita devo avere una lista dove salvo le coppie {socket,username}

                        // addtolist
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

                        // RICEZIONE DEL MESSAGGIO (HEADER)
                        // ispezionamento dell'header
                        ret = recv(i, (void *)masterHeader_string, HEADER_LEN, 0);
                        sscanf(masterHeader_string, "%c%8s%5s", &masterHeader.RequestType, masterHeader.Options, masterHeader.PortNumber);
                        printf("La stringa che ho ricevuto dal client è %s\n", masterHeader_string);
                        printf("Il tipo della richiesta è: %c\n", masterHeader.RequestType);
                        printf("Il contenuto del campo options è: %s\n", masterHeader.Options);
                        printf("Il contenuto del campo PortNumber è: %s\n", masterHeader.PortNumber);

                        // SOCKET DI COMUNICAZIONE
                        // conversione del messaggio da stringa verso struttura ClientMessage

                        /*
                        ==================GESTIONE RICHIESTE========================
                        In questa parte vengono gestite le richieste dei clients verso il server
                        I casi in cui un client contatta il server sono i seguenti e viene definita una struttura del messaggio
                        dalla struttura ClientMessage. Segue una descrizione dei tipi di richiesta.

                            REQ_TYPE = A: Richiesta di registrazione sul server da parte del client.
                            REQ_TYPE = B: Richiesta di Login sul server da parte del client.
                            REQ_TYPE = C: Un client avvia una chat con un altro client
                        */

                        switch (masterHeader.RequestType)
                        {
                        case 'A':
                        {
                            // RICHIESTA DI REGISTRAZIONE AL SERVER
                            /* devo aprire il file degli utenti registrati e ci devo scrivere
                               nel payload trovo user e pass
                            */
                            struct HistoryRecord record;
                            ret = recv(i, (void *)buffer, 100, 0);
                            printf("Sto per registrare un utente che mi ha passato il seguente buffer: %s\n", buffer);
                            // apro il file e ci sputo dentro i dati della registrazione con il formato username password
                            sscanf(buffer, "%s %s", MyCredentials.Username, MyCredentials.Password);
                            LogPointer = fopen("Log.txt", "ab");
                            fwrite(&MyCredentials, sizeof(MyCredentials), 1, LogPointer);
                            fclose(LogPointer);
                            // Devo anche aggiungere un record nel file client_hystory dato che deve esserne presente uno per ogni utente che usa il servizio
                            HistoryPointer = fopen("Client_History.txt", "ab"); // modalità append per non sovrascrivere i record precedenti
                            // inizializzo la struttura dati sul registro che poi verra sovrascritta dal primo login in poi
                            strcpy(record.Username, MyCredentials.Username);
                            record.Port = 0;
                            record.timestamp_in = 0;
                            record.timestamp_out = 0;
                            fwrite(&record, sizeof(struct HistoryRecord), 1, HistoryPointer);
                            // chiudo il file
                            fclose(HistoryPointer);
                        }
                        break;
                        case 'B':
                            // RICHIESTA DI LOGIN AL SERVER
                            // RICEZIONE DEL MESSAGGIO DI LOGIN
                            ret = recv(i, (void *)buffer, 100, 0);
                            printf("Ho ricevuto %d caratteri\n", ret);
                            buffer[ret] = '\0';
                            printf("%s\n", buffer);
                            // AUTENTICAZIONE DELLE CREDENZIALI PASSATE DAL CLIENT
                            sscanf(buffer, "%s %s", cl_credentials.Username, cl_credentials.Password);
                            // Devo aprire il file di log degli utenti registrati e devo vedere se si è registrato in precedenza
                            value = LoginCheck(LogPointer, &cl_credentials, sizeof(cl_credentials));
                            if (value == 0)
                            {
                                printf("Sto mandando l'ack positivo\n");
                                make_header(&gpHeader, 'C', "ACK", "8000", HEADER_LEN);
                                // Registrazione su file del momento in cui il client si è loggato
                                if (historyUpdateLogin(HistoryPointer, cl_credentials.Username, masterHeader.PortNumber) == 0)
                                    printf("Sono riuscito ad aggiornare con successo i campi history di un utente\n");
                                printHistory();
                            }
                            else
                            {
                                printf("Non ho trovato le credenziali nel registro del server\n");
                                make_header(&gpHeader, 'C', "NAK", "8000", HEADER_LEN);
                            }
                            printf("Header ha il valore del tipo: %c\n", gpHeader.RequestType);
                            sprintf(gpHeader_string, "%c%7s%5s", gpHeader.RequestType, gpHeader.Options, gpHeader.PortNumber);
                            // TODO: METTERE SEMPRE \n ALLA FINE DI OGNI PRINTF
                            printf("Sto cercando di mandare la stringa di ack che è la seguente: %s\n", gpHeader_string);
                            client_ret = send(i, (void *)gpHeader_string, HEADER_LEN, 0);
                            memset(buffer, 0, sizeof(buffer));
                            break;
                        case 'C':
                            // Un client ha chiesto di avviare una chat con un altro utente del servizio
                            // Devo controllare se il destinatario sia registrato al servizio
                            // ricezione del payload (so gia che se la richiesta è del tipo C la dim massima è 50)
                            ret = recv(i, (void *)buffer, 50, 0);
                            printf("Ho ricevuto la richiesta di una chat con l'utente <%s>\n", buffer);
                            // deserializzo il buffer
                            // devo cercare l'usernmame passato in buffer nel file history, se trovo una corrispondenza
                            // devo vedere se il timestamp di logout è nullo, se lo è l'utente destinatario è online
                            Port = isOnline(buffer);
                            if (Port == 0)
                                printf("Port sta a zero\n");
                            sprintf(portString, "%d", Port); // converto da intero a stringa per passare la porta nell'header
                            if (Port == -1)
                                // devo dire al client che l'utente destinatario non è online
                                make_header(&gpHeader, 'C', "OFF", portString, HEADER_LEN);
                            else
                                // ho trovato il destinatario che l'utente stava cercando
                                make_header(&gpHeader, 'C', "ON", portString, HEADER_LEN);

                            // serializzazione dell'header
                            sprintf(gpHeader_string, "%c%8s%5s", gpHeader.RequestType, gpHeader.Options, gpHeader.PortNumber);
                            client_ret = send(i, (void *)gpHeader_string, HEADER_LEN, 0);
                            if (client_ret < 0)
                                printf("Non sono riuscito a comunicare al client se il destinatario fosse online\n");

                            printf("Ho inviato al client informazioni sul destinatario\n");
                            break;
                        case 'E':
                        {
                            struct bufferedMessage tobuffer;
                            int numbyte;
                            char messagebuffer[4096 + 100];
                            memset(&tobuffer, 0, sizeof(tobuffer));
                            /*
                            Un client sta mandando un messaggio ad un destinatario che ha trovato OFFLINE, quindi se mi invia un pacchetto
                            con header del TIPO "E" vuol dire che devo bufferizzare un messaggio nel registro del server.
                            devo però capire chi è il destinatario e chi me lo sta mandando
                            */
                            // decido di progettare il protocollo in questo modo: se ricevo un header del tipo E allora vuol dire che i primi 100 byte
                            // del payload contengono username del sender e del receiver
                            memset(messagebuffer, 0, sizeof(messagebuffer));
                            numbyte = recv(i, (void *)messagebuffer, 1024, 0);

                            printf("prima della de-serializzazione: %s\n", messagebuffer);
                            // deserializzazione contenuto msgbuffer che conterrà il nome del sender e del receiver ed il messaggio
                            sscanf(messagebuffer, "%s %s %[^\t]", tobuffer.sender, tobuffer.receiver, tobuffer.message);
                            printf("ho ricevuto %d byte, sender: %s\nreceiver:%s \n", numbyte, tobuffer.sender, tobuffer.receiver);
                            printf("Il messaggio che ho ricevuto dal client è:%s \n", tobuffer.message);
                            printfBuffer();
                            bufferWrite(&tobuffer);
                        }
                        break;
                        case 'D':
                        {
                            int msgnum; // numero di messaggi pendenti da inviare al client
                            char msgnumstring[8] = "";
                            struct bufferedMessage msg;
                            FILE *fptr;
                            // mi è arrivata una richiesta di show username
                            char userTarget[50];     // argomento della show eseguita dal client
                            char userRequesting[50]; // client che ha fatto la richiesta
                            ret = recv(i, (void *)buffer, 101, 0);
                            sscanf(buffer, "%s %s", userRequesting, userTarget);
                            // ora devo usare questi valori per cercare nel registro delle chat bufferizzate tutti i messaggi da requesting verso target pendenti
                            // posso decidere di inviare i messaggi pendenti uno ad uno e mettere nell'intestazione del messaggio il fatto che mandero x mess al destinatario
                            msgnum = countBuffered(userTarget, userRequesting); //
                            // conversione da int a stringa del numero di messaggi
                            sprintf(msgnumstring, "%d", msgnum);
                            // mando il numero di messaggi che si aspetta di ricevere
                            make_header(&gpHeader, 'D', msgnumstring, "0000", sizeof(gpHeader));
                            ret = send(i, (void *)gpHeader_string, HEADER_LEN, 0);
                            // ora devo mandare ad uno ad uno i messaggi bufferrizzati a quello che mi ha fatto la richiesta
                        }
                        break;
                        }
                        if (ret == 0)
                        {
                            close(i);
                            FD_CLR(i, &master);
                            // devo eseguire l'operazione di logout, quindi andare a cercare nel log
                            // mi prendo l'username relativo al socket andandolo a cercare nella lista
                            // setto il timestamp di logout nel file clienthistory.txt relativo all'username che ho trovato

                            // remove from list
                            printf("Ho chiuso la comunicazione con il socket: %d\n", i);
                        }
                    } // Fine listen/Comunicate
                }     // FINE GESTIONE COMUNICAZIONI/STDIN
            }         // FINE IS_SET
        }             // FINE for1
    }                 // FINE for0
} // FINE main()
