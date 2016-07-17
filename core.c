/** \file core.c
	\author Mattia Villani
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera
	originale dell' autore.  */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <sys/syscall.h>

#include "core.h"


Ide testedMalloc( size_t size ){
	/* ritorna il puntatore verificando sia diverso da NULL */
	return testNull ( malloc(size) , "Malloc fault", NOPERROR );
}


void lock ( Mutex mutex ) {
	/* simile a quello visto a lezione. */
	int err ;
	testNull ( mutex, "Locking null mutex", NOPERROR );
	if (( err = pthread_mutex_lock ( mutex ) )) {
		errno = err;
		Log( "Can't lock", FATAL, PERROR );
	}
}

void unlock ( Mutex mutex ) {
	/* simile a quello visto a lezione. */
	int err ;
	testNull ( mutex, "Unlocking null mutex", NOPERROR );
	if (( err = pthread_mutex_unlock ( mutex ) )) {
		errno = err;
		Log( "Can't unlock", FATAL, PERROR );
	}
}

int top(int num, int div){
	/* divido e sommo 1 se c'è un resto, 0 altrimenti */
	return (int)(num/div) + ( (num%div) ? 1 : 0 ) ;
}

/********************************************************************************************************************************* /
  *
  *										LOGGING
  *
/ *********************************************************************************************************************************/

void Log ( const char msg [] , Log_type type , Log_mode mode ){
	/* scrittura a schermo di msg con dati aggiuntivi, per debug principalmente! */
	/* carattere da stampare che identifichi il type */
	char c;
	/* thread identificator da stampare qualora sia diponibile */
	pid_t tid;
	
	/* nel caso in cui non si sia in debug, la funzione termina prematuramente se il type è debug*/
	#ifndef _DEBUG_
		if ( type == DEBUG ) return;
	#endif 
	
	/* Identifico un simbolo da associare al messaggio */
	switch ( type ) {
		case DEBUG: c='D'; break;
		case FATAL: c='E'; break;
		default: 	c='?'; break;
	}
	
	/* Nel caso in cui sia disponibile la funzione gettid, metto il tid in tid.*/
	#ifdef SYS_gettid
		/* la sc gettid() nel mio pc non c'era. */
		tid = syscall(SYS_gettid); 
	#else
		/* se il tid non è stato possibile trovarlo */
		tid = -1 ;
	#endif
	
	/* stampa del messaggio sullo standard error */
	fprintf( stderr, "--LOG_MSG--%c: pid(%5d) ppid(%5d) tid(%5d) msg: %s ", c, getpid(), getppid(), tid, msg );
	
	/* qualora sia definito mode come PERROR, invoco anche perror.*/
	if ( mode == PERROR ) perror( "perror message:" );
	/* end line per flush e leggibilità */
	printf("\n");
		
	/* qualora type sia FATAL, è avvenuto un errore e quindi il processo termina! */
	if ( type == FATAL )
		exit(EXIT_FAILURE);
}

Ide testNull ( Ide pointer , const char msg [] , Log_mode mode ){
	/* Se il valore passato è null, notifica il messaggio e termina, altrimenti restituisce il valore */
	if ( pointer == NULL )
		Log( msg, FATAL, mode );
	return pointer;
}

int testMinus( int value , const char msg [] , Log_mode mode ){
	/* Se il valore passato è -1, notifica il messaggio e termina, altrimenti restituisce il valore */
	if ( value == -1 )
		Log( msg, FATAL, mode );
	return value;
}



/********************************************************************************************************************************* /
  *
  *										QUEUEU
  *
/ *********************************************************************************************************************************/



Queue queue_create (){
	Queue que = testedMalloc ( sizeof( Queue ) );
	/* coda vuota */
	que->head = que->tail = NULL;
	return que;
}

void queue_destroy ( Queue que ){
	/* controllo sul parametro */
	testNull( que, "Destroy called on NULL", NOPERROR);
	/* rimuovo tutti gli elementi dalla coda e poi libero la coda */
	queue_clear( que );
	free(que);
}

int isEmpty ( Queue que ){
	testNull( que, "IsEmpty called on NULL", NOPERROR);
	return que->head == NULL;
}

