/** \file visualizer.c
	\author Mattia Villani
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera
	originale dell' autore.  */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <wait.h>
/*Includo le funzionalità di core ( servirà sycqueue e decompres ) + costanti */
#include "core.h"

/* le memorizzo solo una volta nel caso di init */
int nrow, ncol;

/*Abbreviazione*/
#define READ(fd,ind,dim) (checked_read(fd,ind,dim))

/** Funzione che effettua una read controllando che essa avvenga in maniera corretta
 *	Lancia Log come FATAL nel caso in cui il numero di byte atteso non corrisponda a quello letto
 *	param fd: file descriptor da cui leggere
 *	param ind: area di memoria da riempire ( deve esser stata preallocata )
 *	param dim: numero di byte atteso MAGGIORE di 1 
 */
void checked_read(int fd,void *ind,int dim){
	/* effettuo la lettura trammite sc e memorizzo il numero di byte letti */
	int nbr = read(fd,ind,dim) ;
	/* discrimino i casi sulla base di quanti byte ho letto */
	switch(nbr){ 
		/* read ritorna 0 quando raggiunto l'end of file, quindi non è stato letto nulla */
		case 0: 	Log("Unexpected end of sock", FATAL, NOPERROR); break;
		/* gestione di errore */
		case -1: 	Log("Error read",FATAL,PERROR); break; 
		/* ho letto nbr>0 byte => controllo che corrispondano a quanti ne volevo! */
		default: 	if ( nbr != dim ) Log("Read unsufficient bytes", FATAL, NOPERROR); break; 
	}	
}

/** Funzione che gestisce il ciclo di fetching dei comandi che arrivano dalla socket.
 *	Si basa sulla filosofia generale espressa in wator.c ovvero un ciclo degli eventi.
 *	param fd: file descriptor della socket 
 *	param outstream: stream su cui scrivere la matrice quando ricevuto il comando di show 
 *	retval: 1 se ricevuto exit, 0 altrimenti
 */
int fetching ( int fd, FILE * outstream ) { 
	SOCK_CMDS cmd;	/* comando da estrarre */
	int seq_len, bits_len,i; /* lunghezza in byte della sequenza, lunghezza in bits ~ seq_len /4 */
	bits_t *buff = NULL;
	cell_t *matrix = NULL;

	/* ciclo degli eventi */	
	while ( 1 ){
		/* estraggo un comando dalla socket */
		READ( fd, &cmd, sizeof(cmd) );
		/* decodifica del comando */
		switch ( cmd ) {
			case SOCK_CMD_EXIT:
			 	Log("server <- SOCK_CMD_EXIT",DEBUG, NOPERROR);
				/* libero la memoria */
				if ( buff ) free( buff );
				if ( matrix ) free( matrix );
				/* => devo terminare il processo => retval 1 */
			 	return 1; break;				
			 /* e' stato richiesto una init*/
			case SOCK_CMD_INIT: 
				Log("server <- SOCK_CMD_INIT",DEBUG, NOPERROR);
				/* prelevo la descrizione del mondo */
				READ ( fd, &nrow, sizeof( int ) );
				READ ( fd, &ncol, sizeof( int ) );
				/* controllo la validità della descrizione */
				if ( nrow<1 || ncol<1 ) 
					Log("Recived too small dims", FATAL, NOPERROR); 
				/* creo i buffer temporanei */
			case SOCK_CMD_SHOW_AND_QUIT:
				/* array del tipo arrotondamento per eccesso della dimensione del mondo/4*/
				buff = testedMalloc( sizeof(bits_t)*( (nrow*ncol+4) >> 2 ) );
				/* array grande quanto il mondo */
				matrix = testedMalloc( sizeof(cell_t)*nrow*ncol );
				/* nel caso del comando composto procedo alla visualizzazione, se no sono nella init */
				if ( cmd != SOCK_CMD_SHOW_AND_QUIT ) break;			
			/* è stato richiesto di visualizzare il mondo */
			case SOCK_CMD_SHOW: 
				Log("server <- SOCK_CMD_SHOW",DEBUG, NOPERROR);
				/* nel caso stia visualizzando su file, lo riscrivo dalla cima */
				if ( outstream != stdout ) rewind( outstream );
				/* controllo usi insoliti del "protocollo" */
				if ( ! buff ) Log("Sock un-init, but show, called",FATAL,NOPERROR);
				/* estraggo l'immagine del mondo */
				READ ( fd, &bits_len, sizeof( int ) );	
				READ ( fd, &seq_len, sizeof( int ) );	
				READ ( fd, buff, seq_len );
				
				/* ripristino il formato I(plan) a plan */
				decompress( matrix, buff, bits_len );

				/* print di di plan */
				fprintf (outstream, "%d\n%d\n", nrow, ncol);
				for (i=0; i<nrow; i++){
					int j;
					for (j=0; j<ncol; j++){
						#ifndef COLOR_DEBUG
						fprintf(outstream, "%c%c", cell_to_char( matrix[i*ncol+j] ), (j==ncol-1)?'\n':' ' ); 
						#else
						switch ( matrix[i*ncol+j] ) {
							case WATER :fprintf (outstream, "\x1b[34m" "W " "\x1b[0m" );break;
							case SHARK :fprintf (outstream, "\x1b[31m" "S " "\x1b[0m" );break;
							case FISH  :fprintf (outstream, "\x1b[32m" "F " "\x1b[0m" );break;
						}
						#endif
					}
					#ifdef COLOR_DEBUG
					fprintf(outstream, "\n");
					#endif
				}
				/* end print */			
				if ( cmd != SOCK_CMD_SHOW_AND_QUIT ) break;			
			/* è' stato richiesto di chiudere la connessione */ 
			case SOCK_CMD_QUIT: 
				Log("server <- SOCK_CMD_QUIT",DEBUG, NOPERROR);
				/* libero la memoria */
				if ( buff ) free( buff );
				if ( matrix ) free( matrix );
				/* => non devo terminare il processo => retval 0 */
				return 0; break;	
			/* Possono esser rivenuti solo i comandi precedenti! */
			default: Log("Recived unknown comand on sock", FATAL, NOPERROR); break;
		}			
	}
}

