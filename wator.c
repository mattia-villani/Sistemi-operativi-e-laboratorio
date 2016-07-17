/** \file wator.c
	\author Mattia Villani
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera
	originale dell' autore.  */

#include "main_header.h"

/**************************************************************************************
 *						FILOSOFIA GENERALE DELLA SINCRONIZZAZIONE:
 *
 *		Ispirata alla gestione degli eventi dei sistemi grafici
 *
 *		Ogni thread che viene creato compreso quello principale del processo,
 *	ed il processo visualizer, si basano su un ciclo degli eventi:
 *	la comunicazione tra le varie componenti avviene mediante:
 *		-> delle code condivise nel caso della comunicazione tra thread
 *		-> della socket per la comunicazione tra il thread principale e visualizer.
 *		
 *						Descrizione del ciclo degli eventi:
 *		Ogni componente avrà la sua funzione di attivazione ( i rispettivi main ),
 *	all'interno dei quali il componente entrerà in un ciclo "infinito",
 *	durante il quale ad ogni iterazione, sia nel caso delle code che della socket,
 *	farà le seguenti azioni (ovviamente ogni risorsa ha un solo consumatore, ovvero il
 *	componente a cui è diretto il messaggio ):
 *		1) estrarrà un comando dalla risorsa coi seguenti possibili comportamenti:
 *			-> la risorsa è vuota 
 *				=> il componente si mette in attesa fin tanto che la risorsa rimane tale
 *			-> la risorsa non è vuota
 *				=> viene estratto il messaggio, che rappresenterà un comando.
 *		2) il messaggio estratto verrà "decodificato" nel senso associato ad un comando.
 *		3) se il comando necessità di argomenti aggiuntivi, verranno presi come segue:
 *			-> la risorsa ha un solo produttore 
 *				=> la risorsa potrà esser veicolo degli argomenti passati al componente 
 *					per assolvere al comando. es: thread main --(socket)--> visualizer 
 *			-> la risorsa ha più produttori 
 *				=> la risorsa non può esser usata come veicolo poichè potrebbe accadere
 *					che un'altro produttore inserisca i propri messaggi in mezzo agli 
 *					argomenti. Questa situazione potrebbe esser risolta estendendo la 
 *					zona critica durante la quale scrivere nella risorsa quindi con 
 *					una complicazione della scrittura ed un rischio di lock annidate,
 *					col rischio di deadlock. 
 *					Politica usata:
 *						-> variabili globali 
 *						-> le due componenti ricorrono ad un'altra risorsa per condividere
 *							gli argomenti. es: dispacher e collector usano il pool di worker (deprecato).
 *		4) vengono testate evetuali variabili globali. es: main thread testa _SIG_*
 *		5) se il comando ricevuto non è exit, la componente ripete questo ciclo.
 *	
 *		Le componenti che dunque figurano sono le seguenti:
 *			-> wator_process 
 *				-> Main_thread ( crea le le due successive e visualizer )
 *				-> Dispacher_thread	( crea i workers )
 *				-> Collector_thread
 *				-> Worker_i_thread (con i da 1 ad nwork )
 *			-> visualizer_process
 *
 *		Giustificazioni:
 *			Perchè il Main_thread ? 
 *				Il M_T potrebbe essere il dispacher, ma è una componente introdotta 
 *				per ridurre il carico del dispacher, per aumentarne la leggibilità
 *				e per avere una gestione del processo ( in termini di ciclo di vita )
 *				non mischiata alla gestione del pianeta wator:
 *					Il M_T si occupa di gestire i segnali e di dialogare con visualizer.
 *					Chiaramente il D_T sarà in wait durante tale dialogo, ma l'aver separato
 *					le componenti che se ne occupano, permette di non sporcare il ciclo 
 *					degli eventi del Dispacher con operazioni non inerenti alla gestione dei workers. 
 *			Perchè una coda e non una pipe come risorsa di dialogo ?
 *				Il motivo è che una coda degli eventi realizzata mediante coda condivisa
 *				potrebbe esser più efficiente di una pipe (non necessaria):
 *					-> Pipe: ogni trasmissione richiederebbe di passare mediante SC
 *					-> Code: in una implementazione migliore rispetto all'attuale
 *						(cioè in cui ad ogni enqueue non corrisponda una malloc)
 *						si avrebbe ovvi vantaggi in performance poichè non si passa da SC
 *
 *							DINAMICA DI UNA SINCRONIZZAZIONE:
 *		Sequenza di eventi ( "<--" avrà il significato di "riceve il messaggio" ): 
 *			-> il M_T <-- EVENT_QUEUE_MSG_REQUEST_UPDATE
 *				=> se
 *					| M_T <-- _SIG_EXIT 
 *						1)	EVENT_QUEUE_MSG_SHOW --> C_T
 *						2)	EVENT_QUEUE_MSG_EXIT --> M_T
 *					| M_T <-- _SIG_ALARM
 *						1) 	dump di plan.
 *						2)	EVENT_QUEUE_MSG_DISPACHER_UPDATE --> D_T
 *					| _
 *						1) 	EVENT_QUEUE_MSG_DISPACHER_UPDATE --> D_T
 *				-> Il C_T <-- EVENT_QUEUE_MSG_SHOW 
 *					=> C_T crea un'immagine del pianeta I(plan) :
 *							#connessione al socket  
 *						1) SOCK_CMD_INIT 		--(socket)--> visualizer
 *						2) Descrizione(plan) 	--(socket)--> visualizer
 *						3) SOCK_CMD_SHOW 		--(socket)--> visualizer
 *						4) I(plan) 				--(socket)--> visualizer
 *						5) SOCK_CMD_QUIT 		--(socket)--> visualizer
 *							#chiusura della connessione  
 *				-> il D_T <-- EVENT_QUEUE_MSG_DISPACHER_UPDATE
 *					-> foreach sbi in { sub_matrix } do worker_i <- dequeue ( workers_pool ) 
 *						-> Immagine ( sbi ) --> worker_i 
 *						Quindi :
 *							-> worker_i <-- I(sbi)
 *								1) aggiorna sbi
 *								2) worker_i --> C_T
 *							-> C_T <-- worker_i
 *								=> se
 *									| #thread ricevuti != #{sub_matrix} 
 *										=> #thread ricevuti++;
 *									| #thread ricevuti == #{sub_matrix} 
 *										=> 	EVENT_QUEUE_REQUEST_UPDATE --> M_T
 *								-> #update fatti == #chronon 
 *									=> EVENT_QUEUE_MSG_SHOW --> C_T
 *			-> il M_T <-- EVENT_QUEUE_MSG_EXIT
 *				1) EVENT_QUEUE_MSG_EXIT --> D_T 
 *					-> il D_T <-- EVENT_QUEUE_MSG_EXIT 
 *						1) foreach worker_i do EVENT_QUEUE_MSG_EXIT --> worker_i
 *							-> il worker_i <-- EVENT_QUEUE_MSG_EXIT => termina.
 *						2) EVENT_QUEUE_MSG_EXIT --> C_T	=> termina.
 *						3) termina.
 *				2) SOCK_CMD_EXIT --(socket)--> visualizer
 *					-> il visualizer <-- SOCK_CMD_EXIT => termina.
 *				3) termina.
 *		
 *		Inoltre chiunque riceva un segnale, lo convoglia nel M_T
 *
 *
 *
 *		------ il codice è stato scritto con tabs di dimensione 4 spazi monospace ------
 *					------ il codice è stato scritto su ubuntu 15.04 ------
 ***************************************************************************************/