void enqueue( Queue que , Elem value ){
	Node * node = testedMalloc ( sizeof(Node) );
	testNull( que , "Enqueue called on NULL", NOPERROR );
	
	/* nuovo nodo */
	node->next = NULL;
	node->info = value;
	
	/* se que->head => la coda era vuota, l'elemento è sia primo che ultimo */
	if ( que->head == NULL ) que->tail = que->head = node;
	else{
		/* appendo l'elemento alla lista */
		que->tail->next = node;
		que->tail = node ;
	}
}

Elem dequeue( Queue que, Elem defau ) {
	Node * node ;
	/* valore da ritornare */
	Elem ret ;

	testNull( que , "Dequeueing NULL", NOPERROR );
	
	/* se la coda è vuota ritorno defau per evitare comportamenti anomali */
	if ( isEmpty ( que ) ) return defau;
	
	/* estraggo il nodo dalla cima */
	node = que->head;
	que->head = que->head->next;
	if (que->head== NULL) que->tail = NULL; /* Empty */	
	
	/* salvo il valore e libero il nodo */
	ret = node->info ;
	free ( node );
	
	return ret;
}

int queue_clear ( Queue que ){
	int c=0;
	testNull( que , "Clear called on NULL", NOPERROR );
	/* fin tanto che ho elementi li rimuovo */
	while ( ! isEmpty(que) ){
		dequeue( que , 0 );
		++c;
	}
	/* ritorno il numero di elementi rimossi */
	return c;
}


/********************************************************************************************************************************* /
  *
  *										STRUTTURA CONDIVISA!
  *
/ *********************************************************************************************************************************/

void syc_lock ( SycCont syc ){
	/* effettuo una lock su syc in maniera safe: controllo di non operare su null */
	testNull( syc , "Locking NULL SYC", NOPERROR);
	lock( testNull(syc->mutex,"Locking NULL mutex", NOPERROR) );
}
void syc_unlock ( SycCont syc ){
	/* effettuo una unlock su syc in maniera safe: controllo di non operare su null */
	testNull( syc , "unlocking NULL SYC", NOPERROR);
	unlock( testNull(syc->mutex,"Unlocking NULL mutex", NOPERROR) );
}
SycCont syc_create( Ide shared ){
	/* creo syc come un contenitore di shared con mutex e cond_var associate */
	SycCont syc=testedMalloc(sizeof(_SycCont));
	syc->sharedItem = shared; 
	syc->mutex = testedMalloc (  sizeof(pthread_mutex_t) );
	syc->cond_var = testedMalloc(sizeof(pthread_cond_t ) );
	/* inizializzo i nuovi puntatori */
	pthread_mutex_init( syc->mutex , NULL );
	pthread_cond_init ( syc->cond_var, NULL );
	return syc;
}
void syc_destroy( SycCont syc ){
	/* distrugge correttamente syc */
	free( testNull( syc , "Called Destroy on NULL SYC", NOPERROR) );
	free( testNull( syc->mutex , "Called Destroy on NULL SYC->Mutex", NOPERROR) );
	free( testNull( syc->cond_var , "Called Destroy on NULL SYC->Cond_Var", NOPERROR) );
}

/********************************************************************************************************************************* /
  *
  *										Syc queue
  *
/ *********************************************************************************************************************************/

SycQueue sycqueue_create( ){
	/* crea un syc che abbia come sharedItem una cosa */
	Queue que = queue_create();
	SycQueue syc = syc_create( que );
	return syc;
}
void sycqueue_enqueue( SycQueue que, Elem value ){
	syc_lock( que );
	/* enqueue chiamata su sharedItem in una zona safe da race condition */
	enqueue( que -> sharedItem , value );
	/* avviso eventuali consumatori in attesa che è pronto un nuovo valore da consumare */
	pthread_cond_signal ( que -> cond_var );
	syc_unlock( que );
}
Elem sycqueue_dequeue(SycQueue que){
	Elem ret;
	syc_lock( que );
	/* ciclo fin tanto che la coda è vuota (in caso di signal spurie) */
	while ( isEmpty( que->sharedItem ) )
		/* mi metto in attesa che un produttore faccia una enqueue */
		pthread_cond_wait( que->cond_var, que->mutex );
	/* dequeue chiamata su sharedItem in una zona safe da race condition */
	ret = dequeue( que -> sharedItem , NULL );
	syc_unlock( que );
	return ret;
}
int sycqueue_isEmpty( SycQueue que ){
	int ret ;
	syc_lock( que );
	/* controllo in maniera safe */
	ret = isEmpty ( que -> sharedItem );
	syc_unlock( que );
	return ret;
}
void sycqueue_destroy( SycQueue que ){
	/* distruggo propriamente la struttura */
	queue_destroy( que -> sharedItem );
	syc_destroy( que );
}