int main( int argc, char **argv ){
	/* fd della socket */
	int sfd;
	/* address della socket */
	struct sockaddr_un sock_add;
	/* guardia del ciclo degli eventi */
	int REQUIRE_EXIT = 0;
	/* assegnazione di default ad outstream ( ridefinita se passato un file ) */
	FILE *outstream = stdout;
	
	/* controllo che non sia stato passato un file come primo argomento*/
	if ( argc >= 2 && argv[1] )
		/* lo apro come file di dump! */
		testNull ( outstream = fopen( argv[1], "w+" ) , "FOPEN", PERROR);
	
	/* inizializzazione dei signal handler */
	{		
		struct sigaction s; 
		memset( &s, 0, sizeof(s) );
		s.sa_handler = SIG_IGN;
		/* Il comportamento per i seguenti segnali è ignorarli. */
		/* La terminazione è gestita esclusivamente mediante il protocollo wator <-> visualizer */
		testMinus( sigaction( SIGINT, &s, NULL ) , "SIGACTION", PERROR);
		testMinus( sigaction( SIGTERM, &s, NULL ) , "SIGACTION", PERROR);
		testMinus( sigaction( SIGALRM, &s, NULL ) , "SIGACTION", PERROR);		
		testMinus( sigaction( SIGUSR1, &s, NULL ) , "SIGACTION", PERROR);		
	}
	
	/* creazione del socket */
	/* elimino un eventuale vechio file rimasto per sbaglio */
	unlink( SOCK_NAME );
	/* battezzo la socket */
	strncpy( sock_add.sun_path, SOCK_NAME, UNIX_PATH_MAX );
	/* socket con lettura e scrittura dalla stessa macchina */
	sock_add.sun_family = AF_UNIX;
	
	/* creo la socket */
	testMinus( sfd = socket( AF_UNIX, SOCK_STREAM, 0) , "Socket creation", PERROR );
	/* effettuo il binding della socket con il suo address */
	testMinus( bind ( sfd, (struct sockaddr*)&sock_add, sizeof(sock_add) ), "Binding", PERROR );	
	/* dichiaro che la socket accetta connessioni! */
	testMinus( listen ( sfd, MAX_CONNECTIONS ), "Listening", PERROR ); 
	
	/* fetching; contenitore dell'event loop */
	do {
		/* file descriptor su cui comunicare dopo l'accettazione */
		int fdc;
		/* sta in attesa di una connessione da wator e poi la accetta */
		fdc = accept ( sfd, NULL, 0 );
		/* eseguo il fetching dei comandi da wator */
		REQUIRE_EXIT = fetching( testMinus( fdc , "Accepting", PERROR ), outstream );
		/* chiudo la comunicazione con wator*/
		close(fdc);
	}while ( !REQUIRE_EXIT );
	
	/* end */
	/* chiudo la socket */
	close( sfd );
	/* nel caso in cui il file di output sia stato definito, lo chiudo */
	if ( outstream != stdout ) fclose( outstream );
	/* rimuovo il file usato come socket */
	unlink( SOCK_NAME );	
	return EXIT_SUCCESS;
}


