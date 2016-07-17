/*\author Mattia Villani
 * Dichiaro che questo codice è in ogni sua parte inventiva dell'autore
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <err.h>
#include "wator.h"

/** \enum bool
 * 	assume i valori true e false. Serve ad emulare il tipo boolean
 * */
typedef enum {
	FALSE = 0,
	TRUE = 1
} bool ;


char cell_to_char (cell_t a) {
	/* svolgimento del testo mediante costrutto switch. casi esplicitati nella descrizione della funzione*/
	char ret ;
	switch ( a ) {
		case WATER : 	ret = 'W'; 	break;
		case SHARK : 	ret = 'S'; 	break;
		case FISH : 	ret = 'F'; 	break;
		default : 		ret = '?'; 	break;
	}
	return ret;
}


int char_to_cell (char c) {
	/* svolgimento del testo mediante costrutto switch. casi esplicitati nella descrizione della funzione*/
	int ret ;
	switch ( c ){
		case 'W' : 		ret = WATER;	break;
		case 'S' : 		ret = SHARK;	break;
		case 'F' : 		ret = FISH;		break;
		default :		ret = -1; 		break;
	}
	return ret;
}


planet_t * new_planet (unsigned int nrow, unsigned int ncol){
	planet_t *p ;
	int i;
	/** area della matrice */
	int area = nrow * ncol; 
	/** considero errata la richiesta di una matrice che abbia una dimensione vuota */
	if ( ! area ) return NULL;
	
	/** Per una maggiore facilità di comprensione del codice, per una maggiore semplicità
	 * 		di trattamento di errori e per evitare un uso massiccio di system call,
	 * 		la malloc sarà chiamata una sola volta e tutte le infformazioni sul planet saranno contigue.
	 * 	Assumo che new_planet sia una delle prime funzioni chiamate nel programma,
	 * 		per cui l'effetto di una serie di malloc dovrebbe avere lo stesso risultato
	 * 	La suddivisione della memoria allocata sarà esemplificata nei commenti successivi.
	 *  L'idea è la seguente:
	 * 		< planet_t >
	 * 		< vettore di referenze a righe di w>
	 * 		< vettore di referenze a righe di btime>
	 * 		< vettore di referenze a righe di dtime>
	 * 		< vettore rappresentante matrice unidimensionale di dimensione area ( w ) >
	 * 		< vettore rappresentante matrice unidimensionale di dimensione area ( btime ) >
	 * 		< vettore rappresentante matrice unidimensionale di dimensione area ( dtime ) >	  	 
	 * 	Benchè abbiano lo stesso size e siano entrambi int32, verrà distinto tra int ed enum per chiarezza
	 * 		così come per int* ed enum* che vengono distinti per rendere più leggibile il codice
	 *  Si stima un'allocamento di sizeof(planet_t) + 3*nrow*sizeof(void*) + 3*nrow*ncol*sizeof(int) 
	 * */
	p = malloc(
		sizeof( planet_t ) 				/* spazio destinato a planet */
		+ nrow*sizeof( cell_t * ) 		/* spazio destinato a referenziare le righe di w*/
		+ 2*nrow*sizeof( int * ) 		/* spazio destinato a referenziare le righe di btime e dtime*/
		+ area*sizeof( cell_t )			/* matrice w */
		+ 2*area*sizeof( int )			/* matrici btime e dtime */
	);
	/** errore di allocazione. */
	if ( ! p  ) return NULL;

	/** l'allocazione è avvenuta */
	p->nrow = nrow;
	p->ncol = ncol;

	/** In base allo schema indicato prima w punta al vettore di righe successivo a plane_t
	 * 	Il cast a char* serve affinchè l'operazione aritmentica sul puntatore si sposti esattamente 
	 *  	alla memoria successiva al pl
	 * anet. sarebbe stato equivalente p->w = (void*)( p+1 ); */
	p->w = (cell_t**) ( (char*)p + sizeof( planet_t ) );
	/** p->btime e p->dtime si posizionano ai vettori righa successivi a p->w */
	p->btime = (int**)( (p->w) + nrow );
	p->dtime = (int**)( (p->btime) + nrow );

	/** Inizializzazione dei vettori riga : */
	p->w [0] = (cell_t*)(p->dtime + nrow);
	p->btime[0] = (int*)(p->w[0] + area);
	p->dtime[0] = (int*)(p->btime[0] + area);
	for ( i=1; i<nrow; i++ ){
		p->w[i] = p->w[i-1] + ncol;
		p->btime[i] = p->btime[i-1] + ncol;
		p->dtime[i] = p->dtime[i-1] + ncol;		
	}
	/** Inizializzazione delle matrici */
	for ( i=0; i<area; i++ ){
		p->w[0][i] = WATER;
		p->btime[0][i] = p->dtime[0][i] = 0;
	}
	
	return p;
}