/********************************************************************************************************************************* /
  *
  *										COMPRESSIONE!
  *
/ *********************************************************************************************************************************/

/* For debug purpose*/
/* Restituisco la valutazione booleana di v */
#define IS(v) ( v?1:0 )
void printBin ( char v ) {
	/* v all'inizio è un byte */
	int b=(int)v;
	/* stampo v come una sequenza di bit */
	printf("%d%d %d%d %d%d %d%d",IS(b&128),IS(b&64),IS(b&32),IS(b&16),IS(b&8),IS(b&4),IS(b&2),IS(b&1));
}

bits_t cell_to_bits( cell_t cell ) {
	/* USELESS: per evitare malfunzionamenti in caso di assegnamenti numerici all'emun; */
	/* definisce un assegnamnto di cell -> {0,1,2} che può esser diverso dall'enum di cell */
	switch ( cell ){
		case SHARK: return 0; break;
		case FISH : return 1; break;
		case WATER: return 2; break;
	}
	Log("Converting to bit unrecognized cell", FATAL, NOPERROR);
	return 0;	
}
cell_t bits_to_cell( bits_t bits ) {
	/* funzione inversa alla precedente */
	switch ( bits ){
		case 0: return SHARK; break;
		case 1: return FISH; break;
		case 2: return WATER; break;
	}
	Log("Error deconverting: maybe special 11 ignored", FATAL, NOPERROR);
	return 0;	
}

/* definisco delle maschere per prender ei singoli bit di un bits_t */
const unsigned char MASK[]={ 
	192, /* 11 00 00 00  */
	48,  /* 00 11 00 00  */
	12,  /* 00 00 11 00  */
	3    /* 00 00 00 11  */  };
/* indico di quanti bit devo shiftare per spostare una coppia di bit in una posizione da poterli valutare come int */
const unsigned char SHIFT[]={
	6,  /* converte xx000000 in 000000xx */
	4,  /* converte 00xx0000 in 000000xx */
	2,  /* converte 0000xx00 in 000000xx */
	0   /* converte 000000xx in 000000xx */};
/* ottiene il valore dei due bit in posizione i su c */
#define GETPOS(c,i) ( (c&MASK[i])>>SHIFT[i] ) 

/* struttra che indica una cella ed il numero di volte che occorre */
typedef struct {
	cell_t cell;
	int count;
} cellOccs;

/* funzione static che crea una struttura cellOccs inizializzata coi parametri omonimi*/
cellOccs makecelloccs( cell_t cell , int count ){
	cellOccs o;
	o.cell = cell;
	o.count = count;
	return o;
}

/** funzione che dato un array di cellOccs preventivamente allocato, 
 * 	lo riempie con le coppie che occorrono in origin,
 *	=> compatta origin associando alla celle il numero di ripetizioni contigue.
 * param occ: array da riempire
 * param origin: array da cui attingere
 * param dim: lunghezza di origin
 * retval: numero di elementi inseriti in occ
 */
int countOcc ( cellOccs *occ , cell_t *origin, int dim ) {
	/* assumo che almeno un carattere ci sia */
	int len = 1;
	int i;
	/* la prima coppia è il primo elemento con occorrenza 1 */
	occ[0] = makecelloccs( origin[0], 1 );
	for ( i=1; i<dim ; i++ ) 
		/* fin tanto che ci sono elementi */ 
		if ( occ[len-1].cell == origin[i] ) 
			/* se l'ultima coppia inserita ha lo stesso valore cell ne incremento l'occorrenza*/ 
			occ[len-1].count++;
		else
			/* altrimenti inserisco una nuova coppia con occorrenza 1 */
			occ[len++] = makecelloccs( origin[i], 1 ); 
	return len;
}

