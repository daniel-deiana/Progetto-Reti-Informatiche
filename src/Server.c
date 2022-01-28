#include "utils.h"

// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////// NET SERVER /////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, const char **argv)
{

	struct sockaddr_in server_addr, client_addr;
	struct Credentials MyCredentials, cl_credentials;

	// lista gruppi
	struct des_group *group_head = NULL;
	// lista utenti online
	struct clientList *head = NULL;
	// lista richieste di notify
	struct notify_queue *notify_head = NULL;

	// gruppi
	int next_group_id = 0;

	int Listener, communicate, ret, msglen, addrlen, fdmax, value;
	int client_ret;

	int Port;
	char buffer[1024 * 4 + HEADER_LEN];
	char ServerPort[5] = "4242";
	char Command[1024];
	char Options[8];
	char timestamp_string[TIMESTAMP_LEN];

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
	printf("------------------ server online ------------------\n\n");
	printf("Comandi disponibili\n\n");
	printf("1 <help>\n2 <list>\n3 <esc>\n\n");

	memset(&cl_credentials, 0, sizeof(MyCredentials));
	memset(&MyCredentials, 0, sizeof(MyCredentials));
	FD_ZERO(&master);
	FD_ZERO(&readfds);
	FD_SET(0, &master);

	// ------------------ indirizzo server ----------------------
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(4242);
	inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

	// socket di listen
	Listener = socket(AF_INET, SOCK_STREAM, 0);
	ret = bind(Listener, (struct sockaddr *)&server_addr, sizeof(server_addr));
	ret = listen(Listener, 50); // Metto una coda di 50 possibili richieste di connesione al server

	FD_SET(Listener, &master);
	fdmax = Listener;

	for (;;)
	{
		readfds = master;
		fflush(stdin);

		// gestione input da tastiera e da sockets tramite io multiplexing
		select(fdmax + 1, &readfds, NULL, NULL, NULL);

		for (int i = 0; i <= fdmax; i++)
		{
			if (FD_ISSET(i, &readfds))
			{
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

					// ------------------ switching comandi server -------------------
					switch (Command[0])
					{
					case '1':
						// show
						break;
					case '2':
						// list
						break;
					case '3':
						// esc
						printf("------------------ chiusura del server ------------------\n");

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
						// socket di comunicazione

						struct msg_header masterHeader;
						char portString[5];

						// client mi sta inviando l'header
						if (ricevi_header(i, &masterHeader) == 0)
						{
							// mi è stato mandato un messaggio di "close()"
							char logout_user[50];

							close(i);
							FD_CLR(i, &master);

							rimuovi_utente(&head, i, logout_user);
							stampa_lista_utenti(head);

							logout(logout_user);
							stampa_history_utenti();

							// debug
							printf("Ho chiuso la comunicazione con il socket: %d\n", i);
							continue;
						}

						// debug
						printf("Il tipo della richiesta è: %c\n", masterHeader.RequestType);
						printf("Il contenuto del campo options è: %s\n", masterHeader.Options);
						printf("Il contenuto del campo PortNumber è: %s\n", masterHeader.PortNumber);

						// pulizia buffer messaggi
						memset(buffer, 0, sizeof(buffer));

						switch (masterHeader.RequestType)
						{
						case 'A':
						{
							// ----------------------- routine di registrazione ---------------------------

							struct HistoryRecord record;

							ricevi_messaggio(buffer, i);

							// debug
							printf("Sto per registrare un utente che mi ha passato il seguente buffer: %s\n", buffer);

							sscanf(buffer, "%s %s", MyCredentials.Username, MyCredentials.Password);

							if (is_client_registered(MyCredentials.Username) == 0)
								invia_header(i, 'A', "before", "0000");
							else
							{
								invia_header(i, 'A', "first", "0000");

								// gestione log e history utenti
								LogPointer = fopen("registered_clients", "ab");
								fwrite(&MyCredentials, sizeof(MyCredentials), 1, LogPointer);
								fclose(LogPointer);

								HistoryPointer = fopen("clients_history.txt", "ab"); // modalità append per non sovrascrivere i record precedenti

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
							//-------------------------- routine di login -------------------------------

							// client mi sta mandando credenziali di login
							ricevi_messaggio(buffer, i);

							sscanf(buffer, "%s %s", cl_credentials.Username, cl_credentials.Password);

							if (is_client_registered(cl_credentials.Username) == 0)
							{
								// debug
								printf("LOG: Sto mandando l'ack positivo\n");

								invia_header(i, 'B', "ok", "8000");

								pulisci_buffer(timestamp_string, sizeof(timestamp_string));
								ricevi_messaggio(timestamp_string, i);

								printf("LOG: timestamp ultima disconnessione di %s : %s \n", cl_credentials.Username, timestamp_string);

								aggiorna_history_utente(HistoryPointer, cl_credentials.Username, masterHeader.PortNumber);
								stampa_history_utenti();

								// gestione lista utenti online
								inserisci_utente(&head, cl_credentials.Username, i);
								stampa_lista_utenti(head);
							}
							else
							{
								// debug
								printf("LOG: Non ho trovato le credenziali nel registro del server\n");
								invia_header(i, 'B', "noreg", "8000");
							}
						}
						break;

						case 'C':
						{
							// ---------------------------- routine chat ------------------------------

							ricevi_messaggio(buffer, i);
							printf("LOG: Ho ricevuto la richiesta di una chat con l'utente <%s>\n", buffer);

							Port = check_username_online(buffer);
							if (Port == 0)
								printf("Port sta a zero\n");

							sprintf(portString, "%d", Port);
							if (Port == -1)
								invia_header(i, 'C', "OFF", portString);
							else
								invia_header(i, 'C', "ON", portString);

							char user_target[50];

							// mi prendo l'username dell'utente che ha avviato la chat
							pulisci_buffer(user_target, sizeof(user_target));
							username_da_socket(i, head, user_target);
							// provo a cercare se l'utente che ha iniziato la chat ha messaggi pendenti dal dest
							if (notify_dequeue(&notify_head, buffer, user_target) == 0)
							{
								pulisci_buffer(buffer, sizeof(buffer));
								strcpy(buffer, "notify_ack");
							}
							else
							{
								pulisci_buffer(buffer, sizeof(buffer));
								strcpy(buffer, "notify_nak");
							}

							invia_messaggio(buffer, i);
						}
						break;

						case 'E':
						{
							// -------------- routine messaggio pendente da salvare -----------------

							struct des_buffered_msg tobuffer;
							char messagebuffer[4096];
							int numbyte;

							memset(&tobuffer, 0, sizeof(tobuffer));

							// device mi sta mandando il suo nome e il nome del destinatario
							memset(messagebuffer, 0, sizeof(messagebuffer));
							ricevi_messaggio(messagebuffer, i);

							sscanf(messagebuffer, "%s %s %[^\t]", tobuffer.sender, tobuffer.receiver, tobuffer.message);
							printf("LOG: ho ricevuto %d byte, sender: %s\nreceiver:%s \n", numbyte, tobuffer.sender, tobuffer.receiver);
							printf("LOG: Il messaggio che ho ricevuto dal client è:%s \n", tobuffer.message);

							bufferizza_msg(&tobuffer);
							aggiorna_hanging(tobuffer.sender, tobuffer.receiver);

							stampa_msg_bufferizzati();
						}
						break;

						case 'D':
						{
							// ---------------- routine messaggi pendenti da inviare ---------------

							char buf_sender[USERNAME_LEN];	 // argomento della show eseguita dal client
							char buf_receiver[USERNAME_LEN]; // client che ha fatto la richiesta

							// ricevo il nome del sender e del receiver
							ricevi_messaggio(buffer, i);
							sscanf(buffer, "%s %s", buf_receiver, buf_sender);

							invia_messaggi_pendenti(buf_receiver, buf_sender, i);
							aggiorna_hanging(buf_receiver, buf_sender);
						}
						break;
						case 'H':
						{
							// ----------------- routine richiesta di hanging ---------------

							// ricevo il nome del "target" della hanging
							char buf_receiver[USERNAME_LEN];

							// prendo il nome del client che mi ha fatto la richiesta
							username_da_socket(i, head, buf_receiver);

							print_hanging();
							// prendo le informazioni della hanging
							send_hanging_info(buf_receiver, i);
						}
						break;
						case 'U':
						{
							// ---------------------- routine chat di gruppo -----------------------

							int porta;
							char group_name[50];

							// invio sringa che contiene username utenti online
							copia_username_utenti_online(buffer);
							invia_messaggio(buffer, i);

							// ricevo il nome dell'utente da aggiungere
							char user_to_add[USERNAME_LEN];
							pulisci_buffer(user_to_add, sizeof(user_to_add));
							ricevi_messaggio(user_to_add, i);

							// ricevo il nome del gruppo
							pulisci_buffer(group_name, sizeof(group_name));
							ricevi_messaggio(group_name, i);

							// devo capire se l'utente vuole creare un gruppo oppure aggiungere un utente ad un gruppo gia esistente
							if (strcmp(masterHeader.Options, "create") == 0)
							{
								// ----------- routine creazione gruppo -------------

								char user_creator[USERNAME_LEN];
								// mi prendo il nome del socket che mi ha contattato
								pulisci_buffer(user_creator, sizeof(user_creator));
								username_da_socket(i, head, user_creator);

								// creo gruppo
								aggiungi_gruppo(user_creator, group_name, &next_group_id, &group_head);
							}

							// aggiungo un nuovo utente al gruppo
							aggiungi_utente_a_gruppo(user_to_add, group_name, &group_head);

							// spedisco il numero di porta dell'utente
							porta = porta_da_username(user_to_add);
							int ret = send(i, (void *)&porta, sizeof(uint32_t), 0);
						}
						break;
						case 'N':
						{
							// ---------------------- routine richiesta di notify --------------------
							char notify_target[50];
							char notify_sender[50];
							// ricevo il nome dell'utente da notificare
							ricevi_messaggio(buffer, i);
							sscanf(buffer, "%s %s", notify_sender, notify_target);

							notify_enqueue(&notify_head, notify_sender, notify_target);
						}
						break;

						case 'P':
						{
							// ---------------------- routine richiesta di numero di porta utente -----------

							// ricevo il nome dell'utente di cui devo cercare il numero di porta
							ricevi_messaggio(buffer, i);

							// mando la porta
							int porta = check_username_online(buffer);
							int ret = send(i, (void *)&porta, sizeof(uint32_t), 0);
						}
						break;
						}
					}
				}
			}
		}
	}
}