/* threads: D_T e C_T */
pthread_t t_dispacher;
pthread_t t_collector;


/* variabili settate degli handler dei segnali e consumate da M_T s*/
volatile sig_atomic_t _SIG_EXIT, _SIG_ALARM;

/* ---------------------------------------------------------------------------------- */

/* 		Handler dei segnali di significato ovvio. Benchè molto simili, gli handler sono 
 *	divisi per chiarezza.
 */

static void handler_sigint( int sig ){
	_SIG_EXIT=1; 
	#ifdef _DEBUG_ 
		/* in assenza del define, si ha un handler ancora più minimale! */
		Log("Cought SIGINT",DEBUG,NOPERROR);
	#endif
}
static void handler_alarm( int sig ){
	_SIG_ALARM=1; 
	#ifdef _DEBUG_ 
		Log("Cought ALARM",DEBUG,NOPERROR);
	#endif
}
static void handler_check( int sig ){
	/* il segnale ha la stessa semantica di SIGALARM */
	_SIG_ALARM=1; 
	#ifdef _DEBUG_ 
		Log("Cought CHECK",DEBUG,NOPERROR);
	#endif
}

/** Funzione che inisizlizza le maschere dei segnali ed associa gli handler
 */
void setSignals(){		
	/* Segnali, assegno i vari handler */
	struct sigaction s; 
	memset( &s, 0, sizeof(s) );
	s.sa_handler = handler_sigint;
	testMinus( sigaction( SIGINT, &s, NULL ) , "SIGACTION", PERROR);
	testMinus( sigaction( SIGTERM, &s, NULL ) , "SIGACTION", PERROR);
	s.sa_handler = handler_alarm;
	testMinus( sigaction( SIGALRM, &s, NULL ) , "SIGACTION", PERROR);		
	s.sa_handler = handler_check;
	testMinus( sigaction( SIGUSR1, &s, NULL ) , "SIGACTION", PERROR);		
	Log("Signal setted",DEBUG,NOPERROR);
}

