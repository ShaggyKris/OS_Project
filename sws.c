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

typedef struct{
  int seq;
  int clientfd;
  FILE* file;
  int remainingB;
  int quantum;
  int notFin;//forRR
}RCB;

//RCB table 
RCB *rcbTable[64];
int globalCounter;

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
  //create RCB for this request 
  RCB *newRCB=(RCB*)malloc(sizeof(RCB));
 
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
			write( fd, buffer,len );
			/*Initializes the RCB*/
			newRCB->seq = ++globalCounter;
      newRCB->clientfd = fd;
      fseek(fin, 0L, SEEK_END);
      int sz = ftell(fin);
      rewind(fin);
      newRCB->remainingB = sz;
      newRCB->quantum=4028;
      newRCB->file=fin;
      newRCB->notFin=0;//not finished 
      //add to rcb table 
      rcbTable[globalCounter-1]=newRCB;

    }
  }
  
  
  
}
/*This function is to sort the RCB table.
  At this point, insertion sort is used to sort the job from shortest
  to longest.
*/

int runSJF(){
	//sort the array of RCB 
	RCB *shortest;
	int i,j=0;
	
	//insertion sort 
	for (i=1;i<globalCounter;i++){
		shortest=rcbTable[i];
		for(j=i;j>0 && rcbTable[j-1]->remainingB > shortest ->remainingB; j--){
			rcbTable[j]=rcbTable[j-1];
		}
		rcbTable[j]=shortest;
	}
	
	//process request
	for(i=0;i<globalCounter;i++){
		//for debug 
		printf("Job seq %d\nLength %d\n",rcbTable[i]->seq,rcbTable[i]->remainingB);
		fflush(stdout);
		//call process routine for each rcb stored in table 
		procSJF(rcbTable[i]);
	}
	//for debug 
	puts("\n");
	
	//initializes the counter for next use 
   globalCounter=0;
	return 0;
}

/*
This is the function which actually process the request in rcb table
This function takes in RCB and process the request.
*/

int procSJF(RCB *element){
	static char *buffer;
	int len;
	buffer = malloc( MAX_HTTP_SIZE );

	     
  do {                                          /* loop, read & send file */
    len = fread( buffer, 1, MAX_HTTP_SIZE, element->file );  /* read file chunk */
    if( len < 0 ) {                             /* check for errors */
         perror( "Error while writing to client" );
    } else if( len > 0 ) {                      /* if none, send chunk */
      len = write( element->clientfd, buffer, len );
      if( len < 1 ) {                           /* check for errors */
        perror( "Error while writing to client" );
      }
    }
  } while( len == MAX_HTTP_SIZE );              /* the last chunk < 8192 */
  fclose( element->file );
  close(element->clientfd);
  
  return 0;
}

/*
* Function To process the request in RR queue
*/

int procRR(RCB* element){
	static char *buffer;
	int len;
	buffer = malloc( MAX_HTTP_SIZE );
	int end=0;
	     

    len = fread( buffer, 1, MAX_HTTP_SIZE, element->file );  /* read file chunk */
    if( len < 0 ) {                             /* check for errors */
         perror( "Error while writing to client" );
    } else if( len > 0 ) {                      /* if none, send chunk */
      len = write( element->clientfd, buffer, len );
      element->remainingB=element->remainingB-len;
      if( len < 1 ) {                           /* check for errors */
        perror( "Error while writing to client" );
      }
    }
//if requested is finished 
  if(element->remainingB <= 0){
		end=1;
		element->notFin=1;
		fclose( element->file );
 		close(element->clientfd);
 		printf("REQUEST %d FINISHED(RR)\n",element->seq);
 		fflush(stdout);
 }
 		 
  
  return end;
}


//function to run RR
int runRR(){
	//Run in arrival order until it goes over the quantum

	int i=0;
	int k,end=0;
	int fin=0;
	//loop through the array of table until all process finishes 
	do{
	for (i=0;i<globalCounter;i++){
		//process request if it is not completed yet 
		if(rcbTable[i]->notFin==0)
			end+=procRR(rcbTable[i]);
		}
	}while(end<globalCounter);
	
	globalCounter=0;

	return 0;
}

