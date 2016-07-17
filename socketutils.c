/** \file socketutils.c
	\author Mattia Villani
	Si dichiara che il contenuto di questo file e' in ogni sua parte opera
	originale dell' autore.  */
#include "main_header.h"



/** Instaura una connessione con visualizer e ritorna il descrittore della connessione 
 * 
 */
int create_connection(){
	int i,fd;
	struct sockaddr_un sock_add;
	
	/* definisco l'address del socket */
	strncpy( sock_add.sun_path, SOCK_NAME, UNIX_PATH_MAX );
	sock_add.sun_family = AF_UNIX;
	
	/* creo il socket client side */
	testMinus( fd = socket( AF_UNIX, SOCK_STREAM, 0) , "Socket creation", PERROR );
	
	for ( i=0; i<NUM_OF_TRIAL ; i++ ){
		/* per più volte provo a connettermini al socket server di visualizer */
		if ( ( connect(fd, (struct sockaddr*) &sock_add, sizeof(sock_add) ) ) != -1 )
			/* ho svuto successo => ritorno il file descriptor */
			return fd;
		else
			/* è avvenuto un fallimento, potrebbe esser un caso tollerato */
			if ( errno == ENOENT || errno == ECONNREFUSED ) 
				/* visualizer potrebbe non esser ancora pronto => riprovo aspettando */
				sleep( DELAY );
			else 
				/* errore non previsto */
				Log("Socket connection",FATAL,PERROR);
	}
	/* sono terminati i tentativi disponibili => visualizer è inraggiugibile */
	Log("Socket connection: max num of hit reached", FATAL, NOPERROR);
	return -1; /* non raggiungibile */
}

/** Chiude il processo visualizer, cioè gli scrive sul socket il comando SOCK_CMD_EXIT
 */
void closeVisualizer( ){
	/* apre la connessione ed assegna il suo file descriptor ad fd */
	int fd = create_connection();
	/* definisco il comando */
	SOCK_CMDS cmd = SOCK_CMD_EXIT;
	/* invio la richiesta */
	write(fd, &cmd, sizeof(SOCK_CMDS));
	/* chiudo la connessione */
	close (fd);
}

void visualizer_init( int nrow, int ncol ) {
	SOCK_CMDS cmd;
	/* apre la connessione ed assegna il suo file descriptor ad fd */
	int fd = create_connection();
	/* scrivo al socket il comando di init con la descrizione del pianeta */
	cmd = SOCK_CMD_INIT;
	write(fd, &cmd, sizeof(SOCK_CMDS));
	write(fd, &nrow, sizeof(int) );
	write(fd, &ncol, sizeof(int) );
	/* chiudo la connessione */
	cmd = SOCK_CMD_QUIT;
	write(fd, &cmd, sizeof(SOCK_CMDS));
	/* chiusura di questo lato della connessione e liberazione della memoria */
	close(fd);
}

void show(cell_t*w, int nrow, int ncol){
	SOCK_CMDS cmd;
	/* apre la connessione ed assegna il suo file descriptor ad fd */
	int fd = create_connection();
	int area = nrow*ncol;
	int bits_len;
	int seq_len ;
	/* creo un buffer in cui scrivere l'immagine del pianeta (la dimensione è area/4 arrotondata per eccesso)*/
	bits_t *buff = testedMalloc( sizeof(bits_t)*( (area+4)>>2 ) ); /* (area+4)>>2 lunghezza massima */
	/* creo l'immagine (comprimendo il pianeta) e memorizzo la sua lunghezza */
	bits_len = compress( buff, w, area );
	/* stabilisco quanti byte trasmettere */
	seq_len = top(bits_len, 4);
		
	/* scrivo al socket il comando show ed invio l'immagine */
	cmd = SOCK_CMD_SHOW_AND_QUIT;
	write(fd, &cmd, sizeof(SOCK_CMDS));
	write(fd, &bits_len, sizeof(int) ); /* dimensione dell'immagine (bits) */
	write(fd, &seq_len , sizeof(int) ); /* byte occupati dall'immagine */
	write(fd, buff, seq_len*sizeof(bits_t) );
	
	/* chiusura di questo lato della connessione e liberazione della memoria */
	close(fd);
	free( buff );
}


/* deprecated */
void show_stepped(cell_t*w, int nrow, int ncol){
	SOCK_CMDS cmd;
	/* apre la connessione ed assegna il suo file descriptor ad fd */
	int fd = create_connection();
	int area = nrow*ncol;
	int bits_len;
	int seq_len ;
	/* creo un buffer in cui scrivere l'immagine del pianeta (la dimensione è area/4 arrotondata per eccesso)*/
	bits_t *buff = testedMalloc( sizeof(bits_t)*( (area+4)>>2 ) ); /* (area+4)>>2 lunghezza massima */
	/* creo l'immagine (comprimendo il pianeta) e memorizzo la sua lunghezza */
	bits_len = compress( buff, w, area );
	/* stabilisco quanti byte trasmettere */
	seq_len = top(bits_len, 4);
	
	/* Quella che segue è una gestione frammentata della comunicazione,
	 * che potrebbe esser unificata da un'unico comando, ma per un ipotetica versabilità futura
	 * le varie fasi sono tenute distinte! 
	 */
	
	/* scrivo al socket il comando di init con la descrizione del pianeta */
	cmd = SOCK_CMD_INIT;
	write(fd, &cmd, sizeof(SOCK_CMDS));
	write(fd, &nrow, sizeof(int) );
	write(fd, &ncol, sizeof(int) );
	
	/* scrivo al socket il comando show ed invio l'immagine */
	cmd = SOCK_CMD_SHOW;
	write(fd, &cmd, sizeof(SOCK_CMDS));
	write(fd, &bits_len, sizeof(int) ); /* dimensione dell'immagine (bits) */
	write(fd, &seq_len , sizeof(int) ); /* byte occupati dall'immagine */
	write(fd, buff, seq_len*sizeof(bits_t) );
	
	/* chiudo la connessione */
	cmd = SOCK_CMD_QUIT;
	write(fd, &cmd, sizeof(SOCK_CMDS));
	
	/* chiusura di questo lato della connessione e liberazione della memoria */
	close(fd);
	free( buff );
}





