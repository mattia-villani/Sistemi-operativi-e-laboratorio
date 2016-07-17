/** \file dispacher.c
	\author Mattia Villani
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera
	originale dell' autore.  */
#include "main_header.h"

/*
 * Seguono funzioni di debug, per cui non commentate. (sotto c'è il main_dispacher)
 */
void debug_utils_show_sub_matrix ( sub_planet_t * sub ) {
	int i,j;
	#ifndef _DEBUG_
	return;
	#endif
	printf("printing subplanet : ( %d x %d ) :\n", sub->_nrow, sub->_ncol );
	for ( i=0; i<sub->_nrow+2*WEIGHT; i++ ){
		for ( j=0; j<sub->_ncol+2*WEIGHT; j++ ){
			real_cell_t * c = & (sub->cell[i][j]);	
			int not_my_area = i<WEIGHT || i>=WEIGHT+sub->_nrow || j<WEIGHT || j>=WEIGHT+sub->_ncol ;
			int mutex_not_null = c->mutex!=NULL;
			
			if ( mutex_not_null )printf("(");
			else printf(" ");
			
			if ( not_my_area ) printf("\x1b[47m");			
			
			if ( *(c->state) != UNKNOWN ) printf("\x1b[41m");
			
			switch( * (c->w) ){
				case WATER :printf ("\x1b[34m" "W" "\x1b[0m" );break;
				case SHARK :printf ("\x1b[31m" "S" "\x1b[0m" );break;
				case FISH  :printf ("\x1b[32m" "F" "\x1b[0m" );break;			
			}

			if ( mutex_not_null )printf(")");
			else printf(" ");
						
			printf(" ");
		}
		printf("\n");		
	}

}

void dump_of_subs( ){
	#ifdef _SUB_MATRIX_DEBUG_
	int i;
	for ( i=0 ; i<num_of_subs ; i++ ){
		printf("%d ) ",i);
		debug_utils_show_sub_matrix( sub_planets+i );				
	}
	printf("--- counting : #fish = %d ; #shark = %d \n", ((wator_t*)syc_wator->sharedItem)->nf, ((wator_t*)syc_wator->sharedItem)->ns ); 
	#endif
}


void* main_dispacher( void* args ){
	int i;
	Elem END_EVENT_LOOP = 0;

	/* inizializzo i segnali */ 
	setSignals();

	/* LOOP */
	do
		switch ( (uintptr_t) (sycqueue_dequeue( TO_DISPACHER_QUEUE )) ) {	
			case EVENT_QUEUE_MSG_DISPACHER_UPDATE:
				Log("Dispacher <- MSG_UPDATE", DEBUG, NOPERROR);
				/* metto nel pool le sotto matrici da aggiornare */
				#ifdef _DEBUG_
				dump_of_subs( );
				#endif
				for ( i=0 ; i<num_of_subs ; i++ )
					sycqueue_enqueue( sm_pool, sub_planets+i );											
				break;
			case EVENT_QUEUE_MSG_EXIT: 
				Log("Dispacher <- MSG_EXIT", DEBUG, NOPERROR);
				/* setto il flag di scape dal ciclo degli eventi */
				END_EVENT_LOOP = (Elem)1; 
				/* notifico al collector di terminare */
				sycqueue_enqueue( TO_COLLECTOR_QUEUE, (Elem)EVENT_QUEUE_MSG_EXIT );	
				Log("Dispacher: MSG_EXIT --> Collector",DEBUG, NOPERROR);
				break;
			default: Log("Dispacher <- INVALID MSG", DEBUG, NOPERROR);
		}
	while ( !END_EVENT_LOOP );
	return 0;
}