/**	scrive su buffer preallocato, a partire dalla posizione pos, che però è intesa come la posizione dei due bits
 *	quindi pos NON è il numero di byte occupati in buffer. 
 *	param buffer: array in cui scrivere
 *	param bits: bits da scrivere ( solo due bit )
 *	param pos: posizione all'interno di buffer in cui scrivere bits
 */
void WRITE( bits_t buffer[], bits_t bits, int pos ) {
	int i = pos>>2; /* pos / 4 definisce l'indice del byte di buffer in cui si scrive*/
	int j = pos&3;  /* pos % 4 definisce in quale punto del byte scrivere */
	if ( j==0 ) buffer[i]=0; /* clear in caso non sia stata fatta precedentemente */
	bits = bits & 3 ; /* AND con la maschera 00 00 00 11 per sicurezza */
	/* scrivo i bits come l'or bit a bit del byte con bits da scrivere opportunamente posizionata su pos */
	buffer[i] = buffer[i] | ( bits << SHIFT[j] );
}

/** dato il buffer in cui scrivere e la posizione da cui partire, scrive la sequenza bits,
 *	tante volte quante indicate da times possibilemente in una forma compatta.
 *	Si limita a mettere un solo fattore.
 *	param buff: buffer in cui scrivre (deve essere preallocato)
 *	param bits: coppia di bit da scrivere 
 *	param times: numero di volte che bits deve esser scritto
 *	param pos: posizione da cui inziare a scrivere
 *	retval posizione da cui continuare a scrivere
 */
int write_many_times( bits_t buff[], bits_t bits, int times, int pos ){
	/* special bits che indica che la sequeza succesiva è un moltiplicatore */
	#define REP (3)
	/* ciclo fin tanto che non ho terminato di scrivere le volte richieste */
	while ( times ){
		/* poichè scrivere meno di 4 volte un carattere è meno dispendioso 
		 * ripeterlo esplicitamente, lo ripeto.
		 */
		if ( times < 4 )
			for ( ; times; --times)
				WRITE( buff, bits, pos++ );
		else if ( times < 8 ){
			/* in questo caso, posso usare la forma a fattore */
			/* scrivo il carattere */
			WRITE( buff, bits, pos++ );
			/* scrivo il jolly che mi indicata il moltiplicatore */
			WRITE( buff, REP, pos++ );
			/* scrivo il numero di volte per cui moltiplicare, assumendo che moltiplico almeno 4 volte */
			WRITE( buff, (bits_t)(times-4), pos++ );
			/* ho scritto tutte le volte richieste */
			times=0;
		}else{
			/* in una versione migliore a questo punto potrei concatenare i fattori
			 * in questa versione, separo la richiesta di scritture in tante scritture di massimo 7 */
			pos = write_many_times( buff, bits, 7, pos );
			times-=7;				
		}
	}		
	/* ritorno il nuovo indice */
	return pos;
}

/** Funzione che data una matrice linearizzata di cell_t, di dimensione dim, la comprime e la carica su dest
 *	param dest: array in cui viene scritta la forma compressa ( deve esser preallocato )
 *	param src: matrice da comprimere 
 *	param dim: lunghezza della matrice src
 *	retval : lunghezza di dest
 */
int compress( bits_t dest[], cell_t src[], int dim ){
	/* array di coppie <carattere, occorrenza> */
	cellOccs *occ;
	int occ_len;
	int dest_len=0;
	int i;
	
	/* probabile errore */
	if ( dim < 1 ) Log ( "Compressing a too much short sequence" , FATAL, NOPERROR);

	/* alloco l'array di coppie di supporto e lo inizializzo*/
	occ = testedMalloc( sizeof(cellOccs)*dim );
	occ_len = countOcc ( occ, src, dim ) ;
	
	#ifdef _DEBUG_COMPRESS_
	{	
		int sum = 0;
		Log( "\n\tCounting...:\n", DEBUG, NOPERROR );
		for ( i=0; i<occ_len ; i++){ 
			printf("%c.%d ", cell_to_char(occ[i].cell), occ[i].count );
			sum+=occ[i].count;
		}
		printf("\nSUM:%d\n",sum);
	}
	#endif
	
	/* per ogni carattere, lo scrivo tante volte, quante indicato nella coppia */
	dest_len=0;
	for ( i=0; i<occ_len ; i++ )
		dest_len = write_many_times( dest, cell_to_bits( occ[i].cell ), occ[i].count, dest_len );
	/* libero l'array di coppie */
	free( occ ) ;

	#ifdef _DEBUG_COMPRESS_ 
		Log( "\n\tCompressed...:\n", DEBUG, NOPERROR );
		printf("Buffer size: %d, charlen %d\n", dest_len, top(dest_len,4));
		/* size conta qunati bits ci sono (4 per char )*/
		for ( i=0; i<top(dest_len,4) ; i++){ 
			printBin( dest[i] );
			printf(", ");
		}
		printf(" .. done\n");
	#endif
	
	/* lunghezza della sequenza come coppie di bit */
	return dest_len; 
}

