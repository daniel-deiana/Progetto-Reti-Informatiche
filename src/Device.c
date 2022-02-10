#include "utils.h"

// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// ///////////////////////////////////////// NET CLIENT /////////////////////////////////////////////////////////////////////
// //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

int main(int argc, const char **argv)
{

	// lista per gli utenti attivi
	struct clientList *active_sockets_list_head = NULL;

	// lista degli utenti del gruppo corrente
	struct clientList *group_chat_sockets_head = NULL;

	// utente con cui sto chattando
	struct clientList *current_chatting_user = NULL;

	// stato dell'utente con cui sto chattando (ONLINE/OFFLINE)
	int current_chatting_user_state = NO_MEAN;

	// struttura dove mi salvo le credenziali passate nel login
	struct credentials my_credentials;

	// indirizzi per server, client, socket di listen
	struct sockaddr_in server_addr, cl_addr, cl_listen_addr, gp_addr;

	// variabili dove salvo i valori dei socket, valore di ritorno, valore descrittore max
	int sv_communicate, communicate, cl_socket, listener, ret, fdmax = 0;

	// variabile che mi dice se mi trovo in un gruppo o no
	int is_in_group = -1;

	// buffer generico
	char buffer[4096];

	// variabile dove salvo il nome del destinatario target (istruzioni \a user, chat user)
	char destUsername[50];

	// set di descrittori per la select()
	fd_set master, readfds;

	FD_ZERO(&master);
	FD_ZERO(&readfds);

	// --------------------- indirizzo server ---------------------

	memset(&server_addr, 0, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

	// socket server
	sv_communicate = socket(AF_INET, SOCK_STREAM, 0);

	// aggiungo il socket di comunincazione con il server tra i monitorati dall select
	FD_SET(STDIN, &master);

	// controlli di correttezza sugli argomenti passati al programma
	if (argc != 2)
	{
		perror("Il numero di parametri che ho inserito all'avvio è sbagliato");
		exit(1);
	}

	// ---------------------- indirizzo device --------------------

	memset(&cl_addr, 0, sizeof(cl_addr));
	cl_listen_addr.sin_family = AF_INET;
	cl_listen_addr.sin_port = htons(atoi(argv[1]));
	inet_pton(AF_INET, "127.0.0.1", &cl_listen_addr.sin_addr);

	listener = socket(AF_INET, SOCK_STREAM, 0);
	bind(listener, (struct sockaddr *)&cl_listen_addr, sizeof(cl_listen_addr));
	listen(listener, 50);

	FD_SET(listener, &master);
	fdmax = listener;

	// //////////////////////////////////////////////////////////// FASE IN/SIGNUP ///////////////////////////////////////////////////////////
	// ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// comandi signup e in
	int attempts = 0;

	for (;;)
	{
		char commandString[1024], headerBuffer[1024], sendbuffer[1024];
		char cmd[20], first[50], second[50], third[50];
		struct msg_header header;

		// parsing comandi
		fgets(commandString, 1024 - 1, stdin);
		sscanf(commandString, "%s %s %s %s", cmd, first, second, third);
		printf("stampo i comandi passati in input: first: %s, second: %s, third %s\n", first, second, third);

		// se è la prima volta che faccio partire il programma provo a connettermi

		// se la connect() va a buon file allora invio la mia richiesta/senno riprovo a connnettermi
		if (attempts == 0)
		{
			server_addr.sin_port = htons(atoi(first));
			printf("first ha valore: %s", first);
			if (connect(sv_communicate, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0)
			{
				perror("LOG: Errore nella connnesione iniziale con il server, inserire numero di porta corretto ...");
				continue;
			}
			printf("LOG: Connessione stabilita con il server\n");
			attempts++;
		}

		// 2 comandi possibili in/signup

		if (strcmp(cmd, "signup") == 0)
		{
			invia_header(sv_communicate, 'A', "reg");
			sprintf(sendbuffer, "%s %s", second, third);
			crea_rubrica(second);
		}
		else if (strcmp(cmd, "in") == 0)
		{
			invia_header(sv_communicate, 'B', "login");
			strcpy(my_credentials.Username, second);
			strcpy(my_credentials.Password, third);
			sprintf(sendbuffer, "%s %s", second, third);
		}
		else
		{
			printf("Comando errato ... riprovare\n");
			continue;
		}

		// invio numero di porta
		uint32_t port = atoi(argv[1]);
		if (send(sv_communicate, (void *)&port, sizeof(uint32_t), 0) < 0)
		{
			perror("LOG: errore nell'invio del numero di porta nella fase iniziale");
			exit(1);
		}

		// invio credenziali utente
		printf("prima di invia_msg: sto mandando: %s\n", sendbuffer);
		invia_messaggio(sendbuffer, sv_communicate);

		// risposta del server
		ricevi_header(sv_communicate, &header);

		if (header.RequestType == 'A')
		{
			// il server mi ha risposto in merito ad una richiesta di signup
			if (strcmp(header.Options, "first") == 0)
			{
				printf("Client registrato correttamente al servizio\n");
				salva_disconnessione(second);
			}
			else
				printf("Client gia registrato al servizio\n");
		}
		else
		{
			// controllo messaggio di login al server
			if (strcmp(header.Options, "ok") == 0)
			{
				char timestamp_string[TIMESTAMP_LEN];

				printf("Utente loggato\n");
				inserisci_utente(&active_sockets_list_head, "server", sv_communicate);

				// invio l'istante di disconnessione salvato l'ultima volta
				prendi_istante_disconnessione(my_credentials.Username, timestamp_string);
				invia_messaggio(timestamp_string, sv_communicate);
				break;
			}
			else
				printf("LOG: Hai inserito un username/password sbagliati\n");
		}
	}

	// //////////////////////////////////////////////////////////// CORE CLIENT //////////////////////////////////////////////////////////////
	// ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	// ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	// stampo comandi device
	stampa_comandi_device();

	fflush(stdin);
	// GESTIONE DEGLI INPUT DA STDIN E DEI SOCKET TRAMITE SELECT
	for (;;)
	{
		readfds = master;
		select(fdmax + 1, &readfds, NULL, NULL, NULL);
		for (int i = 0; i <= fdmax; i++)
		{
			if (FD_ISSET(i, &readfds))
			{
				// controllo se ho qualcosa di nuovo nello stdin
				if (i == STDIN)
				{

					// HO UN NUOVO COMANDO NELLO STDIN
					struct clientcmd cmd;
					char inputstring[1024];

					pulisci_buffer(inputstring, sizeof(inputstring));
					memset(&cmd, 0, sizeof(cmd));
					if (fgets(inputstring, 1024 - 1, stdin) == NULL)
						perror("Errore in lettura del comando\n");
					fflush(stdin);

					// se l'utente si trova in un gruppo o in una chat allora invio il messaggioo sullo stdin
					if (is_in_group == 0 || current_chatting_user != NULL)
					{
						// ////////////////////////// comandi chat /////////////////////////////////

						// controllo se l'utente ha scritto share <nome_file>
						if (check_share_command(my_credentials.Username, inputstring) == 0)
						{

							///////////////////////////// INVIO FILE ///////////////////////////////

							if (is_in_group == 0)
							{
								invio_file_gruppo(my_credentials.Username, inputstring, group_chat_sockets_head);
							}
							else
							{
								// prima di fare tutti i controlli, vedo se è un comando del tipo share <filename> e se il file esiste
								if (invia_file(my_credentials.Username, inputstring, current_chatting_user->socket) < 0)
									printf("LOG: invia_file ha ritornato un errore ");
							}
						}
						else if (inputstring[0] != '\\')
						{
							// //////////////////////// INVIO MESSAGGIO //////////////////////////////

							if (is_in_group == 0)
							{
								// INVIO DEL MESSAGGIO: CASO CHAT DI GRUPPO
								invia_messaggio_gruppo(inputstring, group_chat_sockets_head);
							}
							else
							{
								// INVIO DEL MESSAGGIO CASO CHAT SINGOLA

								if (current_chatting_user_state == OFFLINE)
								{
									// MESSAGGIO PENDENTE

									// bufferizzo sul server, invio prima richiesta di buffer poi messaggio vero e proprio
									char premessage[1024];

									invia_header(sv_communicate, 'E', "tosend");

									pulisci_buffer(premessage, sizeof(premessage));

									sprintf(premessage, "%s %s %s", my_credentials.Username, destUsername, inputstring);
									invia_messaggio(premessage, sv_communicate);

									// scrivo con una sola spunta
									scrivi_file_chat(my_credentials.Username, current_chatting_user->username, inputstring, SENDING, ONLY_SENDED);
								}
								else if (current_chatting_user_state == ONLINE)
								{
									// MESSAGGIO INVIATO DIRETTAMENTE AL PEER

									invia_messaggio(inputstring, current_chatting_user->socket);
									scrivi_file_chat(my_credentials.Username, current_chatting_user->username, inputstring, SENDING, RECEIVED);
								}
							}
						}
						else if (inputstring[1] == 'q')
						{
							// -------------------- comando "\q + "INVIO" -------------------------

							// TORNO ALLA SCHERMATA DEI COMANDI
							stampa_lista_utenti(group_chat_sockets_head);

							free(current_chatting_user);
							current_chatting_user = NULL;

							is_in_group = -1;
							elimina_utenti_lista(&group_chat_sockets_head);

							printf("///////// Menu comandi ///////////\n");

							break;
						}
						else if (inputstring[1] == 'u')
						{
							// -------------------- comando "\u + "INVIO" -------------------------

							// creazione di un gruppo

							// dico al server che voglio la lista degli utenti online
							if (is_in_group < 0)
							{
								// vuol dire che questa è la prima volta che aggiungo un utente dopo essere entrato in chat con un altro,
								// quindi creo un nuovo gruppo dove i primi partecipanti saremo io e l'utente con cui stavo chattando
								invia_header(sv_communicate, 'U', "create");
							}
							else
								// sono gia in un gruppo e aggiungo un utente a quest ultimo
								invia_header(sv_communicate, 'U', "add");

							// risposta server
							ricevi_messaggio(buffer, sv_communicate);
							printf("Lista utenti online:\n%s", buffer);

							// -----------------  comando "\a username + "INVIO" ------------------

							char group_name[50], username[50], cmd;
							pulisci_buffer(username, sizeof(username));
							pulisci_buffer(group_name, sizeof(group_name));

						riprova:
							scanf("%c %s", &cmd, username);
							fflush(stdin);

							if (cmd != 'a')
								goto riprova;

							// invio username utente da aggiungere
							invia_messaggio(username, sv_communicate);

							// inserisco nome gruppo
							printf("NOME DEL GRUPPO: ");
							scanf("%s", group_name);
							fflush(stdin);

							// invio nome gruppo
							invia_messaggio(group_name, sv_communicate);

							// ricevo numero di porta dell'utente che voglio aggiungere
							int porta, ret;
							ret = recv(sv_communicate, (void *)&porta, sizeof(uint32_t), 0);
							if (ret < 0)
								break;

							int new_socket = socket_da_username(active_sockets_list_head, username);

							// controllo se ho gia una connessione attiva con l'utente che voglio aggiungere
							if (new_socket < 0)
							{
								printf("LOG: Utente connesso sulla porta %d aggiunto\n", porta);

								// creo indirizzo destinatario e lo aggiungo ai socket della chat
								struct sockaddr_in indirizzo;

								// ------------------creazione indirizzo -------------------
								// ---------------------------------------------------------
								memset(&indirizzo, 0, sizeof(indirizzo));
								indirizzo.sin_family = AF_INET;
								indirizzo.sin_port = htons(porta);
								indirizzo.sin_addr.s_addr = INADDR_ANY;

								new_socket = socket(AF_INET, SOCK_STREAM, 0);
								if (connect(new_socket, (struct sockaddr *)&indirizzo, sizeof(indirizzo)) < 0)
								{
									perror("non sono riuscito a connettermi al client ");
									break;
								}
								invia_messaggio(my_credentials.Username, new_socket);
								// --------------------------------------------------------

								// aggiorno fd
								fdmax = (new_socket > fdmax) ? new_socket : fdmax;
								FD_SET(new_socket, &master);

								// aggiungo alla lista utenti attivi
								inserisci_utente(&active_sockets_list_head, username, new_socket);
							}

							if (current_chatting_user != NULL)
							{
								inserisci_utente(&group_chat_sockets_head, current_chatting_user->username, current_chatting_user->socket);
								// quando creo un gruppo sicuramente ero in una chat singola, quindi dico all'utente con cui avevo iniziato una chat di unirsi al gruppo
								invia_messaggio("group_established", current_chatting_user->socket);
								// dato che sto entrando in un gruppo non sono piu in una chat singola quindi aggiorno i campi relativi ad esssa
								free(current_chatting_user);
								current_chatting_user = NULL;
							}

							// dico al destinatario che è stato aggiunto in un gruppo
							invia_messaggio("new_group", new_socket);
							invia_messaggio(group_name, new_socket);

							// invio il numero di utenti
							int num_utenti = conta_utenti_chat(group_chat_sockets_head);
							ret = send(new_socket, (void *)&num_utenti, sizeof(int), 0);
							if (ret < 0)
								perror("LOG: Errore nell'invio del numero di utenti al nuovo utente del gruppo");

							// invio i nomi degli utenti della chat al nuovo arrivato
							invia_nomi_utenti(group_chat_sockets_head, new_socket);

							// inserisco il nuovo utente appena aggiunto ai miei sockets del gruppo
							inserisci_utente(&group_chat_sockets_head, username, new_socket);

							// setto il flag che mi dice se sto chattando con un solo utente o con un gruppo, e setto il nome del gruppo corrente
							is_in_group = 0;
						}
					}
					else
					{
						// /////////////////////// comandi menu principale //////////////////////////

						fflush(stdin);
						sscanf(inputstring, "%s %s", cmd.Command, cmd.Argument1);
						if (check(cmd.Command) == -1)
						{
							printf("Comando non valido:\n");
							break; // Ho passato un comando non valido
						}

						//  ////////////////////////// switching comandi /////////////////////////////

						switch (cmd.Command[0])
						{
						case 's':
						{
							// ------------------------ comando show -----------------------------
							if (handler_comand_show(my_credentials.Username, cmd.Argument1, sv_communicate) > 0)
							{
								printf("doveva inviarmi dei messaggi\n");
								// devo notificare il server che ho visualizzato i messaggi pendenti con la show
								invia_header(sv_communicate, 'N', "notify");

								char buf[256];

								// invio il nome del client da notificare al server
								pulisci_buffer(buf, sizeof(buf));

								sprintf(buf, "%s %s", my_credentials.Username, cmd.Argument1);
								invia_messaggio(buf, sv_communicate);
							}
						}
						break;

						case 'h':
						{
							// ----------------------- comando hanging ----------------------------
							invia_header(sv_communicate, 'H', "hang");

							receive_hanging_info(sv_communicate);
						}
						break;

						case 'o':
						{
							// -----------------------  comando out  -------------------------------

							// chiudo le comunicazioni con tutti i socket

							// devo sempre salvare il mio istante di disconnessione su file

							stampa_lista_utenti(active_sockets_list_head);
							chiudi_connesioni_attive(&active_sockets_list_head);
							salva_disconnessione(my_credentials.Username);
							exit(1);
						}
						break;

						case 'c':
						{
							// -----------------------  comando chat  -----------------------------
							struct msg_header header;

							char notify_response_buf[50];
							char optionString[8] = "NULLO";
							char sendbuffer[1024];

							// controllo se possiedo il destinatario nella mia rubrica personale
							if (check_utente_rubrica(my_credentials.Username, cmd.Argument1) < 0)
							{
								printf("Non ho il destinatario nella rubrica, non posso inziare la chat:\n");
								break;
							}

							invia_header(sv_communicate, 'C', optionString);

							strcpy(destUsername, cmd.Argument1);
							sprintf(sendbuffer, "%s", cmd.Argument1);

							// invio destinatario della chat al server
							invia_messaggio(sendbuffer, sv_communicate);

							// risposta server: destinatario online/offline
							ricevi_header(sv_communicate, &header);

							// ricevo il numero di porta dell'utente con cui voglio aprire una chat
							int port;

							// ricezione numero di porta
							if (recv(sv_communicate, (void *)&port, sizeof(int), 0) < 0)
								perror("LOG: errore nella ricezione del numero di porta");

							printf("il server mi ha mandato un header con la porta del dest che è la seguente\nPorta:%d\n", port);

							// prima della procedura devo vedere se l'utente con cui sto aprendo la chat mi aveva visualizzato i messaggi
							pulisci_buffer(notify_response_buf, sizeof(notify_response_buf));
							ricevi_messaggio(notify_response_buf, sv_communicate);

							if (strcmp(notify_response_buf, "notify_ack") == 0)
								aggiorna_stato_messaggi(my_credentials.Username, cmd.Argument1);

							// inizializzo ad offline, poi in base alla risposta del server lo setto ad online
							current_chatting_user_state = OFFLINE;

							// destinatario online
							if (strcmp("ON", header.Options) == 0)
							{
								// debug
								printf("LOG: destinatario online\n");

								current_chatting_user_state = ONLINE;
								if (socket_da_username(active_sockets_list_head, destUsername) < 0)
								{
									// debug
									printf("LOG: apro connessione TCP\n");

									// creazione indirizzo e socket e connessione al destinatario
									memset(&cl_addr, 0, sizeof(cl_addr));

									cl_addr.sin_family = AF_INET;
									cl_addr.sin_port = htons(port);
									inet_pton(AF_INET, "127.0.0.1", &cl_addr.sin_addr);
									cl_socket = socket(AF_INET, SOCK_STREAM, 0);

									// connessione al peer
									ret = connect(cl_socket, (struct sockaddr *)&cl_addr, sizeof(cl_addr));
									if (ret < 0)
										printf("Non sono riuscito a iniziare la chat con il destinatario\n");

									// invio il mio user al destinatario
									invia_messaggio(my_credentials.Username, cl_socket);

									// aggiorno descriptor list
									FD_SET(cl_socket, &master);
									fdmax = (cl_socket > fdmax) ? cl_socket : fdmax;

									// aggiungo alla lista utenti con cui sto comunicando
									inserisci_utente(&active_sockets_list_head, cmd.Argument1, cl_socket);
								}
								else
								{
									// ho gia il socket, mi basta inzializzare la variabile di riferimen
									cl_socket = socket_da_username(active_sockets_list_head, cmd.Argument1);
								}
							}

							inserisci_utente(&current_chatting_user, cmd.Argument1, cl_socket);
							/*

								prima di iniziare la chat devo chiedere per messaggi pendenti
								rispetto all'utente con cui sto per iniziare la chat

							*/

							if (handler_comand_show(my_credentials.Username, cmd.Argument1, sv_communicate) > 0)
							{
								printf("doveva inviarmi dei messaggi\n");
								// devo notificare il server che ho visualizzato i messaggi pendenti con la show
								invia_header(sv_communicate, 'N', "notify");

								char buf[256];

								// invio il nome del client da notificare al server
								pulisci_buffer(buf, sizeof(buf));

								sprintf(buf, "%s %s", my_credentials.Username, cmd.Argument1);
								invia_messaggio(buf, sv_communicate);
							}

							// carico i contenuti della chat su stdout
							carica_chat(my_credentials.Username, current_chatting_user->username);
						}
						}
					}
				}
				else
				{
					// ---------------------------- nuova connessione -----------------------------------------
					if (i == listener)
					{
						int addrlen = sizeof(gp_addr);

						// accetta la richiesta ed aggiorna i file descriptor
						communicate = accept(i, (struct sockaddr *)&gp_addr, (socklen_t *)&addrlen);
						if (communicate < 0)
							break;

						fdmax = (communicate > fdmax) ? communicate : fdmax;
						FD_SET(communicate, &master);

						char new_user[USERNAME_LEN];

						// chi mi chiede una connesione per prima cosa mi dice il suo nome
						pulisci_buffer(new_user, sizeof(new_user));
						ricevi_messaggio(new_user, communicate);
						inserisci_utente(&active_sockets_list_head, new_user, communicate);

						// se mi arriva una connessione da un client mentre sono in un gruppo vuol dire che è un componente del gruppo che mi sta aggiungendo
						if (is_in_group == 0)
							inserisci_utente(&group_chat_sockets_head, new_user, communicate);

						printf("Un client sta provando a connettersi\n");
					}
					else
					{
						// ------------------------ messaggi in entrata ---------------------------------------

						char bufferChatMessage[1024];
						char sender_username[50];

						pulisci_buffer(bufferChatMessage, sizeof(bufferChatMessage));

						if (ricevi_messaggio(bufferChatMessage, i) == 0)
						{
							// -------------------- gestione disconnesioni -------------------------------

							char User_logged_out[50];

							printf("Connessione chiusa con il socket: %d\n", i);

							// mi prendo l'username di chi è uscito
							pulisci_buffer(User_logged_out, sizeof(User_logged_out));
							username_da_socket(i, active_sockets_list_head, User_logged_out);

							if (current_chatting_user != NULL && strcmp(current_chatting_user->username, User_logged_out) == 0)
								// devo cambiare il suo stato in OFFLINE
								current_chatting_user_state = OFFLINE;

							FD_CLR(i, &master);
							close(i);

							rimuovi_utente(&active_sockets_list_head, i, User_logged_out);
							rimuovi_utente(&group_chat_sockets_head, i, User_logged_out);

							if (is_in_group == 0)
								printf("LOG: %s è uscito dalla chat di gruppo\n", User_logged_out);

							continue;
						}

						// mi prendo l'username di chi mi ha inviato il messaggio
						pulisci_buffer(sender_username, sizeof(sender_username));
						username_da_socket(i, active_sockets_list_head, sender_username);

						if (strcmp(bufferChatMessage, "group_established") == 0)
						{
							// vuol dire che un utente aveva iniziato una chat con me e sta iniziando un gruppo, quindi mi ci devo unire
							is_in_group = 0;
							inserisci_utente(&group_chat_sockets_head, sender_username, i);

							if (current_chatting_user != NULL)
							{
								// ero in una chat, la devo chiudere
								free(current_chatting_user);
								current_chatting_user = NULL;
							}
						}
						else if (strcmp(bufferChatMessage, "new_group") == 0 && is_in_group < 0)
						{
							// -------------------- controllo nuovo gruppo -------------------------------

							// devo controllare in che stato sono, se non sto chattando con nessuno allora entro direttamente nel gruppo
							// se mi arriva il messaggio di new_group ed ero in una chat con un utente allora esco dalla chat ed entro in modalità gruppo

							if (current_chatting_user != NULL)
							{
								// ero in una chat, la devo chiudere
								free(current_chatting_user);
								current_chatting_user = NULL;
								// setto il flag che mi dice se sto partecipando ad un gruppo
							}

							pulisci_buffer(bufferChatMessage, sizeof(bufferChatMessage));

							// sono stato aggiunto ad un gruppo e devo vedere se sono gia connesso con gli utenti
							int num_utenti;
							char group_name[50];

							// inserisco il creatore del gruppo fra gli utenti della chat
							inserisci_utente(&group_chat_sockets_head, sender_username, i);

							// ricevo il nome del gruppo
							pulisci_buffer(group_name, sizeof(group_name));
							ricevi_messaggio(group_name, i);

							printf("LOG: mi hanno aggiunto in un gruppo, nome: %s", group_name);

							int ret = recv(i, (void *)&num_utenti, sizeof(int), 0);
							if (ret < 0)
							{
								perror("LOG: Errore nella ricezione del numero di utenti");
								break;
							}

							// ricevo gli username degli utenti
							for (int k = 0; k < num_utenti; k++)
							{
								char buffer[256];
								pulisci_buffer(buffer, sizeof(buffer));

								// chi mi aggiunge mi dice i nomi delle altre persone nel gruppo
								ricevi_messaggio(buffer, i);
								printf("LOG: ricevuto il nome di un utente da aggiungere -> %s\n", buffer);
								int socket_utente_chat = socket_da_username(active_sockets_list_head, buffer);

								if (socket_utente_chat < 0)
								{
									printf("aggiungo un estraneo\n");

									int port;
									// devo creare una connessione con l'utente
									invia_header(sv_communicate, 'P', "port_req");

									// invio il nome dell'utente di cui voglio sapere la porta
									invia_messaggio(buffer, sv_communicate);

									// ricevo il numero di porta
									ret = recv(sv_communicate, (void *)&port, sizeof(int), 0);
									if (ret < 0)
									{
										perror("LOG: errore nella ricezione del numero di porta del partecipante alla chat");
										break;
									}

									if (port < 0)
										continue;

									// creazione indirizzo
									struct sockaddr_in indirizzo;
									indirizzo.sin_family = AF_INET;
									indirizzo.sin_port = htons(port);
									indirizzo.sin_addr.s_addr = INADDR_ANY;

									// mi connetto al nuovo client
									socket_utente_chat = socket(AF_INET, SOCK_STREAM, 0);
									ret = connect(socket_utente_chat, (struct sockaddr *)&indirizzo, sizeof(indirizzo));
									// mando il mio nome
									invia_messaggio(my_credentials.Username, socket_utente_chat);

									// aggiorno lista descrittori
									fdmax = (socket_utente_chat > fdmax) ? socket_utente_chat : fdmax;
									FD_SET(socket_utente_chat, &master);

									// aggiungo il nuovo utente alla lista utenti con cui sono connesso
									inserisci_utente(&active_sockets_list_head, buffer, socket_utente_chat);
								}
								// aggiungo l'utente con cui ho appena creato una connesione alla lista del gruppo
								inserisci_utente(&group_chat_sockets_head, buffer, socket_utente_chat);
							}
							// aggiorno il flag che mi dice se sto partecipando attualmente ad un gruppo
							free(current_chatting_user);
							current_chatting_user = NULL;
							is_in_group = 0;
						}
						else if (strcmp(bufferChatMessage, "***FILE***") == 0)
						{
							// -------------------- controllo richiesta invio file -------------------------------

							// il sender mi vuole inviare un file
							printf("LOG: Sto ricevendo un file\n");
							if (ricevi_file(my_credentials.Username, i))
								printf("LOG: ricevi_file ha ritornato un errore");
						}
						else
						{
							// -------------------- ricezione messaggio di una chat ------------------------------

							if (is_in_group == 0)
							{
								printf("chat di gruppo: messaggio da %s -> %s", sender_username, bufferChatMessage);
							}
							else
							{
								// non sono in una chat di gruppo quindi abbiamo 2 casi, o sono in una chat singola o non sto chattando con nessuno

								if (current_chatting_user != NULL)
									// sono in una chat con qualcuno, stampo diretttamente il messaggio a schermo
									printf("chat singola: messaggio --> %s\n", bufferChatMessage);

								else
									// allora non sono in una chat, devo solamente avvisare che è arrivato un messaggio e da chi
									printf("nuovo messaggio da parte di %s\n", sender_username);

								// scrivo il messaggio ricevuto nella chat relativa a sender_username
								scrivi_file_chat(my_credentials.Username, sender_username, bufferChatMessage, RECEIVING, NO_MEAN);
							}
						}
					}
				}
			}
		}
	}
}