void free_planet (planet_t* p){
	/** ATTENZIONE la seguente funzione ha senso solo se planet è stato creato con new_planet!!!!!!
	 *  In base alla funzione sopra citata lo spazzio allocato è unico e contiguo.
	 * */	
	free(p);
}

int print_planet (FILE* f, planet_t* p){
	int i,j;
	/** errore de file non valido */
	if ( ! f ) 
		return -1;
	/* stampo le indicazioni riga colonna */
	fprintf (f, "%d\n%d\n", p->nrow, p->ncol);
	for (i=0; i<p->nrow; i++)
		for( j=0; j<p->ncol; j++ )
			/* per ogni cella stampo il carattere ed uno spazio o il carattere ed un endline */ 
			#ifndef COLOR_DEBUG
			if ( ! fprintf(f, ( j==p->ncol-1 ) ? "%c\n" : "%c ", cell_to_char( p->w[i][j] )) ) 
				return -1;
			#else
			{
				if ( p->w[i][j] == WATER )
					printf ( "\x1b[34m" "W" "\x1b[0m" );
				if ( p->w[i][j] == SHARK )
					printf ( "\x1b[31m" "S" "\x1b[0m" );
				if ( p->w[i][j] == FISH )
					printf ( "\x1b[32m" "F" "\x1b[0m" );
				printf ( (j == p->ncol -1) ? "\n" : " " );
			}
			#endif
	#ifndef DEBUG
	return 0;
	#endif
}

planet_t* load_planet (FILE* f){
	planet_t *p ;
	char c;
	int nr, nc, i, j;
	/* la lettura di numeri negativi è considerata errata */
	if ( fscanf(f,"%d\n%d\n", &nr, &nc) != 2 || nr<0 || nc <0 ){
		errno = ERANGE;
		return NULL;
	}
	p = new_planet( nr, nc );
	for (i=0; i<nr; i++)
		for (j=0; j<nc; j++)
			/* per ogni cella mi aspetto di leggere esattamente e solo il carattere o poi uno spazio o un endline*/
			if ( fscanf( f, (j==nc-1) ? "%c\n" : "%c " , &c ) != 1 
			|| ( p->w[i][j] = char_to_cell(c) ) == '?' ){
				errno = ERANGE;
				/* libero la memoria */
				free_planet( p );
				return NULL;
			}
	return p;
}


/**definizione di una macro utile SOLO in new_wator. essa restituisce l'intero all'indice i dove 
 * i è l'indice in cui str e path sono uguali. Ha senso SOLO sotto le assunzioni di new wator
 * */
#define CERCA( pt, strs, vs ) (( ! strcmp(pt,strs[0]) ? vs[0] : (! strcmp(pt,strs[1]) ? vs[1] : vs[2])  )) 

