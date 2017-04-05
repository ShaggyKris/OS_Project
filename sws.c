/* 
 * File: sws.c
 * Author: Alex Brodsky
 * Purpose: This file contains the implementation of a simple web server.
 *          It consists of two functions: main() which contains the main 
 *          loop accept client connections, and serve_client(), which
 *          processes each client request.
 */



#include "network.h"
#include "sws.h"

#define MAX_HTTP_SIZE 8192                 /* size of buffer to allocate */
#define TIME_QUANTUM 4096					//Length of time for Round Robin CPU time
#define NUM_THREADS 2

struct Queue *WorkQueue;
struct Queue *SJF;
struct Queue *RR;

pthread_mutex_t signal, enqueue_m, process_m;
pthread_cond_t sig_no_work;

int global_counter;
//int RCB_elements = 0;

/* This function takes a file handle to a client, reads in the request, 
 *    parses the request, and sends back the requested file.  If the
 *    request is improper or the file is not available, the appropriate
 *    error is sent back.
 * Parameters: 
 *             fd : the file descriptor to the client connection
 * Returns: None
 */
void printQueue(struct Queue *q){
	if(q->size <= 0){
		printf("\nThere are no items in this list!\n");
	}
	struct RCB* rcb = q->head;
	while(rcb!=NULL){
		printf("\tSequence: %d\tClientFD: %d\tRemaining Bytes: %d\n",rcb->sequence, rcb->clientfd, rcb->remainingBytes);
		rcb = rcb->next;
	}
}
void init(void){
	WorkQueue = (struct Queue*)malloc(sizeof(struct Queue));
	SJF = (struct Queue*)malloc(sizeof(struct Queue));
	RR = (struct Queue*)malloc(sizeof(struct Queue));
	global_counter = 0;
	WorkQueue->name = "WorkQueue";
	SJF->name = "SJF";
	RR->name="RR";
} 
 
static void serve_client( int fd ) {
  struct RCB* rcb = (struct RCB*)malloc(sizeof(struct RCB));
 
  static char *buffer;                              /* request buffer */
  char *req = NULL;                                 /* ptr to req file */
  char *brk;                                        /* state used by strtok */
  char *tmp;                                        /* error checking ptr */
  FILE *fin;                                        /* input file handle */
  int len;                                          /* length of data read */

  if( !buffer ) {                                   /* 1st time, alloc buffer */
    buffer = malloc( MAX_HTTP_SIZE );
    if( !buffer ) {                                 /* error check */
      perror( "Error while allocating memory" );
      abort();
    }
  }

  memset( buffer, 0, MAX_HTTP_SIZE );
  if( read( fd, buffer, MAX_HTTP_SIZE ) <= 0 ) {    /* read req from client */
    perror( "Error while reading request" );
    abort();
  } 

  /* standard requests are of the form
   *   GET /foo/bar/qux.html HTTP/1.1
   * We want the second token (the file path).
   */
  tmp = strtok_r( buffer, " ", &brk );              /* parse request */
  if( tmp && !strcmp( "GET", tmp ) ) {
    req = strtok_r( NULL, " ", &brk );
  }
  
  if( !req ) {                                      /* is req valid? */
    len = sprintf( buffer, "HTTP/1.1 400 Bad request\n\n" );
    write( fd, buffer, len );                       /* if not, send err */
  } else {                                          /* if so, open file */
    req++;                                          /* skip leading / */
    fin = fopen( req, "r" );                        /* open file */
    if( !fin ) {                                    /* check if successful */
      len = sprintf( buffer, "HTTP/1.1 404 File not found\n\n" );  
      write( fd, buffer, len );                     /* if not, send err */
    } else {                                        /* if so, send file */
      len = sprintf( buffer, "HTTP/1.1 200 OK\n\n" );/* send success code */
      write( fd, buffer, len );

/*      do {                                          // loop, read & send file */
/*        len = fread( buffer, 1, MAX_HTTP_SIZE, fin );  // read file chunk */
/*        if( len < 0 ) {                             // check for errors */
/*            perror( "Error while writing to client" );*/
/*        } else if( len > 0 ) {                      // if none, send chunk */
/*          len = write( fd, buffer, len );*/
/*          if( len < 1 ) {                           // check for errors */
/*            perror( "Error while writing to client" );*/
/*          }*/
/*        }*/
/*      } while( len == MAX_HTTP_SIZE );              // the last chunk < 8192 */
      
      //Somewhere in here, add in RCB allocation and enqueueing
      fseek(fin, 0L, SEEK_END);
      int size = ftell(fin);
      rewind(fin);      
      
      rcb->remainingBytes = size;
      rcb->clientfd = fd;
      rcb->quantum = 4028;
      rcb->file = fin;
      rcb->sequence = ++global_counter;
      
      enqueue(WorkQueue,rcb);
      
      
      //fclose( fin );
    }
  }
  //close( fd );                                     /* close client connectuin*/
}



	