/* 	Tipo di supporto per agevolare la decompressione.
 *	La decompressione è realizzata mediante un automa di tre stati (quelli indicati dalla enum)
 */
typedef enum{
	READ,
	LOOK_ADD,
	MORE_OR_READ
} dec_state_t;

/** Funzione che decomprime la matrice
 *	param dest: array in cui caricare la matrice decompressa (preallocata)
 *	param src: array di coppie di bit da decomprimere
 *	param src_dim: numero di coppie di bit in src
 *	retval : lunghezza di dest 
 */
int decompress( cell_t dest[], bits_t src[], int src_dim ){ 
	#define REP (3)
	/* Stato iniziale */
	dec_state_t state = READ;
	int len=0;
	int i;
	/* variabile di appoggio che indica di quanto è stato trovato il moltiplicatore */
	int fattore;
	/* ricordo l'ultimo carattere letto per moltiplicarlo eventualmente */
	cell_t lastW;

	#ifdef _DEBUG_COMPRESS_
	printf("Decompressing...\n");
	for ( i=0; i<top(src_dim,4) ; i++){ 
		printBin( src[i] );
		printf(", ");
	}	
	printf("\n...Done\n");
	#endif

	for ( i=0; i<src_dim; i++ ){
		/* per ogni cella, calcolo gli indici effettivi */
		int index = i >> 2 ; /* /4 */
		int inpos = i & 3; /* 00 00 00 11 indice all'interno del char */
		/* prendo il carattere all'interno di src, otterò due bit */
		char c = GETPOS( src[index] , inpos );
		/* verifico in che stato sono per cambiarlo */
		switch( state ){
			case READ: default:	
				/* mi aspetto un carattere o un moltiplicatore */
				if ( c!=REP )
					/* ho letto un carattere, lo scrivo in dest */
					dest[len++] = lastW = bits_to_cell( c );
				else{
					/* ho letto un fattore, mi aspetto quindi di avere un moltiplicatore dopo */
					state=LOOK_ADD;				
					/* base della moltiplicazione 1 (utile per moltiplicazione concatenate)*/
					fattore=1;
					/* PER SCRIVERE CCC non usere il rep, per cui le C sono almeno 4 */
					dest[len++] = bits_to_cell( lastW );
					dest[len++] = bits_to_cell( lastW );
					dest[len++] = bits_to_cell( lastW );
				}
				break;
			case LOOK_ADD:
				/* mi aspetto un numero e lo moltiplico per il fattore */
				fattore *= (c);
				/* potrei avere ora un'altro moltiplicatore (non in questa versione) o un carattere */
				state = MORE_OR_READ;
				break;
			case MORE_OR_READ:
				if ( c!=REP ){
					/* ho trovato un carattere, consumo il fattore */
					for ( ; fattore ; fattore -- )
						dest[len++] = bits_to_cell( lastW );
					dest[len++] = lastW = bits_to_cell( c );
					/* torno nello stato iniziale */
					state=READ;
				}else
					/* non in questa versione */
					state=LOOK_ADD;
				break;
		}
	}
	/* mi aspetto di finire in uno stato diverso da Look_add, poichè non è previsto dall'automa */
	if ( state == LOOK_ADD ) Log("BITS BAD FORMATTED, finished with 11", FATAL, NOPERROR);
	/* se ho lasciato il moltiplicatore in sospeso, lo consumo */
	if ( state == MORE_OR_READ )
		for (; fattore ; fattore -- )
			dest[len++] = bits_to_cell(lastW);
	return len;
}