wator_t* new_wator (char* fileplan){
	FILE *f_p= fopen(fileplan, "r");
	FILE *f  = fopen(CONFIGURATION_FILE, "r");
	wator_t *p ;
	uint v[3];
	/** Le successive variabili sarranno usate per cumulare la lettura
	 * In particolare verranno usate stringhe per semplicità (per usare strcmp)
	 */
	char st[3][3];
	st[0][2] = st[1][2] = st[2][2] = '\0'; /*verranno letti caratteri per cui l'end line dovrà esser messo a mano*/
	
	if ( ! f || ! f_p ){ 
		if (f) fclose(f);
		if (f_p) fclose(f);
		/* errno settato da fopen */
		return NULL;
	}
	if(fscanf( f, "%c%c %u\n", st[0], st[0]+1, &v[0] ) < 3 /* Se l'esito è true allora il formato è errato*/
	|| fscanf( f, "%c%c %u\n", st[1], st[1]+1, &v[1] ) < 3
	|| fscanf( f, "%c%c %u\n", st[2], st[2]+1, &v[2] ) < 3 
	|| ( strcmp(st[0],"sd") && strcmp(st[1],"sd") && strcmp(st[2],"sd") )
	|| ( strcmp(st[0],"sb") && strcmp(st[1],"sb") && strcmp(st[2],"sb") )
	|| ( strcmp(st[0],"fb") && strcmp(st[1],"fb") && strcmp(st[2],"fb") )
	){ /** Se uno degli ultimi tre confronti è vero => esiste un st* diverso da tutte le stringhe accettate
		* Viceversa se l'esito è false allora tutte le stringhe sono ovviamente diverse!
		*/
		fclose(f);
		errno = EBADF; /*Non saprei che errore sia più consono tra i disponibili */ 
		return NULL;
	}
	/* alloco wator*/
	if ( ! ( p = malloc(sizeof(wator_t)) ) ){
		errno = EFAULT;
		fclose(f);
		fclose(f_p);
		return NULL;
	}
	/* inizializzazioni non previste dal file conf*/
	p->plan = NULL;
	p->nf = p->ns = p->nwork = p->chronon = 0;
	/* utilizzo della macro sopra definita per cercare in st il valore "sd" e restituire il rispettivo v */
	p->sd = CERCA("sd", st, v);
	p->sb = CERCA("sb", st, v);
	p->fb = CERCA("fb", st, v);
	/***** PARTE DUBBIA ****/
	if ( ! (p->plan = load_planet( f_p ) ) ) {
		/*errno settato da load*/
		fclose(f_p);
		fclose(f);
		return NULL;
	}
	p->ns = shark_count ( p->plan );
	p->nf = fish_count ( p->plan );
	/************************/
	fclose(f_p);
	fclose(f);
	return p;	
}

void free_wator(wator_t* pw){
	/* ritono se pw non è valido */
	if ( ! pw ) return;
	/* libero la memoria di planet */
	if ( pw->plan ) free_planet ( pw->plan ) ;
	free( pw );
}

/*********************************************************** RULES *******************************************************/

/** La seguente struttura verrà usata per prorogarela diffuzione di un piccolo array lungo MASSIMO MAX_L
 *  n indica quanti di quei MAX_L pos sono valide.
 *  Ogni cella ha due dimensioni: i e j rispettivamente ..[0] e ..[1] 
 * */
#define MAX_L 4
typedef struct {
	uint n;
	int cells[MAX_L][2];
} cell_set;

/** La seguente macro rende valido un index che sfora il range [0,n).
 * *//* deprecata.
#define VALID_INDEX(i, n) ( (i<0) ? ( (i+n)%(n) ) : ( (i)%(n) ) )
*/
#define VALID_INDEX(i,n) ( (i+n) % (n) )

/** \function closeCells
 * 	\param i è il valore y
 * 	\param j è il valore x
 *  \param pred funzione che controlla un predicato sulla cella
 *      La funzione controlla i 4 vicini 
 *            (x-1,y)
 *      (x,y-1) *** (x,y+1)
 *            (x+1,y)
 * 	e su ognuno di essi controlla il predicato pred.
 *  Se su (i,j) il pred è true, essa viene aggiunta all'array di cell_set
 * 	\retval un cell_set che contiene tutte le celle interono ad i,j che soddisfano pred
 * */
cell_set closeCells ( uint i, uint j, planet_t *p, bool(pred)(cell_t) ) {
	cell_set ret ;
	ret.n = 0;
		/* Controllo la cella (x,y-1) */
	ret.cells[ret.n][0] = VALID_INDEX( i-1, p->nrow );
	ret.cells[ret.n][1] = VALID_INDEX( j, p->ncol );
	if ( pred ( p->w[ ret.cells[ret.n][0] ][ ret.cells[ret.n][1] ] ) ) ret.n ++;
		/* Controllo la cella (x,y+1) */
	ret.cells[ret.n][0] = VALID_INDEX( i+1, p->nrow );
	ret.cells[ret.n][1] = VALID_INDEX( j, p->ncol );
	if ( pred ( p->w[ ret.cells[ret.n][0] ][ ret.cells[ret.n][1] ] ) ) ret.n ++;
		/* Controllo la cella (x-1,y) */
	ret.cells[ret.n][0] = VALID_INDEX( i, p->nrow );
	ret.cells[ret.n][1] = VALID_INDEX( j-1, p->ncol );
	if ( pred ( p->w[ ret.cells[ret.n][0] ][ ret.cells[ret.n][1] ] ) ) ret.n ++;
		/* Controllo la cella (x+1,y) */
	ret.cells[ret.n][0] = VALID_INDEX( i, p->nrow );
	ret.cells[ret.n][1] = VALID_INDEX( j+1, p->ncol );
	if ( pred ( p->w[ ret.cells[ret.n][0] ][ ret.cells[ret.n][1] ] ) ) ret.n ++;	
/*	#ifdef _DEBUG_
	printf("(%d,%d) where matrix[%d][%d] => [ ", i ,j, p->nrow, p->ncol );
	for ( i=0;i<ret.n; i++) printf("(%d,%d) ", ret.cells[i][0], ret.cells[i][1] );
	printf("]\n");
	#endif
*/	return ret;
}
/* piccolo set di predicati */
bool pred_is_water ( cell_t c ) {  return c==WATER;  }
bool pred_is_fish  ( cell_t c ) {  return c==FISH;   }
bool pred_is_shark ( cell_t c ) {  return c==SHARK;  }

