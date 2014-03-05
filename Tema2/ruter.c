#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "helpers.h"

#define INF 999

typedef struct {
	int vecini[10];
	int destination;
	int cost;
	int creation_time;
	char payload[1348];
} my_pkt;

int out, in, nod_id;
int timp = -1;
int gata = FALSE;
int seq_no = 0;

//nu modificati numele, modalitatea de alocare si initializare a tabelei de rutare
//se foloseste la mesajele de tip 8/10 (deja implementate aici) si la logare (implementata in simulator.c)
int tab_rutare [KIDS][2];
int topologie[KIDS][KIDS];
msg LSADatabase[KIDS];
int got_LSA[KIDS];
msg newQueue[BUFSIZE], oldQueue[BUFSIZE];
int fnQ = -1, lnQ = -1, foQ = -1, loQ = -1;

int power(int x, int y) {
	int i, rez = 1;
	for(i = 0; i < y; i++)
		rez = rez * x;
	return rez;
}


void calcul_tabela_rutare() {
	//calculati tabela de rutare pornind de la topologie
	//folosind un algoritm de drum minim
	//Dijkstra
	struct state {
		int predecessor;
		int length;
		enum {permanent, temporar} label;
	} state [KIDS];

	int j, i, k, min;
	struct state *p;

	for (j = 0 ; j < KIDS; j++) {
		p = &state[j];
		p->predecessor = - 1 ;
		p->length = INF;
		p->label = temporar;
	}

	state[nod_id].length = 0;
	state[nod_id].label = permanent;
	k = nod_id;

	int count = 1;
	do {

		for (i = 0; i < KIDS; i++) {
			if (topologie[k][i] > 0 && state[i].label == temporar) {
				if (state[k].length + topologie[k][i] < state[i].length) {
					state[i].predecessor = k;
					state[i].length = state[k].length + topologie[k][i];
				} else if (state[k].length + topologie[k][i] == state[i].length) {
					if(k < state[i].predecessor)
						state[i].predecessor = k;
				}
			}
		}

		k = 0; min = INF;
		for (i = 0; i < KIDS; i++) {
			if (state[i].label == temporar && state[i].length < min) {
				min = state[i].length;
				k = i;
			}
		}
		
		count++;
		state[k].label = permanent;

	} while (count < KIDS);

	i = 0;

	for(i = 0; i < KIDS; i++) {
		if(state[i].length != INF) {
			tab_rutare[i][0] = state[i].length;
			k = i;
			if(i != nod_id) {
				while(state[k].predecessor != nod_id) {
					k = state[k].predecessor;
				}
				//tab_rutare[i][1] = state[i].predecessor;
				tab_rutare[i][1] = k;
			}
		}
	}

}

