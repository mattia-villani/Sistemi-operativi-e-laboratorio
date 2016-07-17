/** \file core.h
	\author Mattia Villani
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera
	originale dell' autore.  */
#ifndef CORE_WATOR
#define CORE_WATOR

#include <pthread.h>
#include <err.h>

#include "wator.h"

/*#define _SUB_MATRIX_DEBUG_ */
/*#define _DEBUG_ */
/* Disabilitare il precedente define per evitare di vedere i messaggi di debug */
/*#define _NO_INIT_ Apparentemente l'uso o meno di pthread_mutex_init sembra ininfluente*/

/***************************************** /

	CORE LIB FOR WATOR! 
	definizioni particolari : 

/ *****************************************/

#define SOCK_NAME "./tmp/visual.sck"
#define UNIX_PATH_MAX (108)

/* numero massimo di connessioni accettate da visualizer. 
 * tendenzialmente inutile poichè si connetterà solo wator 
 */
#define MAX_CONNECTIONS (10)

#define WORK_DEF (4)
#define CHRON_DEF (1)

/***************************************** /

	Strutture di base

/ *****************************************/

/* SOCK_CMDS rappresenta il tipo di comando che può essere inviato a visualizer mediante socket*/
typedef enum {
	SOCK_CMD_INIT, /* in seguito verranno inviati nrow, ncol */
	SOCK_CMD_QUIT, /* chiude la connessione */
	SOCK_CMD_EXIT, /* termina il processo */
	SOCK_CMD_SHOW, /* in seguito verrà inviata la matrice: invia <dim><matrice compressa> */
	SOCK_CMD_SHOW_AND_QUIT /* supponendo la init sia stata precedentemente fatta */
}SOCK_CMDS;

/*definizioni per comodità. inutili*/
typedef int Bool;
typedef void* Ide ;
typedef pthread_mutex_t* Mutex;
typedef pthread_cond_t* Cond;

/**  effettua una malloc con controllo sull'esito. 
 *	in caso di null return della malloc, invoca Log ( definito dopo ) con parametro FATAL e PERROR, quindi blocca l'eseguzione
 * param size: spazio da allocare	
 * retval: puntatore a spazio allocato	
 */
Ide testedMalloc( size_t size );

/** richiede la lock su *mutex
 * param mutex: mutex da lockkare
 */
void lock ( Mutex mutex );

/** richiede la unlock su *mutex
 * param mutex: mutex da unlockkare
 */
void unlock( Mutex mutex );

/** valuta la divisione arrotondata per eccesso di num/div
 * param num: numeratore
 * param div: divisore
 * retval : parte intera superiore di num/div
 */
int top(int num, int div); /* arrotondamento per eccesso */

/** effettua la printf di v nella forma binaria
 * param v: valore da stampare in bit
 */
void printBin ( char v );

/********************************************************************************************************************************* /
  *
  *										LOG
  *
/ *********************************************************************************************************************************/

/* definisce i tipi di log che posso avere:
 *		Debug: stampa a fine di debug, disattivabile eliminando un define
 *		Fatal: chiama la exit dopo la stampa con codice 1 
 */
typedef enum {
	DEBUG,
	FATAL
} Log_type;

/* definisce il modo in cui il log effettua la stampa:
 * 		PERROR: viene chiamata la funzione perror
 *		NOPERROR: non viene chiamato perror
 */
typedef enum {
	PERROR,
	NOPERROR
} Log_mode;

/** effettua il log ovvero la stampa con informazioni ausiliare del messaggio
 * param msg: messaggio da stampare
 * param type: se FATAL invoca la exit, altrimenti no
 * param mode: se PERROR invoca perror, altrimenti no
 */
void Log ( const char msg [] , Log_type type , Log_mode mode );

/** testa se pointer è null, qualora lo sia invoca Log con type FATAL
 * param pointer: puntatore da testare
 * param msg: messaggio da passare a log in caso di errore
 * param mode: mode da passare a log in caso di errore
 * retval: pointer
 */
Ide testNull ( Ide pointer 	, const char msg [] , Log_mode mode );

/** testa se value è -1, qualora lo sia, invoca Log con type FATAL
 * come funz. precedente.
 * retval: value
 */
int testMinus( int value 	, const char msg [] , Log_mode mode );

/********************************************************************************************************************************* /
  *
  *										QUEUEU
  *		Coda realizzata mediante lista. Questo tipo di coda è stato scelto per la semplicità di implementazione
  *			tuttavia pone degli ovvi problemi: sotto stress effettua un grande numero di trap to kernel per le malloc e le free.
  *		Sarebbe opportuno realizzare la coda mediante array e poi mediante liste in caso che l'array non sia sufficiente,
  *			o meglio ancora come vector.
  *		IMPORTANTE: è assolutamente necessario che la queue possa crescere ad oltranza poichè verrà usata come base per la sycqueue,
  *			la quale costituisce un meccanismo di sincronizzazione tra thread.
  *			E' stato assunto che l'operanzione enqueue NON sia bloccante. se lo fosse ci sarebbe il rischio di deadlock
  *
/ *********************************************************************************************************************************/

typedef Ide Elem ;

/* definisce un nodo di una lista */
typedef struct node_ {
	Elem info;
	struct node_ * next;
} Node;

