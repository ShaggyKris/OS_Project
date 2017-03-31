/* 
 * File: sws.c
 * Author: Alex Brodsky
 * Purpose: This file contains the implementation of a simple web server.
 *          It consists of two functions: main() which contains the main 
 *          loop accept client connections, and serve_client(), which
 *          processes each client request.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "network.h"

#define MAX_HTTP_SIZE 8192                 /* size of buffer to allocate */
#define TIME_QUANTUM 4096					//Length of time for Round Robin CPU time


static struct Queue SJF;
static struct Queue RR;

//int RCB_elements = 0;


/* This function takes a file handle to a client, reads in the request, 
 *    parses the request, and sends back the requested file.  If the
 *    request is improper or the file is not available, the appropriate
 *    error is sent back.
 * Parameters: 
 *             fd : the file descriptor to the client connection
 * Returns: None
 */
static void serve_client( int fd ) {
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

      do {                                          /* loop, read & send file */
        len = fread( buffer, 1, MAX_HTTP_SIZE, fin );  /* read file chunk */
        if( len < 0 ) {                             /* check for errors */
            perror( "Error while writing to client" );
        } else if( len > 0 ) {                      /* if none, send chunk */
          len = write( fd, buffer, len );
          if( len < 1 ) {                           /* check for errors */
            perror( "Error while writing to client" );
          }
        }
      } while( len == MAX_HTTP_SIZE );              /* the last chunk < 8192 */
      
      //Somewhere in here, add in RCB allocation and enqueueing
      
      fclose( fin );
    }
  }
  close( fd );                                     /* close client connectuin*/
}


struct RCB{
	int sequence;
	//File Descriptor (network_wait()?)
	FILE* file;
	int remainingBytes;
	int quantum;
	struct RCB* next;
};

struct Queue{
	struct RCB *head;
	struct RCB *tail;
	int size;
};
	


// struct RCB *head = NULL;
// struct RCB *tail = NULL;

//ENSURE MUTEX WHEN CALLING THIS
void enqueue (struct Queue **q, struct RCB *rcb)
{
  rcb->next = NULL;
  if(q->head == NULL && q->tail == NULL)
  {
    q->head = rcb;
    q->tail = rcb;
    q->size++;
    return;
  }
  (q->tail)->next = rcb;
  q->tail = rcb;
  q->size++;
}

struct RCB* dequeue(struct Queue q){
	struct RCB *rcb = q->head;
	if(q->head == NULL) return q->head;
	if(q->head == q->tail){ 
		q->head = q->tail = NULL;
	else head = head->next;
	return rcb;
}

// struct RCB* dequeue(struct RCB *item, struct RCB *head){
// 	struct RCB *rcb;
// 	if(*head == NULL) return *head;
// 	if(*head->next == NULL){
// 		rcb = *head;
// 		*head = NULL;
// 		return rcb;
// 	}
// 	if(*head->next == item){
// 		*head->next = item->next;
// 	}
// 	 
// }

void RoundRobin(struct Queue q){
	struct RCB *rcb;
	
	while(head != NULL){
		rcb = dequeue(q);
		//Work on rcb until time quantum has been filled
		//While working on it, reduce the rcb->remainingBytes
		if(rcb->remainingBytes > 0)
			enqueue(q,rcb);		
	}
}

void SJF(struct Queue q){
	struct RCB *rcb,*temp;
	
	int shortest_quantum = -1;
	
	if(head != NULL)
		rcb = head;
	
	while(head != NULL){	
		rcb = head;
		while(rcb != NULL){
			if(shortest_quantum < rcb->quantum || shortest_quantum == -1)
				shortest_quantum = rcb->quantum;
			rcb = rcb->next;
		}
		rcb = head;
		
		while(rcb-> != NULL){
			
			
			if(head->quantum == shortest_quantum)
				
		}
}

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
	
	
  /* check for and process parameters 
   */
  if( ( argc < 2 ) || ( sscanf( argv[1], "%d", &port ) < 1 ) ) {
    printf( "usage: sms <port>\n" );
    return 0;
  }

  network_init( port );                             /* init network module */

  for( ;; ) {                                       /* main loop */
    network_wait();                                 /* wait for clients */

    for( fd = network_open(); fd >= 0; fd = network_open() ) { /* get clients */
      serve_client( fd );                           /* process each client */
    }
  }
}