/** definizione di un random che generi un numero tra 0 ed n */
#define RAND(n) (random() % n)

/**\function setCell
 * \param p puntatore al pianeta sul quale si setta
 * \param i index di row
 * \param j index di col
 * \param c valore da mettere in w[i][j]
 * \param bt valore da mettere in btime
 * \param dt valore da mettere in dtime
 * \retval void
 * */
void setCell ( planet_t *p , int i, int j, cell_t c, int bt, int dt ){
	p->w[i][j] = c;
	p->btime[i][j] = bt;
	p->dtime[i][j] = dt;
}

/**\function moveFromTo
 * \param p puntatore al pianeta sul quale si agisce
 * \param i i di partenza
 * \param j j di partenza
 * \param ti i di destinazione
 * \param tj j di destinazione
 * funzione che sposta in tutte e tre le matrici, il contenuto di ij in ti,tj
 * approcio DISTRUTTIVO : non viene controllato cosa viene sovrascritto!
 * \retval void
 * */
void moveFromTo( planet_t * p , int i, int j, int ti, int tj ){
	/* sovrascrittura nuovi valori */
	setCell( p, ti, tj, p->w[i][j], p->btime[i][j], p->dtime[i][j] );
	/* eliminazione dei vecchi */
	p->w[i][j] = WATER;
	p->btime[i][j] = p->dtime[i][j] = 0;
}
/*****************************************************************************************/

bool procreate ( planet_t *p, int i, int j, int *k, int *l, int s_o_f_b, cell_t type){
	if ( p->btime[i][j]++ == s_o_f_b ){
		cell_set cs ;
		/* è il momento della riproduzione. se btime è == s_o_f_b allora viene inizializzato
		 * se no è stato incrementato*/ 
		p->btime[i][j] = 0; /* inizializzo btime */
		if ( ( cs =  closeCells( i, j, p, pred_is_water ) ).n ) {
			/* c'è spazzio per riprodursi => seleziona cella random */ 
			int *pos = cs.cells[ RAND(cs.n) ];
			/* setta la cella col figlio*/
			setCell( p, pos[0], pos[1], type, 0, 0);
			*k = pos[0];
			*l = pos[1];
			return TRUE;
		}
	}
	return FALSE;	
}


/******************************** RULE 1 *********************************************************/

int shark_rule1 (wator_t* pw, int x, int y, int *k, int* l){
	cell_set cs /* per gestire return di array */;
	int i = x;
	int j = y;
	int *move = NULL; /* sarà occupato da un array di due elementi i e j */
	int ret = STOP; /* valore di return */
	if ( ! pw ) { 
		errno = EFAULT;
		return -1;
	}
	/* ottine un array di celle che contengono pesci */ 
	if ( ( cs = closeCells( i, j, pw->plan, pred_is_fish ) ).n != 0 ) { 
		/*  viene prediletto mangiare */
		/* scelta random di quale mangiare */
		move = cs.cells[ RAND( cs.n ) ];
		/* rimuovo il pesce dal contatore */
		#ifdef _ONLY_ONE_ 
		pw->nf --; 
		#endif
		ret = EAT;
		pw->plan->dtime[i][j] = 0;  /* NON è scritto nel testo, ma dovrebbe ???  */
	}else
		/* Non sono stati trovati pesci, si cercano celle libere */ 
		if ( ( cs = closeCells ( i, j, pw->plan, pred_is_water ) ).n != 0 ){
			move = cs.cells[ RAND (cs.n) ];
			ret = MOVE;
		}
	/* assegnamento risultato ( se move == NULL ) allora la mossa è stop */
	*k = move ? move[0] : i;
	*l = move ? move[1] : j;
	/* aggioramento distruttivo : */
	if ( move ) moveFromTo( pw->plan , i, j, *k, *l );
	return ret;
}