// struct RCB *head = NULL;
// struct RCB *tail = NULL;

//ENSURE MUTEX WHEN CALLING THIS
void enqueue(struct Queue *q, struct RCB *rcb)
{
	rcb->next = NULL;
	if(q->head == NULL && q->tail == NULL)
	{
		q->head = rcb;
		q->tail = rcb;
		q->size++;
	}
	else{
		q->tail->next = rcb;
		q->tail = rcb;
		q->size++;
	}	
    printf("\nEnqueueing in %s\tSequence: %d\tClientFD: %d\tRemaining Bytes: %d\n",q->name,q->tail->sequence, q->tail->clientfd, q->tail->remainingBytes);
	return;
}

struct RCB* dequeue(struct Queue *q){
	struct RCB *rcb = q->head;
	
	if(q->head == NULL){
		printf("\nERROR YOU SUCK\n");
		exit(0);
	}
	
	if(q->head == q->tail){ 
		q->head = q->tail = NULL;
	}
	
	else
		q->head = q->head->next;
	printf("\nDequeueing from %s\tSequence: %d\tClientFD: %d\tRemaining Bytes: %d\n",q->name,rcb->sequence, rcb->clientfd, rcb->remainingBytes);
	q->size--;
	return rcb;
}

struct RCB* dequeue_at(struct Queue *q, int shortest){
	
	if(q->head == NULL){
		return NULL;
	}
/*	if(q->head->next == NULL)*/
/*		return dequeue(q);*/
	
	
	struct RCB* temp = q->head;
	struct RCB* node = NULL;
	//struct RCB* prev = NULL;
	
	if(q->head->remainingBytes == shortest){		
		return dequeue(q);
	}
	
	while(temp->remainingBytes != shortest && temp != NULL){
		if(temp->next->remainingBytes == shortest){
			//printf("\n
			node = temp->next;
			temp->next = temp->next->next;
			break;			
		}
		temp = temp->next;	
	}
	
	if(temp == NULL){
 		printf("\nError dequeing RCB %d\n", shortest);
		exit(0);
	}
	q->size--;
	printf("\nDequeueing from %s\tSequence: %d\tClientFD: %d\tRemaining Bytes: %d\n",q->name,node->sequence, node->clientfd, node->remainingBytes);
	return node;
}

void enqueueSJF(void){
	struct RCB* rcb;
		
	if(WorkQueue->head != NULL)
		rcb = WorkQueue->head;
		
	int shortest = rcb->remainingBytes;
		
	for(int i=0; rcb != NULL && i < WorkQueue->size; i++){
		if(shortest > rcb->remainingBytes){
			shortest = rcb->remainingBytes;			
		}
		rcb = rcb->next;
	}
	
	printf("\nSmallest = %d\n",shortest);
	enqueue(SJF, dequeue_at(WorkQueue, shortest) );	
}