/* ---------------------------------------------------------------------------------- */

void initializer ( wator_t *wat ) {
	/* indici di supporto */
	int I,J,index = 0;
	/* conterrà poi la matrice di mutex*/
	Mutex *mts;
	/* matrice di stati */
	cell_state_t *dnm;
	int area;	
	
	Log("Init starting...", DEBUG,NOPERROR);
	
	/*
	 *	Global structures
	 */	
	{/* Inizializzazione delle variabili globali di tipo coda e segnali*/
		/* creo un contenitore per wator, mi servirà per aggiornare i contatori */
		syc_wator = syc_create(wat);
		/* pool di sotto matrici */
		sm_pool = sycqueue_create();
		/* code degli eventi */
		EVENT_QUEUE = sycqueue_create();
		TO_COLLECTOR_QUEUE = sycqueue_create();
		TO_DISPACHER_QUEUE = sycqueue_create();
		/* inizializzo le variabili che segnano un segnale */
		_SIG_EXIT = 0;
		_SIG_ALARM = 0;
		/* ed inizializzo le due code di appoggio, toFree memorizza le cose da rimuovere nella destroy */
		toFree = queue_create();
		/* ricorda qualle celle vanno resettate ad ogni update */
		toClean = sycqueue_create();
		/* il numero di sotto matrici è dato dal prodotto delle dimensioni divise per K ed N */
		num_of_subs = top( wat->plan->nrow , K )*top( wat->plan->ncol , N );
		/* creo l'array di sotto pianeti */
		sub_planets = testedMalloc ( sizeof( sub_planet_t ) * num_of_subs );
	}/* fine inizializzazione variabili globali ^ */
	
	/*
	 *	Workers
	 */
	{/* creo un array di worker */
		int i;
		workers = testedMalloc(sizeof(worker_t)*wat->nwork);			
		/* inizializzo l'array di worker */
		for( i=0 ; i<wat->nwork ; i++ ){
			/* memorizzo che worker sia così che esso possa saperlo */
			workers[i].wid = i;
			/* creo il thread e gli passo il proprio descrittore */
			if ( pthread_create( &(workers[i].thread), NULL, main_worker, & (workers[i].wid) ) )
				Log("Creating worker", FATAL, NOPERROR);
		}
	}/* fine init workers */

	#define VALID_INDEX(i,n) ( (i+n) % (n) )
	
	/* dimensione della matrice originale */
	area = wat->plan->ncol * wat->plan->nrow;
	/* creo e registro come da liberare, una matrice di stati linearizzata */
	enqueue( toFree , dnm = testedMalloc(sizeof(cell_state_t)*area) );
	/* creo una matrice linearizzata di puntatori a mutex come appoggio */
	mts = testedMalloc( sizeof(Mutex)*area );
	/* inizializza le nuove matrici */
	for (I=0;I<area;I++){ mts[I] = NULL; dnm[I] = UNKNOWN; }
	Log("Allocated mutex array and state array", DEBUG,NOPERROR);
	
	/* disegno nella matrice, una serie di cornici di mutex, di spessore 2*WEIGHT */	
	for( I=0; I<wat->plan->nrow; I+=K ){ /* cornici orizzontali */
		int j,x;
		for( x=I-WEIGHT; x<I+WEIGHT; x++){
			int i=VALID_INDEX( x, wat->plan->nrow );
			for(j=0; j<wat->plan->ncol; j++)
				/* oltre che creare la mutex, la inserisco in una coda di roba da deallocare successivamente */
				enqueue( toFree, mts[i*wat->plan->ncol+j] = testedMalloc(sizeof(pthread_mutex_t)) );
		}
	}
	for ( J=0; J<wat->plan->ncol; J+=N ){ /* cornici verticali */
		int i,x;
		for( x=J-WEIGHT; x<J+WEIGHT; x++){
			int j=VALID_INDEX( x, wat->plan->ncol );
			for(i=0; i<wat->plan->nrow; i++)
				if ( ! mts[i*wat->plan->ncol+j] ) /* controllo di non riscrivere su una mutex già creata */
				/* oltre che creare la mutex, la inserisco in una coda di roba da deallocare successivamente */
					enqueue( toFree, mts[i*wat->plan->ncol+j] = testedMalloc(sizeof(pthread_mutex_t)) );
		}
	}
	/* inizializzo le mutex */
	for (I=0;I<area;I++) if(mts[I]) pthread_mutex_init( (mts[I]) ,NULL);
	Log("Init mutexs done", DEBUG,NOPERROR);
	/* fine disegno */

	/* inizializzo le sotto matrici */	
	for( I=0; I<wat->plan->nrow; I+=K )
		for ( J=0; J<wat->plan->ncol; J+=N ){
			int i,j;
			/* per ogni sotto matrice */
			sub_planet_t *sub_plan = sub_planets + ( index++ );
			/* definisco la dimensione ( gli estremi: destro, basso ed angolo tra questi due potrebbe avere dimensioni strane )*/
			sub_plan -> _nrow = MIN( wat->plan->nrow-I , K );
			sub_plan -> _ncol = MIN( wat->plan->ncol-J , N );
			/* riempio la sotto matrice con riferimenti alla matrice originale compresi i bordi */
			for ( i=I-WEIGHT; i< I+sub_plan->_nrow + WEIGHT ; i++ )
				for ( j=J-WEIGHT; j< J+sub_plan->_ncol + WEIGHT ; j++ ){
					/* indici all'interno della sottomatrice */
					int v_i = i-(I-WEIGHT); 
					int v_j = j-(J-WEIGHT);
					/* indici reali all'interno della matrice di wator */
					int r_i = VALID_INDEX( i, wat->plan->nrow );
					int r_j = VALID_INDEX( j, wat->plan->ncol );
					/* alias per comodità */
					real_cell_t * rc = & ( sub_plan->cell[v_i][v_j] );
					/* ricordo la cella a cui è associata questa meta cella */
					rc->i = r_i;
					rc->j = r_j;
					/* copio un riferimento al contenuto della matrice ( per leggibilità dopo ) */
					rc->w = & (wat->plan->w[r_i][r_j]);
					/* gli associo la mutex e lo stato dalle matrici create prima */
					rc->mutex = mts[ r_i*wat->plan->ncol + r_j ] ;
					rc->state = dnm+( r_i*wat->plan->ncol + r_j );
					/* salvo il riferimento */
					rc->pw = wat;
				}
		}
	Log("Init sub plantets done", DEBUG,NOPERROR);
	
	/* da il via al ciclo di update */
	sycqueue_enqueue( EVENT_QUEUE , (Elem)EVENT_QUEUE_MSG_REQUEST_UPDATE ); 
	
	/* libero la matrice di mutex, poichè i riferimenti sono già stati salvati */
	free(mts);
	return;
}
void destroy() {
	{ /* distruggo i workers */
		int i;
		for( i=0 ; i<((wator_t*)syc_wator->sharedItem)->nwork ; i++ )
			/* ad ogni worker, notifico di terminare */
			sycqueue_enqueue( sm_pool, (Elem) EVENT_QUEUE_MSG_EXIT );
		/* aspetto che terminino */
		for( i=0 ; i<((wator_t*)syc_wator->sharedItem)->nwork ; i++ )
			if ( pthread_join ( workers[i].thread, NULL ) ) Log("Join w", FATAL, PERROR);
		/* rilascio l'array di worker */
		free( workers );
	} /* fine distruzione workers */

	/* per ogni elemento registrato nella toFree, effettuo la free */
	while ( isEmpty(toFree) == 0 )
		free ( dequeue( toFree, NULL ) );
	/* dealloco l'array, distruggo la coda toFree e quella toClean */
	free( sub_planets );
	queue_destroy( toFree );
	sycqueue_destroy( toClean );
	
	/* distruggo propriamente le strutture create sulle code globali */
	sycqueue_destroy( EVENT_QUEUE );
	sycqueue_destroy( TO_COLLECTOR_QUEUE );
	sycqueue_destroy( TO_DISPACHER_QUEUE );
	sycqueue_destroy( sm_pool );
	/* distruggo wator ed il suo contenitore */
	free_wator((wator_t*)syc_wator->sharedItem);
	syc_destroy( syc_wator );
}

