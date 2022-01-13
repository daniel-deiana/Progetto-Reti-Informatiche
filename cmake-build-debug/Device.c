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
#define LOGIN_MSG_SIZE 100
#define USERNAME_LEN 50
// ---------- STRUTTURE DATI ----------

struct clientcmd
{
    char Command[20];
    char Argument1[50]; // qua prendo un username oppure il nome di un file (viene quindi definita la lunghezza massima che accetto)
};

struct msg_header
{
    char RequestType;
    char Options[8];
    char PortNumber[5];
};
struct msg
{
    struct msg_header Header;
    char Payload[1024 * 4];
};
struct Credentials
{
    char Username[50];
    char Password[50];
};

// funzione che fa il parsing del comando passato in input e controlla la sua validità
int check(char *cmd)
{
    if ((strcmp("share", cmd) == 0) || (strcmp("hanging", cmd) == 0) || (strcmp("show", cmd) == 0) || (strcmp("chat", cmd) == 0) || (strcmp("out", cmd) == 0))
        return 0;
    else
        return -1;
}
// funzione che prende in ingresso una struttura msg_header e la inizializza con i valori passati in ingresso
void make_header(struct msg_header *header, char Type, char *Options, char *PortNumber, int size)
{
    memset(header, 0, size);
    header->RequestType = Type;
    strcpy(header->Options, Options);
    strcpy(header->PortNumber, PortNumber);
    return;
}
// questa funzione prende in ingresso un puntatore a file e controlla la presenza di un utente nella rubrica del client
int check_friend(FILE *fileptr, char *dest)
{
    char Username[50] = "";
    int i = -1;
    fileptr = fopen("Clients.txt", "rb");
    while (fread(Username, USERNAME_LEN, 1, fileptr))
        if (strcmp(Username, dest) == 0)
        {
            i = 0;
            break;
        }
    fclose(fileptr);
    return i;
}

