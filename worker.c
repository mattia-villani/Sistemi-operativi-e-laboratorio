/** \file worker.c
	\author Mattia Villani
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera
	originale dell' autore.  */
#include "main_header.h"

/* enumeratore che rappresenterà il movimento dell'animale */
typedef enum{
	NORD,SUD,OVEST,EST,CENTRO
}direction_t;

/** Funzione che data una posizione iniziale e finale restituisce la direzione
 *	param (i,j) : posizione iniziale 
 *	param (fi,fj) : posizione finale 
 *	retval : direzione dello spostamento
 */
direction_t find_dir( int i, int j, int fi, int fj ) {
	if ( i == fi ){
		/* se mi sono mosso, mi sono mosso orizzontalmente */
		if ( j == fj ) return CENTRO; /* non mosso */
		if ( j<fj ){
			/* ho la posizione finale più a destra, se la distanza è 1 sono andato ad est
			 * altrimenti sono andato ad ovest, ma andando a -1, sono ho ottenuto fj > j (calcolo in modulo) */
			if ( fj == j+1 ) return EST;
			else return OVEST;
		}
		if ( j == fj+1 ) return OVEST;
			else return EST;	
	}else{
		/* casistica analoga alla precedente sull'asse verticale */
		if ( i<fi ){
			if ( fi == i+1 ) return SUD;
			else return NORD;
		}
		if ( i == fi+1 ) return NORD;
			else return SUD;		
	}
}

/** Essenzialmente una map sull'array close di lunghezza d
 *	param close: array su cui fare la map
 *	param d: dimenzione di close 
 *	param fun: funzione da applicare 
 *	ATTENZIONE : 
 *		lo scopo è solo fare la lock o la unlock 
 */
void do_on_cells( real_cell_t* close[], int d, void(*fun)(Mutex) ){
	/* OSSERVAZIONE : le lock sono fatte sempre nel solito ordine.
	 * I FILOSOFI : non è possibile la deadlock perchè le lock fatte sono
	 *  in ordine ed inoltre l'area lockata non occupa tutta la sub matrix 
	 *	per cui al massimo 3 sotto matrici possono avere dell'area inusabile.
	 *	non è dunque possibile che ogni sotto matrice sia in attesa di qualche dun altra
	 */
	int i;
	for ( i=0; i<d; i++ )
		if ( close[i]->mutex )
			fun( close[i]->mutex );	
}

/** versione safe di un incremento di un contatore di wator.
 *	param ref: puntatore al contatore da variare
 *	param value: quanto modificare il contatore
 */
void inc_ref( int *ref, int value ){
	/* entro in una zona safe da race condition per evitare che due thread applichino
	 * una modifica allo stesso contatore contemporanemanete
	*/
	syc_lock(syc_wator); 
	/*safe*/
	/* modifico il contatore */
	*ref=*ref+value; 
	syc_unlock(syc_wator);
}

/**	Inizializza l'array close preallocato con il rombo avente centro in I,J su c
 *	param close: array da riempire 
 *	param (I,J): posizione intorno alla quale tracciare il rombo
 *	param c: matrice sulla quale disegnare il rombo
 */
void do_assign_round ( real_cell_t* close[], int I, int J, real_cell_t c[][N+2*WEIGHT] ){
	int i,j,index=0,d;
	/* piccolo check, poichè la funzione si presta ad adattarsi qualora WEIGHT diventasse due ( moviemento e poi riproduzione )*/
	if ( WEIGHT != 1 ) Log ("ATTENZIONE : do assign round fatta supponendo WEIGHT 1", FATAL, NOPERROR );
	/* disegno effettivamente il rombo */
	for ( i=I-WEIGHT, d=0; i<=I+WEIGHT; i++, d = WEIGHT - abs(I-i) )
		for ( j=J - d ; j<=J + d ; j++ )
			close[index++] = &( c[i][j] );
}


/** Nuova versione di update wator che non opera sull'intera matrice ma solo su un sub_planet
 *	param sub_plan: sotto pianeta su cui operare
 */
