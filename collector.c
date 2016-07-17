/** \file collector.c
	\author Mattia Villani
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera
	originale dell' autore.  */
#include "main_header.h"


void* main_collector( void* args ){
	Elem END_EVENT_LOOP = 0;
	int *wid;
	int count = 0;
	int update_passed = 0;
	wator_t *wat = (wator_t*)syc_wator->sharedItem ;
	
	/* inizializzo i segnali */ 
	setSignals();
	
	/* ciclo degli eventi */
	do
		switch ( (uintptr_t) (wid = sycqueue_dequeue( TO_COLLECTOR_QUEUE )) ) {	
			/* estraggo il comando */
			case EVENT_QUEUE_MSG_EXIT: 
				Log("Collector <- MSG_EXIT", DEBUG, NOPERROR);
				/* termino uscendo dal ciclo degli eventi */
				END_EVENT_LOOP = (Elem)1; break;
			case EVENT_QUEUE_MSG_SHOW:
				Log("Collector <- MSG_SHOW", DEBUG, NOPERROR);
				/* ricebuta la richiesta di visualizzare il pianeta, invio la sua immagine a visualizer*/
				show( wat->plan->w[0], wat->plan->nrow, wat->plan->ncol );			
				/* dopo aver fatto la visualizzazione posso richiedere un nuovo aggiornamento. */	
				sycqueue_enqueue( EVENT_QUEUE, (Elem) EVENT_QUEUE_MSG_REQUEST_UPDATE );
				Log("Collector: MSG_REQUEST_UPDATE --> Main thread",DEBUG, NOPERROR);
				break;
			case EVENT_QUEUE_MSG_LAST_SHOW:
				Log("Collector <- MSG_LAST_SHOW", DEBUG, NOPERROR);
				/* ricebuta la richiesta di visualizzare il pianeta, invio la sua immagine a visualizer*/
				show( wat->plan->w[0], wat->plan->nrow, wat->plan->ncol );				
				break;
			default: 
				/*Log("Collector <- Worker", DEBUG, NOPERROR );*/
				/* verifico che l'update non sia terminato, se lo fosse lo notifico al dispacher */
				if ( (++count) == num_of_subs ){
					/* ho ricevuto il lavoro da tutti i worker, non ne dovrebbe arrivare più nessun altro */
					count = 0;
					
					/* faccio il clear dello status delle celle nelle zone condivse ( ora tutti i worker hanno finito di lavorare ) */
					while ( ! sycqueue_isEmpty( toClean ) )
						* (cell_state_t*) sycqueue_dequeue( toClean ) = UNKNOWN;
						
					/* controllo se sia il caso di visualizzare la matrice */
					if ( ( ++update_passed ) == wat->chronon ){
						update_passed = 0;
						/* mi segno di dover visualizzare */
						sycqueue_enqueue( TO_COLLECTOR_QUEUE, (Elem)EVENT_QUEUE_MSG_SHOW );
						Log("Collector: MSG_SHOW --> Collector",DEBUG, NOPERROR);
					}else		
						/* non devo visualizzare la matrice, posso inviare la richiesta di update al main */
						sycqueue_enqueue( EVENT_QUEUE, (Elem) EVENT_QUEUE_MSG_REQUEST_UPDATE );
					Log("Collector: MSG_REQUEST_UPDATE --> Main thread",DEBUG, NOPERROR);
				}
				break;
		}
	while ( !END_EVENT_LOOP );

	return 0;
}