int main(int argc, char** argv ){
	/* variabili nwork e chronon con valori di default*/
	int nwork=WORK_DEF, chronon=CHRON_DEF;
	/* puntatori all'argomento che contiene il nome dei file */
	char *dumpfile=NULL, *file=NULL;
	/* file descriptor del file in cui fare il wator_check */
	FILE *fd_wator_check;
	/* riferimento al processo che verrà creato */
	pid_t visualizer;
	/* puntatore al pianeta */
	wator_t * wat;
	/* variabile di escape dall'event loop */
	Elem END_EVENT_LOOP = 0;	
	
	/* INIZIO INIT */
	Log("-------- WELCOME ------",DEBUG,NOPERROR);
	{/* leggo gli argomenti */
		int opt;
		while ((opt = getopt(argc, argv, "n:v:f:")) != -1) 
			/* per ogni opzione tra n,v,f (ognuna con un argomento) */
			switch (opt) {
				/* -n trovata, assegno a nwork la conversione a numero dell'argomento */
				case 'n': nwork = atoi(optarg); 
					/* controllo che nwork sia significativo */
					if (nwork<1) Log("Necessaio almeno un worker",FATAL,NOPERROR);break;
				/* -v trovata, assegno a chronon, la conversione a numero dell'argomento */
				case 'v': chronon = atoi(optarg); 
					/* controllo che chronon sia significativo */
					if (chronon<1) Log("Necessaio almeno un chronon",FATAL,NOPERROR);break;
				/* -f trovata, salvo il riferimento al file di dump */
				case 'f': dumpfile = optarg; break;
				/* bad usage */
				default: fprintf(stderr, HELP_MSG); exit(EXIT_FAILURE); break;
			}
	}/* fine lettura delle opzioni */
	/* controllo ci sia almeno un'altro argomento ( deve esserci ) e lo assegno al file */
	if (optind < argc) file = argv[optind];
	else Log ( "Expected file name", FATAL, NOPERROR);
	
	/* verifico la validità dei vari argomenti */
	{	/* il file di input deve esser conforme al modello di pianeta */	
		char comand[1025];
		sprintf(comand, "./watorscript %s 2>/dev/null", file);
		if ( testMinus( system( comand ) , "Error invoking watorscript", PERROR ) )
			/* il ritorno del comando è non EXIT_SUCESS => errore */
			Log("Input file didn't pass the watorscript test", FATAL, NOPERROR );
		else Log("watorscript test done", DEBUG,NOPERROR);
	}	/* fine test script */

	/* wator.conf deve essere accedibile in lettura */
	testMinus ( access(CONFIGURATION_FILE, O_RDONLY ), CONFIGURATION_FILE , PERROR );
	/* se dumpfile definito allora deve essere accedibile in scrittura */
	if ( dumpfile ) { 
		int fd;
		/* se la open fallisce allora il file non è accedibile (se non esiste lo crea )*/
		testMinus ( fd = open(dumpfile, O_WRONLY | O_CREAT, 0666 ), "aprendo il file di dump" , PERROR );	
		close ( fd );
	}
	/* apro il file di check */
	testNull( (fd_wator_check = fopen ( WATORCHECK, "w+" )) , "wator_check open", PERROR );	
	/* cambio i permessi del file di check nel caso in cui lo abbia creato. permessi 0666 */
	testMinus( chmod ( WATORCHECK, S_IWOTH | S_IROTH | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP ) , "wator_check chmod", PERROR );	
	
	Log("Controls over the input done", DEBUG,NOPERROR);
	
	/* creo wator con file come file da cui attingere la descrizione di planet */
	wat = testNull( new_wator( file ), "Creating planet", NOPERROR );
	/* inizializzo i valori nwork e chronon passati come argomenti */
	wat->nwork = nwork;
	wat->chronon = chronon;	
	/* richiedo di inizializzare le sotto matrice sulla base di wat e le variabili globali */
	initializer ( wat );
	Log("Init done",DEBUG,NOPERROR);
	/* INIT FINITA */ 
	
	/* creo visualizer */
	/* effettuo la fork */
	testMinus( visualizer = fork(), "Can't Fork", PERROR );	
	if ( visualizer == 0 ){
		/* qui sono le figlio => specializzo il processo ad essere visualizer */
		execl("./visualizer","visualizer", dumpfile,NULL);		
		/* execl non ha funzionato! */
		testNull( NULL , "EXEC" , PERROR );
	}
	/* comunico a visualizer la dimensione della matrice per ridurre la comunicazione dopo */
	visualizer_init( wat->plan->nrow, wat->plan->ncol );
	Log("Visualizer created",DEBUG,NOPERROR);
	/* sono in wator */

	/* Segnali, assegno i vari handler */
	setSignals();
	
	/* creo i thread dispacher e collector */
	if ( pthread_create( &t_dispacher, NULL, main_dispacher, NULL )) Log("Create thread", FATAL, NOPERROR ); 
	if ( pthread_create( &t_collector, NULL, main_collector, NULL)) Log("Create thread", FATAL, NOPERROR ); 
	Log("Thread created",DEBUG,NOPERROR);
	
	/* avvio il timer */
	/*alarm ( SEC );		*/
							
	Log("Starting event loop",DEBUG,NOPERROR);
	/* event loop */
	do{
		switch ( (uintptr_t) (sycqueue_dequeue( EVENT_QUEUE )) ) {	
			/* estraggo un messaggio */
			case EVENT_QUEUE_MSG_EXIT: 
				Log("EventLoop <- MSG_EXIT", DEBUG, NOPERROR);
				/* ricevuta la richiesta di exit => la inoltro al dispacher */
				sycqueue_enqueue( TO_DISPACHER_QUEUE, (Elem)EVENT_QUEUE_MSG_EXIT );
				/* setto a true la fine del event loop */
				END_EVENT_LOOP = (Elem)1; break;
			case EVENT_QUEUE_MSG_REQUEST_UPDATE:
				Log("EventLoop <- REQUEST_UPDATE", DEBUG, NOPERROR);
				/* richiesto un update : significa che il precedente è terminato 
				 * è un buon momento per verificare un eventuale temrminazione gentile o una visualizzazione  */
				if ( _SIG_EXIT ){
					Log("Processing EXIT SIGNAL", DEBUG,NOPERROR);
					/* verifico se sia arrivato un segnale di exit, cioè i sigterm o sigint.
					 * se così fosse richiedo l'ultima visualizzazione e poi richiedo la terminazione */
					/* acquisisco esplicitamente la lock sulla coda */
					syc_lock ( EVENT_QUEUE );
					/* la svuoto */
					queue_clear( EVENT_QUEUE->sharedItem );
					/* inserisco il nuovo ed unico elemento */
					enqueue( EVENT_QUEUE->sharedItem, (Elem) EVENT_QUEUE_MSG_EXIT);
					/* esco dall'area protetta */
					syc_unlock( EVENT_QUEUE );
					/* richiedo al collector la visualizzazione */
					sycqueue_enqueue( TO_COLLECTOR_QUEUE, (Elem) EVENT_QUEUE_MSG_LAST_SHOW);
					/* ripristino _SIG_EXIT anche se da ora in poi non dovrebbe più esser letta */
					_SIG_EXIT = 0;	
				}	
				else{
					/* continuo la vita di wator */
					if( _SIG_ALARM ){
						/* è scattato un allarme, quindi faccio il dump del pianeta sul file di check */
						int i,j;
						/* reset di sig_alarm */
						_SIG_ALARM = 0;
						Log("EventLoop processing alarm", DEBUG, NOPERROR);
						/* riposiziono il cursore all'inizio del file => sovrascrittura */
						rewind(fd_wator_check);
						/* print */
						fprintf (fd_wator_check, "%d\n%d\n", wat->plan->nrow, wat->plan->ncol);
						for (i=0; i<wat->plan->nrow; i++)
							for (j=0; j<wat->plan->ncol; j++)
								fprintf(fd_wator_check, "%c%c", 
									cell_to_char( wat->plan->w[i][j] ), 
									(j==wat->plan->ncol-1)?'\n':' ' ); 
						/* fine print */
						/* richiedo che parta un'allarme tra poco */
						alarm ( SEC );								
					}
					#ifdef _DEBUG_
					getchar();
					#endif
					/* la richiesta è stata accettata ed inoltro al dispacher */
					sycqueue_enqueue( TO_DISPACHER_QUEUE, (Elem)EVENT_QUEUE_MSG_DISPACHER_UPDATE );			
				}
				break;
			/* in ogni altro caso ignoro il messaggio */
			default: Log("EventLoop <- INVALID MSG", DEBUG, NOPERROR); break;
		}
	/* ripeto fino a che non ho ricevuto una EXIT request */
	}while ( !END_EVENT_LOOP );
	
	Log("Event_loop ended",DEBUG,NOPERROR);

	/* chiudo il file di check */
	fclose(fd_wator_check);

	
	/* epilogo */	
	/* attendo che i vari thread abbiano terminato dopo avegli inviato l'exit */
	if ( pthread_join( t_dispacher, NULL ) ) Log("Join", FATAL, NOPERROR);
	if ( pthread_join( t_collector, NULL ) ) Log("Join", FATAL, NOPERROR);
	Log("Threads closed", DEBUG,NOPERROR);
		
	/* invio a visualizer la richiesta di terminazione */
	closeVisualizer();
	/* attendo che abbia concluso la sua terminazione*/
	waitpid( visualizer, NULL, 0 );	
	Log("Visualizer terminated",DEBUG,NOPERROR);
	
	/* distruggo ciò che la initializer ha creato */
	destroy();
	Log("Destruction done",DEBUG,NOPERROR);
	
	/* la terminazione è avvenuta correttamente */
	return EXIT_SUCCESS;
}