void procesare_eveniment(msg mevent) {

	if (mevent.add == TRUE) {
		printf ("Nod %d, msg tip eveniment - am aderat la topologie la pasul %d\n", nod_id, timp);

	}
	else
		printf ("Timp %d, Nod %d, procesare eveniment\n", timp, nod_id);

	char line[100];
	memcpy(&line, mevent.payload, 100);
	int tip_event = line[0] - '0';
	int i;
	
	/* Procesare eveniment tip 1 */
	if(tip_event == 1) {
	
		int id_ruter = line[2] - '0';
		int nr_vecini = line[4] - '0';
		int i_v = 0, cursor = 6;
		int vecin;
		int lista_vecini[20];
	
		/* Parsare linia asociata evenimentului de tip 1 */
		while(i_v < nr_vecini) {
			int dim = 0, cost = 0;
			vecin = line[cursor] - '0';
			cursor = cursor + 2;
			while(line[cursor + dim] != ' ' && line[cursor + dim] != '\n') {
				dim++;
			}
			for(i = 0; i < dim; i++) {
				cost = cost + (line[cursor + i] - '0') * power(10, (dim - i - 1));
			}
			lista_vecini[i_v * 2] = vecin;
			lista_vecini[i_v * 2 + 1] = cost;	
			i_v++;
			cursor = cursor + dim + 1;
		}
		
		/* Trimitere mesaje DatabaseRequest la vecini */
		for(i = 0; i < nr_vecini; i++) {
			msg request_msg;
			request_msg.type = 2;
			request_msg.creator = id_ruter;
			request_msg.seq_no = seq_no++;
			request_msg.sender = id_ruter;
			request_msg.time = timp;
			request_msg.next_hop = lista_vecini[i * 2];
			my_pkt p;
			p.vecini[lista_vecini[i * 2]] = lista_vecini[i * 2 + 1];
			memcpy(request_msg.payload, &p, sizeof(my_pkt));
			write(out, &request_msg, sizeof(msg));
		}
		
	}
	/* Procesarea evenimentului de tip 1 incheiata */
	
	
	/* Procesare eveniment tip 2 */
	if(tip_event == 2) {
		
		/* Parsare linia asociata evenimentului de tip 2 */
		int r1 = line[2] - '0';
		int r2 = line[4] - '0';		
		int cursor = 6, dim = 0, cost = 0;
		while(line[cursor + dim] != '\n') {
			dim++;
		}
		for(i = 0; i < dim; i++) {
			cost = cost + (line[cursor + i] - '0') * power(10, (dim - i - 1));
		}
		
		topologie[r1][r2] = topologie[r2][r1] = cost;
		
		/* Trimitere mesaje DatabaseRequest la capatul celalalt al linkului */
		msg request_msg;
		my_pkt p;
		request_msg.type = 2;
		request_msg.creator = nod_id;
		request_msg.seq_no = seq_no++;
		request_msg.sender = nod_id;
		request_msg.time = timp;
		if(nod_id == r1)	{
			request_msg.next_hop = r2;
			p.vecini[r2] = cost;
		}
		
		if(nod_id == r2)	{
			request_msg.next_hop = r1;
			p.vecini[r1] = cost;
		}
		memcpy(request_msg.payload, &p, sizeof(my_pkt));
		write(out, &request_msg, sizeof(msg));	
	}
	/* Procesarea evenimentului de tip 2 incheiata */
	
	
	/* Procesare eveniment tip 3 */
	if(tip_event == 3) {
		
		/* Parsare linia asociata evenimentului de tip 2 */
		int r1 = line[2] - '0';
		int r2 = line[4] - '0';
		
		/* Actualizare topologie */
		topologie[r1][r2] = topologie[r2][r1] = -1;
		
		/* Trimitere LSA propriu catre toti vecinii */
		msg update_LSA;
		update_LSA.type = 1;
		update_LSA.creator = nod_id;
		update_LSA.sender = nod_id;
		update_LSA.time = timp;
		update_LSA.seq_no = seq_no++;
		my_pkt p1;
		for(i = 0; i < KIDS; i++) {
			p1.vecini[i] = topologie[nod_id][i];
		}
		memcpy(update_LSA.payload, &p1, sizeof(my_pkt));
		for(i = 0; i < KIDS; i++) {
			if(topologie[nod_id][i] > 0) {
				update_LSA.next_hop = i;
				write(out, &update_LSA, sizeof(msg));
			}
		}
				
	}
	/* Procesarea evenimentului de tip 3 incheiata */
	
	
	/* Procesare eveniment tip 4 */
	if(tip_event == 4) {
		
		/* Parsare linia asociata evenimentului de tip 2 */
		int sursa = line[2] - '0';
		int destinatie = line[4] - '0';
		
		/* Creeare pachet tip 4 */		
		msg pkt;
		my_pkt px;
		px.destination = destinatie;
		px.cost = 0;
		px.creation_time = timp;
		memcpy(pkt.payload, &px, sizeof(my_pkt));
		pkt.type = 4;
		pkt.creator = sursa;
		pkt.sender = sursa;
		pkt.seq_no = seq_no;
		pkt.time = timp;
		pkt.next_hop = tab_rutare[destinatie][1];
		write(out, &pkt, sizeof(msg));
		
	}
	/* Procesarea evenimentului de tip 4 incheiata */
	
	
	//aveti de implementat tratarea evenimentelor si trimiterea mesajelor ce tin de protocolul de rutare
	//in campul payload al mesajului de tip 7 e linia de fisier corespunzatoare respectivului eveniment
	//(optional) vezi simulator.c, liniile 93-119 (trimitere mes tip 7) si liniile 209-219 (parsare fisier evenimente)

	//rutere direct implicate in evenimente, care vor primi mesaje de tip 7 de la simulatorul central:
	//eveniment tip 1: ruterul nou adaugat la retea  (ev.d1  - vezi liniile indicate)
	//eveniment tip 2: capetele noului link (ev.d1 si ev.d2)
	//eveniment tip 3: capetele linkului suprimat (ev.d1 si ev.d2)
	//eveniment tip 4:  ruterul sursa al pachetului (ev.d1)
}