void sub_update_wator( sub_planet_t *sub_plan ){
	int I, J;
	real_cell_t *cell;
	
	/* per rispettare il secondo frammento, non effettuo l'update */
	#ifdef _DO_NOT_UPDATE_
	return;
	#endif
	
	/* per ogni riga della sotto matrice (area viola + gialla indiata nella documentazione) */
	for(I=WEIGHT; I<sub_plan->_nrow+WEIGHT; I++)
		/* e per ogni colonna, sempre nell'area di proprietà di questa sotto matrice */
		for( J=WEIGHT; J<sub_plan->_ncol+WEIGHT; J++ ){
			/* creo un array di supporto di celle che mi rappresentano il rombo */
			/* la casistica è fatta per cambiare agevolmente l'ordine delle regole */
			const int dim = (WEIGHT==1)?5:13;
			real_cell_t* close[(WEIGHT==1)?5:13];
			/* le inizializzo e poi effettuo la lock */
			do_assign_round ( close, I, J, sub_plan->cell );
			do_on_cells( close, dim , lock );
			/* safe */
			cell = &(sub_plan->cell[I][J]);
			if ( * (cell->w) != WATER && *(cell->state) == UNKNOWN ){ 
				/* se lo stato è UNKNOWN allora la cella non è stata mossa da nessuno */
				int i= cell->i;
				int j= cell->j;
				/* indici all'interno della sotto matrice */
				int v_i,v_j;
				/* temine del movimento */
				int dest_i = i, dest_j = j;
				/* posizione dei figli */
				int son_i = i, son_j = j;
				/* osservo quale animale sia */
				cell_t type = *(cell->w);
				/* flag se mi dice se è morto di vecchiaiai */
				int dead = 0;
				
				/* applico le regole, verificando che non ci siano errori */
				if ( type == FISH ) { 
					testMinus( fish_rule4( cell->pw, i, j, &son_i, &son_j ), "Applaying fish rule 4", NOPERROR);
					testMinus( fish_rule3( cell->pw, i, j, &dest_i, &dest_j ), "Applaying fish rule 3", NOPERROR);
				}else{ /* SHARK */
					int action ;
					/* guardo lo squalo sia morto */
					dead = DEAD==testMinus( shark_rule2( cell->pw, i, j, &son_i, &son_j ), "Applaying shark rule 2", NOPERROR);
					if ( ! dead ) {
						/* lo squalo è ancora in vita, quindi lo muovo */
						action = testMinus( shark_rule1( cell->pw, i, j, &dest_i, &dest_j ), "Applaying shark rule 1", NOPERROR);				
						if ( action == EAT ) /* se ha mangiato devo ridurre il numero dei pesci */
							inc_ref( &(cell->pw->nf) , -1 );
					}
				}
				
				/* guardo se effettivamente un figlio sia nato */
				if ( son_i != i || son_j != j ) {
					switch ( find_dir( i,j, son_i,son_j ) ){
						/* scopro in che posizione sia stato messo e setto lo stato nella cella corrispondente */
						case NORD:	*(sub_plan->cell[v_i = I-1][v_j = J].state) = CREATED; break;								
						case SUD:	*(sub_plan->cell[v_i = I+1][v_j = J].state) = CREATED; break;								
						case EST:	*(sub_plan->cell[v_i = I][v_j = J+1].state) = CREATED; break;								
						case OVEST:	*(sub_plan->cell[v_i = I][v_j = J-1].state) = CREATED; break;
						default : v_i=I; v_j=J; break;												
					}				
					/* incremento il contatore che conta quell'animale */
					inc_ref( ((type==FISH)?&(cell->pw->nf):&(cell->pw->ns)) , +1 );
					/* se è in un area condivisa, delego al collector di pulire l'etichetta */
					if ( sub_plan->cell[v_i][v_j].mutex ) sycqueue_enqueue( toClean, sub_plan->cell[v_i][v_j].state );
				}
				
				/* se l'animale non è morto ( il pesce mai, lo squalo potrebbe )*/
				if ( ! dead ){
					switch ( find_dir( i,j, dest_i,dest_j ) ){
						/* trovo la cella virtuale in cui è finito lo squalo e cambio lo stato */
						case NORD:	*(sub_plan->cell[v_i = I-1][v_j = J].state) = MOVED; break;								
						case SUD:	*(sub_plan->cell[v_i = I+1][v_j = J].state) = MOVED; break;								
						case EST:	*(sub_plan->cell[v_i = I][v_j = J+1].state) = MOVED; break;								
						case OVEST:	*(sub_plan->cell[v_i = I][v_j = J-1].state) = MOVED; break;
						default : v_i=I; v_j=J; break;												
					}
					/* delego al collector di pulire lo stato se in un area condivisa */
					if ( sub_plan->cell[v_i][v_j].mutex ) sycqueue_enqueue( toClean, sub_plan->cell[v_i][v_j].state );
				}else
					/* diminuisco il contatore degli squali, poichè uno è morto */
					inc_ref( &(cell->pw->ns) , -1 );
			}
			do_on_cells( close, dim , unlock );
			/* unsafe */
		}
	/* ripulisco gli stati se le celle sono in zone non condivise (area gialla della documentazione) */
	for(I=WEIGHT*2; I<sub_plan->_nrow; I++)
		for( J=WEIGHT*2; J<sub_plan->_ncol; J++ )
			if ( sub_plan->cell[I][J].mutex == NULL ) /* per esser sicuro che non siano in zone condivise (inutile) */
				*(sub_plan->cell[I][J].state) = UNKNOWN;
}


void* main_worker( void* args ){
	Elem read ;
	/* prendo dagli argomenti la propria struttura di worker */
	int wid = *(int*)args;
	
	/* inizializzo i segnali */ 
	setSignals();

	{	/* creazione del file di dump */
		/* touch + file name */
		char cmd[100];
		sprintf( cmd, "touch wator_worker_%d", wid );
		/* creo il file vuoto del thread */
		system( cmd );
	}

	do
		if (( read = sycqueue_dequeue( sm_pool ) )) {	
			/* Ho ricevuto la richiesta di elaborare una sotto matrice */ 
			sub_planet_t *sub_plan = read;
			/* aggiorno la sotto matrice */
			sub_update_wator( sub_plan );
			/* comunico al collector che ho finito */
			sycqueue_enqueue( TO_COLLECTOR_QUEUE, &wid );
		}/* else ho ricevuto EVENT_QUEUE_MSG_EXIT */
	while ( read );
	
	Log("Worker has done",DEBUG,NOPERROR);
	return 0;
}