/* la coda si realizza mediante lista 
 * si tiene memoria dell'inizio per estrarre 
 * e della fine per inserire
 */
typedef struct {
	Node * head ;
	Node * tail ;
} queue_t;

/* e' opportuno che venga maneggiata come ref
 */
typedef queue_t * Queue;

/**	
 *  Crea con malloc una queue;
 *	retval: puntatore alla coda
 */
extern Queue queue_create ();

/**	
 *	param que: queue to free
 *  free una queue;
 */
extern void queue_destroy ( Queue que );

/**
 *	param que: queue in cui fare la push
 *	param value: element to insert
 *	Aggiunge value a que;
 */
extern void enqueue( Queue que , Elem value ) ;

/**
 *	param que: queue da cui fare la pop;
 *  param defa: valore da ritornare in caso non sia possibile la dequeue;
 *	toglie il più vecchio inserito e lo ritorna;
 */
extern Elem dequeue( Queue que , Elem defa ) ;

/**
 *	 param que: coda su cui testare il predicato
 *	Indica se vuota.
 */
extern Bool isEmpty ( Queue que );

/**
 *	 param que: coda da svuotare
 *	chiama dequeue fino a che isEmpty non restituisce 1
 *	 retval: restituisce il numero di elementi rimossi
 */
extern int queue_clear ( Queue que );



/********************************************************************************************************************************* /
  *
  *										STRUTTURA CONDIVISA!
  *		Fornisce un astrazione di un Ide condiviso.
  *		L'ide viene incapsulato con una mutex ed una condition variable
  *
/ *********************************************************************************************************************************/

/* Struttura che incapsula un generico riferimento e fornisce le due tipiche mutex e cond */
typedef struct {
	Ide sharedItem;
	Mutex mutex;
	Cond cond_var;	
} _SycCont;
/* alias per lettura più agevole ed uso guidato */
typedef _SycCont* SycCont;

/** effettua la lock sulla mutex di syc
 *	param syc: struttura da lockare
 */
void syc_lock ( SycCont syc );

/** effettua la unlock sulla mutex di syc
 *	param syc: struttura da unlockare
 */
void syc_unlock ( SycCont syc );

/** crea propriamente la syc struct
 *	param shared: valore da incapsulare
 *	retval: la nuova syc
 */
SycCont syc_create( Ide shared );

/** distrugge una syc struct
 * param syc: struttura da distruggere
 */
void syc_destroy( SycCont syc );


/********************************************************************************************************************************* /
  *
  *										Syc queue
  *		Coda condivisa le cui operazioni avvengono in mutua esclusione.
  *		Ottenuta usando SycCont come contenitore per queue.
  *		Questa coda è generalmente usata per contenere pochi elementi, 
  *			tuttavia a volte ne viene fatto un uso intensivo riempiendola di tanti elementi
  *		E' necessario che la enqueue NON sia bloccante per i seguenti motivi:
  *			-->	A volte il produttore ed il consumatore sono lo stesso thread
  *			-->	A volte il produttore è il consumatore del prodotto del proprio consumatore
  *
/ *********************************************************************************************************************************/

/* Alias per chiarezza del codice */
typedef SycCont SycQueue;

/** crea una nuova sycqueue inizializzando
 *		Queue e SycCont;
 * retval: la nuova coda
 */
SycQueue sycqueue_create( );

/** effettua la enqueue su que di value in mutua esclusione
 *	param que: coda a cui aggiungere value
 *	param value: elemento da inserire alla cosa
 */
void sycqueue_enqueue( SycQueue que, Elem value );

/** effettua la dequeue su que in mutua esclusione
 *	param que: coda da cui fare la dequeue
 *	retval: elemento rimosso
 */
Elem sycqueue_dequeue(SycQueue que);

/** testa l'attributo isEmpty su que
 *	param que: coda da testare
 *	retval: 1 se que è vuota, 0 altrimenti
 */
int sycqueue_isEmpty( SycQueue que );

/** distrugge propriamente la coda 
 *	param que: coda da eliminare
 */
void sycqueue_destroy( SycQueue que );

/********************************************************************************************************************************* /
  *
  *										COMPRESSIONE!
  *
/ *********************************************************************************************************************************/

/* Alias per chiarezza del codice 
 * un bits è una coppia di bit 
 * bits_t ne contiene 4
 */
typedef unsigned char bits_t;

/*
 *			ATTENZIONE:
 *	buffer e dest nelle due successive funzioni
 *	devono esser stati preallocati con dimensione opportuna
 */

/** effettua la compressione della sequenza origin,
 *	La sequenza compressa viene caricata in buff
 * param buffer: destinazione della compressione
 * param origin: sequenza da comprimere
 * param dim: numero di elementi di origin
 * retval: numero di bits di buffer
 */
int compress( bits_t buffer[], cell_t origin[], int dim ); 

/** effettua la decompressione della sequenza src,
 *	La sequenza decompressa viene caricata in dest
 * param dest: destinazione della decompressione
 * param src: sequenza da decomprimere
 * param src_dim: numero di elementi di src
 * retval: lunghezza di dest buffer
 */
int decompress( cell_t dest[], bits_t src[], int src_dim );


#endif