int main (int argc, char ** argv)
{

	msg mesaj, mesaj_event;
	msg last_LSA;
	int cit, k;
	int i, j;
	int event = FALSE;

	out = atoi(argv[1]);  //legatura pe care se trimit mesaje catre simulatorul central (toate mesajele se trimit pe aici)
	in = atoi(argv[2]);   //legatura de pe care se citesc mesajele

	nod_id = atoi(argv[3]); //procesul curent participa la simulare numai dupa ce nodul cu id-ul lui este adaugat in topologie



	for(i = 0; i < KIDS; i++) {
		for(j = 0; j < KIDS; j++) {
			if(i == j) topologie[i][j] = 0;
			else topologie[i][j] = -1;
		}
	}
	
	for(i = 0; i < KIDS; i++) {	
		got_LSA[i] = 0;
	}

	//tab_rutare[k][0] reprezinta costul drumului minim de la ruterul curent (nod_id) la ruterul k
	//tab_rutare[k][1] reprezinta next_hop pe drumul minim de la ruterul curent (nod_id) la ruterul k
	for (k = 0; k < KIDS; k++) {
		tab_rutare[k][0] = DRUMAX; 	// drum =DRUMAX daca ruterul k nu e in retea sau informatiile despre el nu au ajuns la ruterul curent
		tab_rutare[k][1] = -1; 			// in cadrul protocolului(pe care il veti implementa), next_hop =-1 inseamna ca ruterul k nu e (inca) cunoscut de ruterul nod_id (vezi mai sus)
	}

	//printf ("Nod %d, pid %u alive & kicking\n", nod_id, getpid());

	if (nod_id == 0) { //sunt deja in topologie
		tab_rutare[0][0] = 0;
		tab_rutare[0][1] = -1;
		timp = -1; //la momentul 0 are loc primul eveniment
		mesaj.type = 5; //finish procesare mesaje timp -1
		mesaj.sender = nod_id;
		write (out, &mesaj, sizeof(msg));
		//printf ("TRIMIS Timp %d, Nod %d, msg tip 5 - terminare procesare mesaje vechi din coada\n", timp, nod_id);

	}

	last_LSA.time = -1;

	while (!gata) {
		cit = read(in, &mesaj, sizeof(msg));

		if (cit <= 0) {
			//printf ("Adio, lume cruda. Timp %d, Nod %d, msg tip %d cit %d\n", timp, nod_id, mesaj.type, cit);
			exit (-1);
		}

		int creator;

		switch (mesaj.type) {

			//1,2,3,4 sunt mesaje din protocolul link state;
			//actiunea imediata corecta la primirea unui pachet de tip 1,2,3,4 este buffer-area
			//(punerea in coada /coada new daca sunt 2 cozi - vezi enunt)

			case 1:
				creator = mesaj.creator;

				if((mesaj.seq_no > LSADatabase[creator].seq_no || 
																got_LSA[creator] == 0) && mesaj.time >= last_LSA.time) {
					if (lnQ == -1) {
						fnQ = lnQ = 0;
		                newQueue[lnQ] = mesaj;
					} else {
		                lnQ++;
		                newQueue[lnQ] = mesaj;
					}
				}
                               

				//printf ("Timp %d, Nod %d, msg tip 1 - LSA\n", timp, nod_id);
				break;

			case 2:
    			if (lnQ == -1) {
					fnQ = lnQ = 0;
               newQueue[lnQ] = mesaj;
				} else {
               lnQ++;
              	newQueue[lnQ] = mesaj;
				}


				//printf ("Timp %d, Nod %d, msg tip 2 - Database Request\n", timp, nod_id);
				break;

			case 3:
				creator = mesaj.creator;

				if((mesaj.seq_no > LSADatabase[creator].seq_no || 
																got_LSA[creator] == 0) && mesaj.time >= last_LSA.time) {	
		       	if (lnQ == -1) {
						fnQ = lnQ = 0;
		            newQueue[lnQ] = mesaj;
					} else {
		            lnQ++;
		            newQueue[lnQ] = mesaj;
					}
				}


				//printf ("Timp %d, Nod %d, msg tip 3 - Database Reply\n", timp, nod_id);
				break;

			case 4:
        		if (lnQ == -1) {
					fnQ = lnQ = 0;
               newQueue[lnQ] = mesaj;
				} else {
               lnQ++;
               newQueue[lnQ] = mesaj;
				}



				//printf ("Timp %d, Nod %d, msg tip 4 - pachet de date (de rutat)\n", timp, nod_id);
				break;

			case 6://complet in ceea ce priveste partea cu mesajele de control
					//puteti inlocui conditia de coada goala/mesaje_vechi, ca sa corespunda cu implementarea personala
				  //aveti de implementat procesarea mesajelor ce tin de protocolul de rutare
				{

				//printf ("Timp %d, Nod %d, msg tip 6 - incepe procesarea mesajelor puse din coada la timpul anterior (%d)\n", timp, nod_id, timp-1);
				
				memcpy(oldQueue, newQueue, (lnQ + 1) * sizeof(msg));
				foQ = fnQ; fnQ = -1;
				loQ = lnQ; lnQ = -1;
				//in scheletul de cod nu exista (inca) o coada

				//cat timp mai exista mesaje venite la timpul anterior
				while (foQ <= loQ) {
					//	procesez toate mesajele venite la timpul anterior
					//	(sau toate mesajele primite inainte de inceperea timpului curent - marcata de mesaj de tip 6)
					//	la acest pas/timp NU se vor procesa mesaje venite DUPA inceperea timpului curent
					//cand trimiteti mesaje de tip 4 nu uitati sa setati (inclusiv) campurile, necesare pt logare:
					//mesaj.time, mesaj.creator,mesaj.seq_no, mesaj.sender, mesaj.next_hop
					//la tip 4 - creator este sursa initiala a pachetului rutat

					msg urm_mesaj = oldQueue[foQ++];
					
					/* Procesare mesaj de tip 1 */
					if(urm_mesaj.type == 1) {					
					
						/* Actualizare LSADatabase si topologie */						
						int creator = urm_mesaj.creator;
						LSADatabase[creator] = urm_mesaj;
						got_LSA[creator] = 1;
						my_pkt p = *((my_pkt*) urm_mesaj.payload);
						for(i = 0; i < KIDS; i++) {
							topologie[creator][i] = topologie[i][creator] = p.vecini[i];
						}
						last_LSA = urm_mesaj;								
											
						/* Trimitere la vecinii sai */
						urm_mesaj.sender = nod_id;
						for(i = 0; i < 10; i++) {
							if(topologie[nod_id][i] != -1 && i != creator) {
								urm_mesaj.next_hop = i;
								write(out, &urm_mesaj, sizeof(msg));
							}
						}							
					}
					/* Terminare procesare mesaj de tip 1 */
					
					
					/* Procesare mesaj de tip 2 */
					if(urm_mesaj.type == 2) {
						int creator = urm_mesaj.creator;
						my_pkt p = *((my_pkt*) urm_mesaj.payload);
						
						/* Actualizare cost in topologie pentru noul vecin */
						int cost = p.vecini[nod_id];
						topologie[creator][nod_id] = topologie[nod_id][creator] = cost;
						
						/* Trimitere toata LSADatabase catre noul vecin */
						msg reply_LSA;
						for(i = 0; i < KIDS; i++) {
							if(got_LSA[i] == 1) {
								reply_LSA = LSADatabase[i];
								reply_LSA.type = 3;
								reply_LSA.sender = nod_id;
								reply_LSA.next_hop = creator;
								write(out, &reply_LSA, sizeof(msg));
							}
						
						}
						
						/* Trimitere LSA propriu catre toti vecinii */
						msg update_LSA;
						update_LSA.type = 1;
						update_LSA.creator = nod_id;
						update_LSA.sender = nod_id;
						update_LSA.time = timp;
						update_LSA.seq_no = seq_no++;
						my_pkt p1;
						for(i = 0; i < KIDS; i++) {
							p1.vecini[i] = topologie[nod_id][i];
						}
						memcpy(update_LSA.payload, &p1, sizeof(my_pkt));
						for(i = 0; i < KIDS; i++) {
							if(topologie[nod_id][i] > 0) {
								update_LSA.next_hop = i;
								write(out, &update_LSA, sizeof(msg));
							}
						}						
					}
					/* Terminare procesare mesaj de tip 2 */
					
					
					/* Procesare mesaj de tip 3 */
					if(urm_mesaj.type == 3) {
					
						/* Actualizare LSADatabase si topologie */
						int creator = urm_mesaj.creator;
						LSADatabase[creator] = urm_mesaj;
						got_LSA[creator] = 1;
						my_pkt p = *((my_pkt*) urm_mesaj.payload);
						for(i = 0; i < KIDS; i++) {
							if(i != nod_id)
								topologie[creator][i] = topologie[i][creator] = p.vecini[i];
						}
						last_LSA = urm_mesaj;					
					}
					/* Terminare procesare mesaj de tip 3 */

					
					
					/* Procesare mesaj de tip 4 */
					if(urm_mesaj.type == 4) {
						
						my_pkt p = *((my_pkt*) urm_mesaj.payload);
						int destinatie = p.destination;
						p.cost = p.cost + topologie[urm_mesaj.sender][nod_id];
						if(destinatie != nod_id) {
							memcpy(urm_mesaj.payload, &p, sizeof(my_pkt));
							urm_mesaj.sender = nod_id;
							urm_mesaj.time = timp;
							urm_mesaj.next_hop = tab_rutare[destinatie][1];
							write(out, &urm_mesaj, sizeof(msg));
						}
					
					}
					/* Terminare procesare mesaj de tip 4 */
					
					
					//cand coada devine goala sau urmatorul mesaj are timpul de trimitere == pasul curent de timp:
					
				}

				
							
				//procesez mesaj eveniment
				if (event) {
					procesare_eveniment(mesaj_event);
					event = FALSE;
				}

				calcul_tabela_rutare();

				//nu mai sunt mesaje vechi, am procesat evenimentul(daca exista), am calculat tabela de rutare(in aceasta ordine)
				//trimit mesaj de tip 5 (terminare pas de timp)
				mesaj.type = 5;
				mesaj.sender = nod_id;
				write (out, &mesaj, sizeof(msg));
				}
				break;

			case 7: //complet implementat - nu modificati! (exceptie afisari on/off)
					//aici se trateaza numai salvarea mesajului de tip eveniment(acest mesaj nu se pune in coada), pentru a fi procesat la finalul acestui pas de timp
					//procesarea o veti implementa in functia procesare_eveniment(), apelata in case 6
				{
				event = TRUE;
				memcpy (&mesaj_event, &mesaj, sizeof(msg));

				if (mesaj.add == TRUE)
					timp = mesaj.time+1; //initializam timpul pentru un ruter nou
				}
				break;

			case 8: //complet implementat - nu modificati! (exceptie afisari on/off)

				{
				//printf ("Timp %d, Nod %d, msg tip 8 - cerere tabela de rutare\n", timp, nod_id);

				mesaj.type = 10;  //trimitere tabela de rutare
				mesaj.sender = nod_id;
				mesaj.time = timp;
				memcpy (mesaj.payload, &tab_rutare, sizeof (tab_rutare));
				//Observati ca acest tip de mesaj (8) se proceseaza imediat - nu se pune in nicio coada (vezi enunt)
				write (out, &mesaj, sizeof(msg));
				timp++;
				}
				break;

			case 9: //complet implementat - nu modificati! (exceptie afisari on/off)
				{
				//Aici poate sa apara timp -1 la unele "noduri"
				//E ok, e vorba de procesele care nu reprezentau rutere in retea, deci nu au de unde sa ia valoarea corecta de timp
				//Alternativa ar fi fost ca procesele neparticipante la simularea propriu-zisa sa ramana blocate intr-un apel de read()
				//printf ("Timp %d, Nod %d, msg tip 9 - terminare simulare\n", timp, nod_id);
				gata = TRUE;
				}
				break;


			default:
				//printf ("\nEROARE: Timp %d, Nod %d, msg tip %d - NU PROCESEZ ACEST TIP DE MESAJ\n", timp, nod_id, mesaj.type);
				exit (-1);
		}
	}
	
return 0;

}
