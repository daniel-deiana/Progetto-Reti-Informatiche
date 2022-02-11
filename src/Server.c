#include "utils.h"

// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////// NET SERVER /////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
int main(int argc, const char **argv)
{

	/* variabile dove appoggio i valori delle credenziali passate dal client durante una signup/in */
	struct credentials cl_credentials;

	// strutture per indirizzi
	struct sockaddr_in server_addr, client_addr;

	// lista gruppi
	struct des_group *group_head = NULL;

	// lista utenti online
	struct clientList *head = NULL;

	// lista richieste di notify
	struct notify_queue *notify_head = NULL;

	// gruppi
	uint32_t next_group_id = 0;

	// variabili per socket, descrittoremax
	uint32_t Listener, communicate, ret, addrlen, fdmax;

	// buffer generico
	char buffer[4096];

	// lista di descrittori
	fd_set master, readfds;

	// parsing degli argomenti da linea di comando
	if (argc != 2)
	{
		perror("Sto passando piu di un argomento in linea di comando");
		exit(1);
	}

	// Assegno il valore passato a Serverport

	printf("La porta selezionata è %s \n", argv[1]);

	printf("//////////////////// SERVER ONLINE ///////////////////////\n\n");
	stampa_comandi_server();

	memset(&cl_credentials, 0, sizeof(cl_credentials));
	FD_ZERO(&master);
	FD_ZERO(&readfds);
	FD_SET(0, &master);

	// ------------------ indirizzo server ----------------------
	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(atoi(argv[1]));
	inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

	// socket di listen
	Listener = socket(AF_INET, SOCK_STREAM, 0);

	// socket di listen
	bind(Listener, (struct sockaddr *)&server_addr, sizeof(server_addr));
	listen(Listener, 50);

	FD_SET(Listener, &master);
	fdmax = Listener;

	for (;;)
	{
		readfds = master;
		fflush(stdin);

		// gestione input da tastiera e da sockets tramite io multiplexing
		select(fdmax + 1, &readfds, NULL, NULL, NULL);

		for (uint32_t i = 0; i <= fdmax; i++)
		{
			if (FD_ISSET(i, &readfds))
			{
				if (i == STDIN)
				{

					char Command[100];
					// Check della correttezza del comando sullo stdin
					fscanf(stdin, "%s", Command);

					// ------------------ switching comandi server -------------------
					if (strcmp(Command, "help") == 0)
					{
						// comando help
						stampa_comandi_server();
					}
					else if (strcmp(Command, "list") == 0)
					{
						// comando list
						comando_list();
					}
					else if (strcmp(Command, "esc") == 0)
					{
						// comando esc
						exit(1);
					}
					else
					{
						// comando non valido, torno sopra
						printf("comando non esistente\n");
						continue;
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

						printf("Nuova connessione TCP\n");
					}
					else
					{
						// socket di comunicazione

						struct msg_header service_hdr;

						printf("///////////////////// HANDLING RICHIESTA ///////////////////\n");

						// ricezione di una richiesta
						if (ricevi_service_msg(i, &service_hdr) == 0)
						{
							// chiusura della connessione TCP
							char logout_user[50];

							close(i);
							FD_CLR(i, &master);

							rimuovi_utente(&head, i, logout_user);
							stampa_lista_utenti(head);

							logout(logout_user);
							stampa_history_utenti();

							// debug
							printf("LOG: connessione chiusa con %d\n", i);
							continue;
						}

						// debug
						printf("LOG: tipo richiesta: %c\n", service_hdr.RequestType);
						printf("LOG: campo option: %s\n", service_hdr.Options);

						// pulizia buffer messaggi
						memset(buffer, 0, sizeof(buffer));

						switch (service_hdr.RequestType)
						{
						case 'A':
						{
							// /////////////////////////// routine di registrazione ///////////////////////////

							struct credentials MyCredentials;
							struct HistoryRecord record;
							uint32_t port;

							// ricezione numero di porta
							if (recv(i, (void *)&port, sizeof(uint32_t), 0) < 0)
								perror("LOG: errore nella ricezione del numero di porta");
							port = ntohs(port);

							// ricezione credenziali
							ricevi_messaggio(buffer, i);
							sscanf(buffer, "%s %s", MyCredentials.Username, MyCredentials.Password);

							if (is_client_registered(MyCredentials.Username) == 0)
								invia_service_msg(i, 'A', "before");
							else
							{
								invia_service_msg(i, 'A', "first");

								// aggiungo record sugli utenti registrati e sulla history degli utenti del nuovo arrivato
								registra_utente(buffer);

								inizializza_history(buffer);
							}
						}
						break;

						case 'B':
						{
							// /////////////////////////// routine di login ///////////////////////////

							char timestamp_string[TIMESTAMP_LEN];
							uint32_t port;

							if (recv(i, (void *)&port, sizeof(uint32_t), 0) < 0)
								perror("LOG: errore nella ricezione del numero di porta");
							port = ntohs(port);
							// client mi sta mandando credenziali di login
							ricevi_messaggio(buffer, i);

							// ricezione numero di porta

							sscanf(buffer, "%s %s", cl_credentials.Username, cl_credentials.Password);

							if (is_client_registered(cl_credentials.Username) == 0)
							{

								invia_service_msg(i, 'B', "ok");

								pulisci_buffer(timestamp_string, sizeof(timestamp_string));
								ricevi_messaggio(timestamp_string, i);

								printf("LOG: timestamp ultima disconnessione di %s : %s \n", cl_credentials.Username, timestamp_string);

								aggiorna_history_utente(cl_credentials.Username, port);
								stampa_history_utenti();

								// gestione lista utenti online
								inserisci_utente(&head, cl_credentials.Username, i);
								stampa_lista_utenti(head);
							}
							else
							{
								// debug
								printf("LOG: Non ho trovato le credenziali nel registro del server\n");
								invia_service_msg(i, 'B', "noreg");
							}
						}
						break;

						case 'C':
						{
							// ---------------------------- routine chat ------------------------------
							uint32_t port;

							ricevi_messaggio(buffer, i);
							printf("LOG: Ho ricevuto la richiesta di una chat con l'utente <%s>\n", buffer);

							port = check_username_online(buffer);

							if (port == -1)
								invia_service_msg(i, 'C', "OFF");
							else
								invia_service_msg(i, 'C', "ON");

							// invio il numero di porta
							port = htons(port);
							if (send(i, (void *)&port, sizeof(uint32_t), 0) < 0)
							{
								perror("LOG: errore nell'invio del numero di porta nella fase iniziale");
								exit(1);
							}

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
							uint32_t numbyte;

							memset(&tobuffer, 0, sizeof(tobuffer));

							// device mi sta mandando il suo nome e il nome del destinatario
							memset(messagebuffer, 0, sizeof(messagebuffer));
							ricevi_messaggio(messagebuffer, i);

							sscanf(messagebuffer, "%s %s %[^\t]", tobuffer.sender, tobuffer.receiver, tobuffer.message);
							printf("LOG: Ho un messaggio da bufferizzare: %s \n", tobuffer.message);

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

							uint32_t porta;
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
							if (strcmp(service_hdr.Options, "create") == 0)
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
							porta = htons(porta_da_username(user_to_add));
							uint32_t ret = send(i, (void *)&porta, sizeof(uint32_t), 0);
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
							uint32_t porta = htons(check_username_online(buffer));
							int ret = send(i, (void *)&porta, sizeof(uint32_t), 0);
						}
						break;
						}
						printf("///////////////// FINE HANDLING RICHIESTA /////////////////\n\n");
					}
				}
			}
		}
	}
}