int processSJF(struct RCB* rcb){
	static char *buffer; 
	
	buffer = malloc(MAX_HTTP_SIZE);
	int len;
	
	do {                                          /* loop, read & send file */
		len = fread( buffer, 1, MAX_HTTP_SIZE, rcb->file );  /* read file chunk */
		
		if( len < 0 ) {                             /* check for errors */
			perror( "Error while writing to client" );
		} 
		else if( len > 0 ){                      /* if none, send chunk */
			len = write( rcb->clientfd, buffer, len );
			
			if( len < 1 ) {                           /* check for errors */
				perror( "Error while writing to client" );
			}
		}	
	} while( len == MAX_HTTP_SIZE );              /* the last chunk < 8192 */
	
	fclose(rcb->file);
	close(rcb->clientfd);
	free(buffer);
	return 1;	
}

void enqueueRR(void){
	while(WorkQueue->size!=0){
		enqueue(RR,dequeue(WorkQueue));
	}
}

void *thread_SJF(void *name){
	printf("\nI am thread %d.\n",name);
	fflush(stdout);
	//struct RCB* rcb;
	
	while(1){
		
		printQueue(SJF);
		
		printf("\nWaiting for work.\n");
		fflush(stdout);	
		pthread_cond_wait(&sig_no_work,&signal);
		
		while(WorkQueue->head != NULL){
			pthread_mutex_lock(&enqueue_m);
			//printf("\n\tHey I'm putting shit into SJF queue.\n");
			if(WorkQueue->head == NULL){
				pthread_mutex_lock(&enqueue_m);
				break;
			}
			
			enqueueSJF();
			
			pthread_mutex_unlock(&enqueue_m);			
		}					
		while(SJF->head != NULL){
			
			pthread_mutex_lock(&enqueue_m);
			if(SJF->head == NULL){
				pthread_mutex_unlock(&enqueue_m);
				break;
			}
			printf("\n\tHey, Thread %d working on stuff from SJF queue.\n",name);
			fflush(stdout);			
			
			processSJF(dequeue(SJF));
			pthread_mutex_unlock(&enqueue_m);			
		}							
	}
	
	pthread_exit(NULL);
}

/*void *thread_RR(void *name){*/

/*}*/

/*void *thread_MLFQ(void *name){*/

/*}*/


/* This function is where the program starts running.
 *    The function first parses its command line parameters to determine port #
 *    Then, it initializes, the network and enters the main loop.
 *    The main loop waits for a client (1 or more to connect, and then processes
 *    all clients by calling the serve_client() function for each one.
 * Parameters: 
 *             argc : number of command line parameters (including program name
 *             argv : array of pointers to command line parameters
 * Returns: an integer status code, 0 for success, something else for error.
 */
int main( int argc, char **argv ) {
  int port = -1;                                    /* server port # */
  int fd;                                           /* client file descriptor */
	init();
	setbuf(stdout, NULL);
	
	
  /* check for and process parameters 
   */
  if( ( argc < 2 ) || ( sscanf( argv[1], "%d", &port ) < 1 ) ) {
    printf( "usage: sms <port>\n" );
    return 0;
  }

 	network_init( port );                             /* init network module */
	pthread_mutex_init(&enqueue_m, NULL);
	pthread_mutex_init(&process_m, NULL);
	pthread_mutex_init(&signal, NULL);
	
	pthread_t threads[NUM_THREADS];
	
	for(int i = 0; i < NUM_THREADS; i++){
		if(pthread_create(&threads[i],NULL,thread_SJF,(void*)i) != 0){
			printf("\nError creating thread %d\n", i);
			return 1;		
		}
	}
	for( ;; ) {                                       /* main loop */
		printf("\nWaiting\n");
		network_wait();                                /* wait for clients */
	
		for( fd = network_open(); fd >= 0; fd = network_open() ) { /* get clients */
		  serve_client( fd ); 
		  fflush(stdout);                          /* process each client */
		}
		
		printQueue(WorkQueue);
		puts("\n");
	/*    while(WorkQueue->size!=0)*/
	/*      dequeue(WorkQueue);*/		
		pthread_cond_signal(&sig_no_work);	
/*    switch(argv[2]){*/
/*    	case "SJF":*/
/*    		*/
/*			break;*/
/*		default:*/
/*    }*/
	}
	for(int i = 0; i < NUM_THREADS; i++){
		pthread_join(threads[i],NULL);
	}
  	
}
