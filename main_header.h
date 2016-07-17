/** \file main_header.h
	\author Mattia Villani
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera
	originale dell' autore.  */
#ifndef _MAIN_HEADER_
#define _MAIN_HEADER_

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <string.h>
#include <wait.h>
#include <error.h>
#include <errno.h>
#include <stdint.h>
#include <signal.h>
#include <math.h>

#include "core.h"

/* massimo numero di tentativi di connessione da effettuare prima di considerare un errore */
#define NUM_OF_TRIAL (10)
/* attesa tra un tentativo e l'altro */
#define DELAY (1)
/* messaggio da visualizzare in caso sia stato invocato wator in maniera sbagliata */
#define HELP_MSG "USAGE : wator file [-n nwork] [-v chronon] [-f dumpfile]"

/* numero di righe e colonne della sub matrix */
#define K (3)
#define N (3)

#define MINIMO (3)

#if K<MINIMO
#error "K too SMALL"
#elif N<MINIMO
#error "N too SMALL"
#endif

/* spessore del bordo condiviso! */
#define WEIGHT (1)

/* secondi da aspettare prima di lanciare un allert */
#define SEC (10)
/* file su cui fare il dump quando un allarme viene catturato */
#define WATORCHECK "wator.check"

#define MIN(a,b) (a<b?a:b)

/* messaggi che i thread si scambiano mediante le code */
#define EVENT_QUEUE_MSG_EXIT (0)
#define EVENT_QUEUE_MSG_SHOW (1)
#define EVENT_QUEUE_MSG_LAST_SHOW (2)
#define EVENT_QUEUE_MSG_REQUEST_UPDATE (9)
#define EVENT_QUEUE_MSG_DISPACHER_UPDATE (10)

/* stato di una cella : serve ad evitare che un pesce appena nato venga mangiato
 *	in quanto il pasto degli squali avviene prima della nascita dei pesci
 * 	cosa che in un'area condivisa non è garantita */
typedef enum{
	UNKNOWN,/* valore di default */ 
	MOVED, 	/* mosso => non può esser mosso */
	CREATED /* appena creato da procreazione => non può esser mangiato */
} cell_state_t;

/*	Reale descrizione di una cella di una sotto matrice che fa riferimento ad una cella reale
 *	Gli indici i e j indicano la posizione della cella all'interno del pianeta
 *	Il valore di w è un riferimento a quello della cella, per comodità
 *	La mutex serve a garantire che un solo worker operi sulla cella
 *	Lo stato definisce come un essere limitrofe si deve comportare nei confronti di quello contenuto qua
 *	Trascino una copia della referenza a wator per comodità
 */
typedef struct { 
	int i, j;
	cell_t * w;
	Mutex mutex;
	cell_state_t *state;
	wator_t *pw;
} real_cell_t;

/* Sotto matrice: ne sono definite le dimensioni
 *	e una matrice di real_cell_t con l'aggiunta dei bordi (area raggiungibile da un
 *	essere in questa sotto matrice, che risiede in un'altra sotto matrice)
 */ 
typedef struct {
	int _nrow, _ncol; /* potrebbero esser minori di n, k*/
	real_cell_t	cell[K+2*WEIGHT][N+2*WEIGHT]; /* per comodità metto la cornice */	
} sub_planet_t;

/*	Un worker_i è definito da:
 *		Il thread che lo concretizza
 *		Un indice nell'array dei worker del dispacer
 */
typedef struct {
	pthread_t thread;
	int wid;
} worker_t;

/*	Mutua esclusione nella modifica dei contatori realizzata mediante
 *	un syccont. sarebbe sufficiente una mutex 
 */
SycCont syc_wator;

/* Code usate per applicare il protocollo descritto ad inizio pagina */
/* coda per far comunicare chiunque con M_T */
SycQueue EVENT_QUEUE;
/* coda per far comunicare chiunque (solitamente dispacher e workers) con C_T */
SycQueue TO_COLLECTOR_QUEUE;
/* coda per far comunicare chiunque (solitamente M_T e C_T ) con D_T */
SycQueue TO_DISPACHER_QUEUE;
/* coda di submatrici diponibili ai worker */
SycQueue sm_pool;

/* array di sottopianeti da inizializzare */
sub_planet_t * sub_planets;
/* dimensione del precedente array */
int num_of_subs;

/* array di worker. la dimensione è nwork */
worker_t * workers;

/* coda di elementi allocati in inizializer di D_T da deallocare al termine di esso */
Queue toFree;
/* coda di real_cell_t di cui va resettato lo stato che risiedono in aree condivise */
SycQueue toClean;

/***************************************************************************************/

/** Funzione che inisizlizza le maschere dei segnali ed associa gli handler
 */
void setSignals();

/***************************************************************************************/
/*
 *	WORKERS
 */

/*
 *	Funzione di avvio di un worker 
 */
void* main_worker( void* args );

/*
 *	DISPACHER
 */

/*
 *	Funzione di avvio del dispacher 
 */
void* main_dispacher( void* args );

/*
 *	COLLECTOR
 */
 
/*
 *	Funzione di avvio del collector 
 */ 
void* main_collector( void* args );

/*
 *	SOCKET_utils
 */

/** 
 *	Chiude il processo visualizer, cioè gli scrive sul socket il comando SOCK_CMD_EXIT
 */
void closeVisualizer( );

/*
 *	Funzione che comunica a visualizer che la matrice ha dimensione nrow x ncol 
 */
void visualizer_init( int nrow, int ncol );

/*
 *	Funzione che data una matrice la invia al visualizer
 */
void show(cell_t*w, int nrow, int ncol);

#endif