// =========== CODICE CLIENT ===========
int main(int argc, const char **argv)
{

    // RUBRICA SEMPLIFICATA
    char **friends;

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

    // CREAZIONE SOCKET DI ASCOLTO PER COMUNICAZIONI CLIENT-CLIENT
    listener = socket(AF_INET, SOCK_STREAM, 0);
    bind(listener, (struct sockaddr *)&cl_listen_addr, sizeof(cl_listen_addr));
    listen(listener, 50);

    FD_SET(listener, &master);
    fdmax = listener;

    /* ============= FASE INIZIALE =============
    - Viene controllato se è presente un record di username e password all'interno del file di registrazione
    - In caso contrario l'utente inserisce l'username e password e invia una richiesta di registrazione al server
    - Dopo la registrazione, o nel caso sia gia stata effettuata in precedenza, appaiono tutti i comandi del Device
    */

    memset(&my_credentials, 0, sizeof(struct Credentials));
    RegistrationLog = fopen("Registration.txt", "r");

    // Controllo credenziali del device
    fread(&my_credentials, sizeof(my_credentials), 1, RegistrationLog);
    fclose(RegistrationLog);

    printf("Sto chiamando il server\n");
    ret = connect(sv_communicate, (struct sockaddr *)&server_addr, sizeof(server_addr));
    if (ret < 0)
    {
        perror("Errore in fase di registrazione: Server non risponde");
        exit(1);
    }

    if ((strcmp("", my_credentials.Username) == 0) && (strcmp("", my_credentials.Password) == 0))
    {
        int msglen;
        RegistrationLog = fopen("Registration.txt", "ab");
        // Vuol dire che non ho letto nulla e che quindi devo eseguire la registrazione
        printf("=======Registrazione======\n");
        // inserimento credenziali
        printf("Inserisci un username: \n");
        scanf("%s", my_credentials.Username);
        printf("Inserisci una password: \n");
        scanf("%s", my_credentials.Password);
        // scrivo le credenziali sul file di registrazione
        // fprintf(RegistrationLog, "%s\n%s\n", my_credentials.Username, my_credentials.Password);
        fwrite(&my_credentials, sizeof(my_credentials), 1, RegistrationLog);
        printf("======Registrazione Completata======\n");
        fclose(RegistrationLog);

        // Mando messaggio di registrazione al server

        // Allora a questo punto vuol dire che il server ha accettato la richiesta di connesione
        // Devo inviare Username e Password per far si che mi riconosca quando il mio client farà login

        // DEVO INVIARE L'HEADER
        make_header(&ClientMessage.Header, 'A', "Nullo", "8000", HEADER_LEN);
        sprintf(buffer, "%c%8s%5s%s %s", ClientMessage.Header.RequestType, ClientMessage.Header.Options, ClientMessage.Header.PortNumber, my_credentials.Username, my_credentials.Password);
        ret = send(sv_communicate, (void *)&buffer, LOGIN_MSG_SIZE, 0);

        if (ret < 0)
        {
            perror("Connessione non riuscita\n");
            exit(1);
        }
        else
            printf("Connessione riuscita con successo\n");
    }
    else
    {
        printf("Utente Precedentemente registrato al servizio\n");
    }
    // COMANDO DI LOGIN

    printf("======= FASE DI LOGIN =======\n");
    fscanf(__stdinp, "%s %s %s", LogInCommand, my_credentials.Username, my_credentials.Password);

    // Sto inviando un messaggio di LOGIN, devo riempire l'header di conseguenza, quindi il campo RequestType = B
    // Nel caso di una richiesta di login option e destination port non vengono considerati e si guarda direttamente il payload
    // che in questo caso conterrà username e pass nel formato Username<spazio>Password

    if (strcmp("signup", LogInCommand) != 0)
    {
        printf("Stai inserendo un commando sbagliato: \n");
        printf("Exit:\n");
        exit(1);
    }

    printf("Mi sto connettendo in login \n");
    make_header(&ClientMessage.Header, 'B', "pi", Port, HEADER_LEN);
    sprintf(buffer, "%c%8s%5s%s %s", ClientMessage.Header.RequestType, ClientMessage.Header.Options, ClientMessage.Header.PortNumber, my_credentials.Username, my_credentials.Password);

    msglen = HEADER_LEN + strlen(my_credentials.Username) + strlen(my_credentials.Password) + 1;
    printf("Sto inviando %d byte al server:\n", msglen);
    ret = send(sv_communicate, (void *)buffer, msglen, 0);
    if (ret < 0)
    {
        perror("Non sono riuscito a inviare il messaggio\n");
        close(sv_communicate);
        exit(1);
    }

    printf("Controllo l'ack di avvenuto login\n");
    ret = recv(sv_communicate, (void *)string, HEADER_LEN, 0);

    string[ret] = '\0';
    printf("la stringa dell ack è %s\n", string);
    // DESERIALIZZAZIONE DELLA STRINGA
    sscanf(string, "%c%8s%5s", &ClientMessage.Header.RequestType, ClientMessage.Header.Options, ClientMessage.Header.PortNumber);
    printf("il mio campo options è: %s\n", ClientMessage.Header.Options);

    if (strcmp("NAK", ClientMessage.Header.Options) == 0)
    {
        printf("Non mi sono loggato: esco. . .\n");
        close(sv_communicate);
        exit(1);
    }

    // STAMPO I COMANDI DISPONIBILI DOPO IL LOGIN
    printf("Mi sono loggato\n");
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
                            // Comando hanging
                            printf("primo comando in lista\n");
                            break;
                        case 'o':
                            // Comando out
                            printf(
                                "Chiusura della connesione con il server. . .\n");
                            close(sv_communicate);
                            exit(1);
                            break;
                        case 'c':
                        {
                            // comando chat username
                            struct msg_header header;
                            char optionString[8] = "NULLO";
                            char sendbuffer[1024];

                            // controllo se possiedo il destinatario nella mia rubrica personale

                            /*
                            if (check_friend(friends, cmd.Argument1) == -1)
                            {
                                printf("Argomento passato all comando chat:
                            <%s>\n", cmd.Argument1); printf("Non ho il
                            destinatario nella rubrica, non posso inziare la
                            chat:\n"); break;
                            }
                            */
                            strcpy(destUsername, cmd.Argument1);
                            make_header(&header, 'C', optionString, "0000", HEADER_LEN);
                            sprintf(sendbuffer, "%c%8s%5s%s", header.RequestType, header.Options, header.PortNumber, cmd.Argument1);
                            // mando header del tipo richiesta chat e username destinatario nel payload
                            ret = send(sv_communicate, (void *)sendbuffer, 56, 0);
                            if (ret < 0)
                                perror("Non sono riuscito ad avviare una chat con l'user\n");
                            // ora mi aspetto una risposta dal server che mi dica se il dest è online e la sua porta Scelgo di farmi passare sempre la porta in portNumber e di farmi dire se è online nel campo options
                            memset(&buffer, 0, sizeof(buffer));
                            ret = recv(sv_communicate, (void *)buffer, HEADER_LEN, 0);
                            if (ret < 0)
                                printf("Il server non mi ha detto nulla\n");
                            // deserializzo il buffer
                            sscanf(buffer, "%c%8s%5s", &header.RequestType, header.Options, header.PortNumber);
                            printf("il server mi ha mandato un header con la porta del dest che è la seguente\nPorta: %s\n", header.PortNumber);
                            // copio il contenuto della porta nella variabile globale portChat
                            strcpy(portChat, header.PortNumber);
                            // nel campo options trovo se l'utente è online oppure no, nel campo porta la porta l'ultima porta registrata decido di passare sempre per il server

                            // qua devo aprire una connesione se il client è online creo indirizzo destinatario

                            if (strcmp("ON", header.Options) == 0)
                            {
                                // creazione indirizzo e socket
                                memset(&cl_addr, 0, sizeof(cl_addr));
                                cl_addr.sin_family = AF_INET;
                                cl_addr.sin_port = htons(1234);
                                inet_pton(AF_INET, "127.0.0.1", &cl_listen_addr.sin_addr);
                                cl_socket = socket(AF_INET, SOCK_STREAM, 0);
                                // connessione con il destinatario
                                ret = connect(cl_socket, (struct sockaddr *)&cl_addr, sizeof(cl_addr));
                                if (ret < 0)
                                    printf("Non sono riuscito a iniziare la chat con il destinatario\n");
                                isDestOnline = 0;
                                printf("dest online\n");
                            }

                            // quindi ora devo settare un flag che mi dica se sto chattando per discriminare quali comandi controllare
                            isChatting = 0;

                            // devo anche capire, quando chatto. a chi devo mandare i messaggi tra server e client
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
                        printf("Un client sta provando a connettersi\n");
                    }
                    else
                    {
                        char bufferChatMessage[1024] = "";
                        ret = recv(i, (void *)bufferChatMessage, 1024, 0);
                        printf("Ho ricevuto %d byte da un altro utente ---> messaggio: %s\n", ret, bufferChatMessage);

                        if (ret == 0)
                        {
                        }
                    }
                }
            }
        }
    }
}