/******************************** RULE 2 *********************************************************/

int shark_rule2 (wator_t* pw, int x, int y, int *k, int* l){
	int i = x; 
	int j = y;
	if ( ! pw ) { 
		errno = EFAULT;
		return -1;
	}
	/* Controllo le nascite */
	if ( procreate( pw->plan, i, j, k, l, pw->sb, SHARK) )
		;/*pw->ns ++; terzo frammento*//* incremento il contatore degli squali */
	/* controllo la morte o meno */
	if ( pw->plan->dtime[i][j]++ == pw->sd ){
		/* rimuovo lo squalo */
		setCell ( pw->plan, i, j, WATER, 0, 0);
		/* lo tolgo dal contatore */
		#ifdef _ONLY_ONE_
		pw->ns --;
		#endif
		return DEAD;
	}
	return ALIVE;
}

/******************************** RULE 3 *********************************************************/
/* simile a rule 1*/
int fish_rule3 (wator_t* pw, int x, int y, int *k, int* l){
	cell_set cs /* per gestire return di array */;
	int i = x; 
	int j = y;
	int *move = NULL; /* sarà occupato da un array di due elementi i e j */
	int ret = STOP; /* valore di return */
	if ( ! pw ) { 
		errno = EFAULT;
		return -1;
	}
	/* prelevo una casella libera a caso */ 
	if ( ( cs = closeCells ( i, j, pw->plan, pred_is_water ) ).n != 0 ){
		move = cs.cells[ RAND (cs.n) ];
		ret = MOVE;
	}
	/* assegnamento risultato ( se move == NULL ) allora la mossa è stop */
	*k = move ? move[0] : i;
	*l = move ? move[1] : j;
	/* aggioramento distruttivo (se è stata selezionata move): */
	if ( move ) moveFromTo( pw->plan , i, j, *k, *l );
	return ret;	
}


/******************************** RULE 4 *********************************************************/
/*sime a rule 2*/
int fish_rule4 (wator_t* pw, int x, int y, int *k, int* l){
	int i = x; 
	int j = y;
	if ( ! pw ) { 
		errno = EFAULT;
		return -1;
	}
	/* Controllo le nascite */
	if ( procreate( pw->plan, i, j, k, l, pw->fb, FISH) )
		#ifdef _ONLY_ONE_
		pw->nf ++; /* incremento il contatore dei pesci */
		#else
		;
		#endif
	return 0;
}

/******************************************* counters ***************************************************/

int count ( planet_t * p , cell_t e ) {
	int i, r = 0;
	if ( ! p ) {
		/* unico caso di errore */
		errno = EFAULT;
		return -1;
	}
	/* per ogni cella di tutta l'area controllo 
	 * Da notare che secondo le assunzioni di new_planet, la matrice è contigua!
	 * */
	for ( i = p->nrow * p->ncol -1 ; i>= 0 ; i-- )
	/* incremento r di 1 se e ed p->w[0][i] sono uguali
	 * */
		r += ! ( e - p->w[0][i] );
	return r;
}

/* delega il conteggio a count che agisce come specificato */
int fish_count (planet_t* p){
	return count ( p, FISH ); 
}

/* delega il conteggio a count che agisce come specificato */
int shark_count (planet_t* p){
	return count ( p, SHARK ); 
}

/********************************* UPDATE ************************************************/

/** calcola un chronon aggiornando tutti i valori della simulazione e il pianeta
   \param pw puntatore al pianeta

   \return 0 se tutto e' andato bene
   \return -1 se si e' verificato un errore (setta errno)

 */
