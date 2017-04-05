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
#include <pthread.h>
#include "network.h"

#define MAX_HTTP_SIZE 8192                 /* size of buffer to allocate */

typedef struct{
  int seq;
  int clientfd;
  FILE* file;
  int remainingB;
  int quantum;
  int notFin;//forRR
  int working;
}RCB;

void *work(void*);

//RCB table 
RCB *rcbTable[64];
int globalCounter;
int sjfEnd;
pthread_mutex_t thread_lock=PTHREAD_MUTEX_INITIALIZER;

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
	  /*-----THREAD SAFE OPERATION---------*/
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
      /*-------END-----------------------*/
    }
  }
  
  
  
}
/*This function is to sort the RCB table.
  At this point, insertion sort is used to sort the job from shortest
  to longest.
*/

int globalI=0;
int isSorted=0;

int runSJF(){
	//sort the array of RCB 
	RCB *shortest;
	int i,j=0;
	//if not sorted by previous threads
	if(isSorted==0){
	//insertion sort 
	    for (i=1;i<globalCounter;i++){
	    	shortest=rcbTable[i];
	    	for(j=i;j>0 && rcbTable[j-1]->remainingB > shortest ->remainingB; j--){
	    		rcbTable[j]=rcbTable[j-1];
	    	}
	    	rcbTable[j]=shortest;
	    }
	}
	isSorted=1;
	
    i=0;
	//process request
	pthread_mutex_lock(&thread_lock);
	while(globalI<globalCounter){

	    if(rcbTable[globalI]->notFin==1){
	    	globalI++;
	       if(globalI>=globalCounter)
	    	 break;
	    	 
	        continue;
	    }

		//for debug 
		printf("Job seq %d\nLength %d\n",rcbTable[globalI]->seq,rcbTable[globalI]->remainingB);
		fflush(stdout);
		//call process routine for each rcb stored in table 

		if(rcbTable[globalI]->notFin==0){
			if(globalI>=globalCounter)
	    	 break;
		    procSJF(rcbTable[globalI]);
		    globalI++;
		}


		
	}
	pthread_mutex_unlock(&thread_lock);

	//initializes the counter for next use 
	if(globalI>=globalCounter)
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
	
	element->working=1;


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
  element->notFin=1;
  element->working=0;
  free(buffer);
  return 1;
}

/*
* Function To process the request in RR queue
*/

int procRR(RCB* element){
	static char *buffer;
	int len;
	buffer = malloc( MAX_HTTP_SIZE );
	int end=0;
	if(element==NULL)
		return 0;
	if(element->notFin==1)
		return 1;

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
 		return end;
 }
 printf("REQUEST %d SENT BACK TO QUEUE(RR) Rem %d\n",element->seq,element->remainingB);
 		fflush(stdout);
  free(buffer);
  return 0;
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
		if(rcbTable[i]->notFin==0){
			//critical section
			pthread_mutex_lock(&thread_lock);
			globalI+=procRR(rcbTable[i]);
			//end of critical section
			pthread_mutex_unlock(&thread_lock);
		}

		}

	}while(globalI<globalCounter);
	
	globalCounter=0;

	return 0;
}

RCB* proc8KB(RCB *element){
	static char *buffer;
	int len;
	buffer = malloc( MAX_HTTP_SIZE );
	int i=0;	     
	
	if(element==NULL)
		return NULL;

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
	free(buffer);
 return element;
}

RCB* proc64KB(RCB *element){
	static char *buffer;
	int len;
	buffer = malloc( MAX_HTTP_SIZE );
	int end=0;
	int i;
	
	if(element==NULL)
		return NULL;
		
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
 	free(buffer);
 return element;
}

int end=0;
int elements64=0;
int remElements=0;
RCB *KB64[64];
RCB *Remainder[64];
int secCount=0;

int runMLFB(){
	//Run in arrival order until it goes over the quantum

	int i=0;
	RCB *element;
	//loop through the array of table until all process finishes 
	
	while(globalI<globalCounter){
		pthread_mutex_lock(&thread_lock);
		element=proc8KB(rcbTable[globalI]);
		if(element==NULL)
			continue;
		if(element->notFin==0){
			KB64[elements64]=element;
			elements64++;
		}
		globalI++;
		pthread_mutex_unlock(&thread_lock);
	}
	
	
	//for 64kb elements
	while(secCount<elements64){
		pthread_mutex_lock(&thread_lock);
		element=proc64KB(KB64[secCount]);
		if(element==NULL)
			continue;
		if(element->notFin==0){
			Remainder[remElements]=element;
			remElements++;
		}
		secCount++;
		pthread_mutex_unlock(&thread_lock);
		
	}

	//for the rest 
	do{
	  for (i=0;i<remElements;i++){
		//process request if it is not completed yet 
		if(Remainder[i]->notFin==0){
			pthread_mutex_lock(&thread_lock);
			end+=procRR(Remainder[i]);
			pthread_mutex_unlock(&thread_lock);
		}
	  }
	}while(end<remElements);
	
	


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
   if( ( argc < 4 ) || ( sscanf( argv[1], "%d", &port ) < 1 ) ||
      ( strcmp(argv[2], "SJF") != 0 && strcmp(argv[2], "RR") != 0 &&
	strcmp(argv[2], "MLFB"))){
    printf( "usage: sms <port> <scheduler> <number of threads>\n" );
    return 0;
  }
  pthread_t worker[4];
  network_init( port );                             /* init network module */
 
	
  for( ;; ) {                                       /* main loop */
    network_wait();                                 /* wait for clients */

    for( fd = network_open(); fd >= 0; fd = network_open() ) { /* get clients */
      serve_client( fd );                           /* process each client */
     
    }
      for(int i=0;i<4;i++){
		pthread_create(&worker[i],NULL,work,(void*)argv[2]);
	}
    
   /*if( strcmp(argv[2], "SJF") == 0)
   	runSJF();
   else if(strcmp(argv[2], "RR")==0)
    runRR();
   else
   	runMLFB();*/
   	for(int i=0;i<4;i++){
		pthread_join(worker[i],NULL);
	}
	globalCounter=0;
	globalI=0;
	isSorted=0;

  }
 }
void *work(void *scheduler){
    if(strcmp(scheduler,"SJF")== 0)
        runSJF();
    else if(strcmp(scheduler, "RR")==0)
    	runRR();
   else
   		runMLFB();
}