RCB* proc8KB(RCB *element){
	static char *buffer;
	int len;
	buffer = malloc( MAX_HTTP_SIZE );
	int i=0;	     
	

    	len = fread( buffer, 1, MAX_HTTP_SIZE, element->file );  /* read file chunk */
    	if( len < 0 ) {                             /* check for errors */
    	     perror( "Error while writing to client" );
    	} else if( len > 0 ) {                      /* if none, send chunk */
    	  len = write( element->clientfd, buffer, len );
    	  
    	  element->remainingB=element->remainingB-len;
    	  if( len < 1 ) {                           /* check for errors */
    	    perror( "Error while writing to client" );
    	  }
    	 }
    	//if requested is finished 
 	 if(element->remainingB <= 0){
		element->notFin=1;
		fclose( element->file );
 		close(element->clientfd);
 		printf("REQUEST %d FINISHED IN 8KB QUEUE\n",element->seq);
 		fflush(stdout);
 		return element;
 		}
   	 printf("REQUEST %d WAS ONCE IN 8KB QUEUE Remaining Bytes: %d\n",element->seq,element->remainingB);
 fflush(stdout);

 return element;
}

RCB* proc64KB(RCB *element){
	static char *buffer;
	int len;
	buffer = malloc( MAX_HTTP_SIZE );
	int end=0;
	int i;
	     
	for(i=0;i<8;i++){
    len = fread( buffer, 1, MAX_HTTP_SIZE, element->file );  /* read file chunk */
    if( len < 0 ) {                             /* check for errors */
         perror( "Error while writing to client" );
    } else if( len > 0 ) {                      /* if none, send chunk */
      len = write( element->clientfd, buffer, len );
      
      element->remainingB=element->remainingB-len;
      if( len < 1 ) {                           /* check for errors */
        perror( "Error while writing to client" );
      }
    }
   
//if requested is finished 
  if(element->remainingB <= 0){
		end=1;
		element->notFin=1;
		fclose( element->file );
 		close(element->clientfd);
 		printf("REQUEST %d FINISHED IN 64KB QUEUE\n",element->seq);
 		fflush(stdout);
 		return element;
 }
 }
 printf("REQUEST %d WAS ONCE IN 64KB QUEUE Remaining Bytes: %d\n",element->seq,element->remainingB);
 fflush(stdout);
 return element;
}


int runMLFB(){
	//Run in arrival order until it goes over the quantum

	int i=0;
	int end=0;
	int elements64=0;
	int remElements=0;
	RCB *KB64[64];
	RCB *Remainder[64];
	RCB *element;
	//loop through the array of table until all process finishes 
	
	for (i=0;i<globalCounter;i++){
		element=proc8KB(rcbTable[i]);
		if(element->notFin==0){
			KB64[elements64]=element;
			elements64++;
		}
	}
	//for 64kb elements
	for (i=0;i<elements64;i++){
		element=proc64KB(KB64[i]);
		if(element->notFin==0){
			Remainder[remElements]=element;
			remElements++;
		}
	}
	//for the rest 
	do{
	for (i=0;i<remElements;i++){
		//process request if it is not completed yet 
		if(KB64[i]->notFin==0)
			end+=procRR(KB64[i]);
		}
	}while(end<remElements);
	
	
	globalCounter=0;

	return 0;
}
	

/* This function is where the program starts running.
 *    The function first parses its command line parameters to determine port #
 *    Then, it initializes, the network and enters the main loop.
 *    The main loop waits for a client (1 or more to connect, and then processes
 *    all clients by calling the seve_client() function for each one.
 * Parameters: 
 *             argc : number of command line parameters (including program name
 *             argv : array of pointers to command line parameters
 * Returns: an integer status code, 0 for success, something else for error.
 */
int main( int argc, char **argv ) {
  int port = -1;                                    /* server port # */
  int fd;                                           /* client file descriptor */
	
	globalCounter=0;
	
  /* check for and process parameters 
   */
   if( ( argc < 3 ) || ( sscanf( argv[1], "%d", &port ) < 1 ) ||
      ( strcmp(argv[2], "SJF") != 0 && strcmp(argv[2], "RR") != 0 &&
	strcmp(argv[2], "MLFB"))){
    printf( "usage: sms <port> <scheduler>\n" );
    return 0;
  }

  network_init( port );                             /* init network module */

  for( ;; ) {                                       /* main loop */
    network_wait();                                 /* wait for clients */

    for( fd = network_open(); fd >= 0; fd = network_open() ) { /* get clients */
      serve_client( fd );                           /* process each client */
     
    }
   if( strcmp(argv[2], "SJF") == 0)
   	runSJF();
   else if(strcmp(argv[2], "RR")==0)
    runRR();
   else
   	runMLFB();

  }
}