int update_wator (wator_t * pw){
	/* array rispettivamente di shark e fish usati per memorizzare le pos di quelli da muovere!
	 * con rispettive dimensioni
	 * NOTAZIONE : le righe sono i e j, mentre le colonne sono gli animali: ogni animale a due righe*/
	int *stm[2], ns=0;
	int *ftm[2], nf=0;
	int i,j, trash_i, trash_j;
	stm[0]=stm[1]=ftm[0]=ftm[1] = NULL;
	/* alloco e se c'è qualche errore rimuovo tutto. */
	if ( ! pw 
	|| ! ( stm[0] = malloc (sizeof(int)*pw->ns) )
	|| ! ( stm[1] = malloc (sizeof(int)*pw->ns) )
	|| ! ( ftm[0] = malloc (sizeof(int)*pw->nf) ) 
	|| ! ( ftm[1] = malloc (sizeof(int)*pw->nf) ) ) {
		if ( stm[0] ) free ( stm[0] );
		if ( ftm[0] ) free ( ftm[0] );
		if ( stm[1] ) free ( stm[1] );
		if ( ftm[1] ) free ( ftm[1] );
		errno = EFAULT;
		return -1;
	}
	/* la scansione degli animali da muovere avviene prima di muoverli per evitare di muovere due volte lo stesso animale*/
	for (i=0; i<pw->plan->nrow; i++)
		for (j=0; j<pw->plan->ncol; j++)
			switch ( pw->plan->w [i][j] ){
				default : break;
				case SHARK:
					if ( ns == pw->ns ) { /* ATTENZIONE STATO INCONSISTENTE !!! trovati più shark di quanti dovrebbe*/
						free ( stm[0] );
						free ( ftm[0] );
						free ( stm[1] );
						free ( ftm[1] );
						errno = EBADF;
						return -1;
					}
					/* memorizzo le celle */
					stm[0][ ns ] = i;
					stm[1][ ns++ ] = j;
					break;
				case FISH:
					if ( nf == pw->nf ) { /* ATTENZIONE STATO INCONSISTENTE !!! trovati più fish di quanti dovrebbe*/
						free ( stm[0] );
						free ( ftm[0] );
						free ( stm[1] );
						free ( ftm[1] );
						errno = EBADF;
						return -1;
					}
					ftm[0][ nf ] = i;
					ftm[1][ nf++ ] = j;
					break;				
			}
	/* Tutto è andato bene, procedo al movimento dei pezzi. 
	 * Il controllo degli errori delle future chiamate è inutile in quanto già controllato in questa*/
	 /* Applico regola 2. Sovrascrivo stm poichè non ho interesse nel sapere le posizioni da ora in poi */
	for (i = 0; i<ns; i++)
		shark_rule2( pw, stm[0][i], stm[1][i], &trash_i, &trash_j );
	 /* Applico regola 1 */
	for (i = 0; i<ns; i++)
		shark_rule1( pw, stm[0][i], stm[1][i], &trash_i, &trash_j );
	 /* Applico regola 4. Sovrascrivo stm poichè non ho interesse nel sapere le posizioni da ora in poi */
	for (i = 0; i<nf; i++)
		if ( pw->plan->w [ftm[0][i]][ftm[1][i]] == FISH ) 
			fish_rule4( pw, ftm[0][i], ftm[1][i], &trash_i, &trash_j );
	 /* Applico regola 3 */
	for (i = 0; i<nf; i++)
		if ( pw->plan->w [ftm[0][i]][ftm[1][i]] == FISH ) 
			fish_rule3( pw, ftm[0][i], ftm[1][i], &trash_i, &trash_j );
	/* è passato un chronon */
	free ( stm[0] );
	free ( ftm[0] );
	free ( stm[1] );
	free ( ftm[1] );
	return 0;
}


/*************************************************************************************************************************/
#ifdef MAIN
int main(){
	int i = 0;
	wator_t *p = new_wator( "planet.dat");
		
	if ( ! p ) err(1, "Wator");
/*	printf( " debug : \n\t'sd'=%d\n\t'sb'=%d\n\t'fb'=%d\n", p->sd, p->sb, p->fb ); 
*/
	print_planet( stdout, p->plan );	
	while ( /*getchar() != 'q'*/ 1 ) {
		printf ( "---------------------------------------------------------------------------------------\n" );
		update_wator( p );
		print_planet( stdout, p->plan );
		printf ( "%2d) SHARKS : %2d ; \tFISH : %2d \n", ++i, p->ns, p->nf );
		system("sleep 0.4");
	}
	
	free_wator ( p );
		
	return 0;
	
}
#endif




