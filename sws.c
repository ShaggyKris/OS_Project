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
#include <unistd.h>

#define MAX_HTTP_SIZE 8192                 /* size of buffer to allocate */

struct Queue *WorkQueue;
struct Queue *SJF;
struct Queue *RR;
struct Queue *First_P;
struct Queue *Second_P;

pthread_mutex_t signal, enqueue_m, process_m,first_m,second_m;
pthread_cond_t sig_no_work = PTHREAD_COND_INITIALIZER;
int sin=0;
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

void init(void){
	WorkQueue = (struct Queue*)malloc(sizeof(struct Queue));
	SJF = (struct Queue*)malloc(sizeof(struct Queue));
	RR = (struct Queue*)malloc(sizeof(struct Queue));
	First_P=(struct Queue*)malloc(sizeof(struct Queue));
	Second_P=(struct Queue*)malloc(sizeof(struct Queue));
	global_counter = 0;
	WorkQueue->name = "WorkQueue";
	SJF->name = "SJF";
	RR->name="RR";
	First_P->name="8KB queue";
	Second_P->name="64KB queue";
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
    close(fd);
  } else {                                          /* if so, open file */
    req++;                                          /* skip leading / */
    fin = fopen( req, "r" );                        /* open file */
    if( !fin ) {                                    /* check if successful */
      len = sprintf( buffer, "HTTP/1.1 404 File not found\n\n" );  
      write( fd, buffer, len );                     /* if not, send err */
      close(fd);
    } else {                                        /* if so, send file */
      len = sprintf( buffer, "HTTP/1.1 200 OK\n\n" );/* send success code */
      write( fd, buffer, len );

      //Somewhere in here, add in RCB allocation and enqueueing
      fseek(fin, 0L, SEEK_END);
      int size = ftell(fin);
      rewind(fin);      
      
      rcb->remainingBytes = size;
      rcb->clientfd = fd;
      rcb->quantum = 4028;
      rcb->file = fin;
      rcb->sequence = ++global_counter;
      //for writing file path
      int MAXSIZE = 0xFFF;
   	 char proclnk[0xFFF];
   	 char filename[0xFFF];
	 ssize_t leng;
	 int fno;
	 fno=fileno(rcb->file);
	 sprintf(proclnk,"/proc/self/fd/%d",fno);
     leng = readlink(proclnk, filename, MAXSIZE);
     filename[leng]='\0';
      printf("Request for file %s admitted\n",filename);
      fflush(stdout);
      enqueue(WorkQueue,rcb);
      //pthread_cond_broadcast(&sig_no_work);
      
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
	if(rcb==NULL){
		return;
	}
	
	rcb->next = NULL;
	if(q->head == NULL)
	{
		q->head = rcb;
		q->tail = rcb;
		q->size++;
		//printf("\nEnqueueing in %s Sequence: %d\tClientFD: %d\tRemaining Bytes: %d\n",q->name,q->tail->sequence, q->tail->clientfd, q->tail->remainingBytes);
	}
	else{
		q->tail->next = rcb;
		q->tail = rcb;
		q->size++;
		//printf("\nEnqueueing to tail in %s Sequence: %d\tClientFD: %d\tRemaining Bytes: %d\n",q->name,q->tail->sequence, q->tail->clientfd, q->tail->remainingBytes);
	}
	fflush(stdout);	
    //printQueue(q);
	return;
}

struct RCB* dequeue(struct Queue *q){
	struct RCB *rcb = q->head;
	
	if(q->head == NULL){
		//printf("\nERROR YOU SUCK\n");
		return NULL;
	}
	
	if(q->head == q->tail){ 
		q->head = NULL;
		q->tail = NULL;
	}
	
	else
		q->head = q->head->next;
	//printf("\nDequeueing from %s Sequence: %d\tClientFD: %d\tRemaining Bytes: %d\n",q->name,rcb->sequence, rcb->clientfd, rcb->remainingBytes);
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
	
	while(temp != NULL && temp->next!=NULL && temp->remainingBytes != shortest){
		if(temp->next->remainingBytes == shortest){
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
	//printf("\nDequeueing from %s Sequence: %d\tClientFD: %d\tRemaining Bytes: %d\n",q->name,node->sequence, node->clientfd, node->remainingBytes);
	return node;
}
//function to enqueue request in increasing order 
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
	
	enqueue(SJF, dequeue_at(WorkQueue, shortest) );	
}
//processing shortest job first 
void processSJF(struct RCB* rcb){
	if(rcb==NULL)
		return;
	//for writing file name 
	static char *buffer; 
	int MAXSIZE = 0xFFF;
    char proclnk[0xFFF];
    char filename[0xFFF];
	ssize_t leng;
	int fno;
	fno=fileno(rcb->file);
	sprintf(proclnk,"/proc/self/fd/%d",fno);
    leng = readlink(proclnk, filename, MAXSIZE);
    filename[leng]='\0';
	
	buffer = malloc(MAX_HTTP_SIZE);
	int len;
	
	
	do {                                          /* loop, read & send file */
		len = fread( buffer, 1, MAX_HTTP_SIZE, rcb->file );  /* read file chunk */
		
		if( len < 0 ) {                             /* check for errors */
			perror( "Error while writing to client" );
		} 
		else if( len > 0 ){                      /* if none, send chunk */
			len = write( rcb->clientfd, buffer, len );
			printf("Sent %d bytes of file %s\n",len,filename);
			fflush(stdout);
			if( len < 1 ) {                           /* check for errors */
				perror( "Error while writing to client" );
			}
		}	
	} while( len == MAX_HTTP_SIZE );              /* the last chunk < 8192 */
	//for showing file name 
	
	printf("Request for file %s completed\n",filename);
	fflush(stdout);
	fclose(rcb->file);
	close(rcb->clientfd);
	free(buffer);
	return;	
}
//Enqueues everything from the WorkQueue into the 
void enqueueRR(void){
	while(WorkQueue->size!=0){
		enqueue(RR,dequeue(WorkQueue));
	}
}
//processing round robint 
void processRR(struct RCB *rcb){
	
	if(rcb==NULL)
		return;
	//for writing file name 
	int MAXSIZE = 0xFFF;
    char proclnk[0xFFF];
    char filename[0xFFF];
	ssize_t leng;
	int fno;
	fno=fileno(rcb->file);
	sprintf(proclnk,"/proc/self/fd/%d",fno);
    leng = readlink(proclnk, filename, MAXSIZE);
    filename[leng]='\0';
    
	static char *buffer; 
	
	buffer = malloc(MAX_HTTP_SIZE);
	int len;
	
	len = fread( buffer, 1, MAX_HTTP_SIZE, rcb->file );  /* read file chunk */
		
	if( len < 0 ) {                             /* check for errors */
		perror( "Error while writing to client" );
	} 
	else if( len > 0 ){                      /* if none, send chunk */
		len = write( rcb->clientfd, buffer, len );
		rcb->remainingBytes=rcb->remainingBytes-len;
		printf("Sent %d bytes of file %s\n",len,filename);
		fflush(stdout);
		if( len < 1 ) {                           /* check for errors */
			perror( "Error while writing to client" );
		}
	}	
	
	if(rcb->remainingBytes<=0){
		printf("Request for file %s completed\n",filename);
		fflush(stdout);
		fclose(rcb->file);
		close(rcb->clientfd);
		free(buffer);
	}else
		enqueue(RR,rcb);
	
	return;
}
/*
*This is function for threads to execute if RR is selected
*/		
void *thread_RR(void *name){


		pthread_mutex_lock(&signal);
		
		while(1){
			struct RCB *rcb;
			//if there is stuff in work queue
			if((pthread_mutex_trylock(&enqueue_m))==0){
				if(WorkQueue->head!=NULL)
					enqueueRR();
				   pthread_mutex_unlock(&enqueue_m);
			}

			//process RR
			while(RR->head!=NULL){
				rcb=dequeue(RR);
		
				if(rcb!=NULL){
					pthread_mutex_lock(&process_m);
					processRR(rcb);
					pthread_mutex_unlock(&process_m);
				}
			}
	}
	pthread_exit(0);
	
	
}
//to enqueu to 8KB queue 
void enqueue_8KB(void){
	while(WorkQueue->size!=0){
		enqueue(First_P,dequeue(WorkQueue));
	}
}
//for processing 8KB queue 
void process_8KB(struct RCB *rcb){
	if(rcb==NULL)
		return;
	//for writing file path
	int MAXSIZE = 0xFFF;
    char proclnk[0xFFF];
    char filename[0xFFF];
	ssize_t leng;
	int fno;
	fno=fileno(rcb->file);
	sprintf(proclnk,"/proc/self/fd/%d",fno);
    leng = readlink(proclnk, filename, MAXSIZE);
    filename[leng]='\0';
    
	
	static char *buffer; 
	
	buffer = malloc(MAX_HTTP_SIZE);
	int len;
	
	len = fread( buffer, 1, MAX_HTTP_SIZE, rcb->file );  /* read file chunk */
		
	if( len < 0 ) {                             /* check for errors */
		perror( "Error while writing to client" );
	} 
	else if( len > 0 ){                      /* if none, send chunk */
		len = write( rcb->clientfd, buffer, len );
		printf("Sent %d bytes of file %s\n",len,filename);
		fflush(stdout);
		rcb->remainingBytes=rcb->remainingBytes-len;
		
		if( len < 1 ) {                           /* check for errors */
			perror( "Error while writing to client" );
		}
	}	
	
	if(rcb->remainingBytes<=0){
		printf("Request for file %s completed\n",filename);
		fflush(stdout);
		fclose(rcb->file);
		close(rcb->clientfd);
		free(buffer);
	}else
		enqueue(Second_P,rcb);
	
	return;
	
}
//for processign 64KB queue
void process_64KB(struct RCB *rcb){
	if(rcb==NULL)
		return;
		

	//for writing file name
	int MAXSIZE = 0xFFF;
    char proclnk[0xFFF];
    char filename[0xFFF];
	ssize_t leng;
	int fno;
	fno=fileno(rcb->file);
	sprintf(proclnk,"/proc/self/fd/%d",fno);
    leng = readlink(proclnk, filename, MAXSIZE);
    filename[leng]='\0';
	static char *buffer; 
	
	buffer = malloc(MAX_HTTP_SIZE);
	int len;
	int i;
	

	//repeate procedure to send 8KB file 8 time. 
	for(i=0;i<8;i++){
   	 len = fread( buffer, 1, MAX_HTTP_SIZE, rcb->file );  /* read file chunk */
    	if( len < 0 ) {                             /* check for errors */
         perror( "Error while writing to client" );
    } else if( len > 0 ) {                      /* if none, send chunk */
 	     len = write( rcb->clientfd, buffer, len );
      	printf("Sent %d bytes of file %s\n",len,filename);
		fflush(stdout);
      rcb->remainingBytes=rcb->remainingBytes-len;
      if( len < 1 ) {                           /* check for errors */
        perror( "Error while writing to client" );
      }
    }
    }
	
	if(rcb->remainingBytes<=0){
		printf("Request for file %s completed\n",filename);
		fflush(stdout);
		fclose(rcb->file);
		close(rcb->clientfd);
		free(buffer);
	}else
		enqueue(RR,rcb);
	
	return;
	
}
/*
*This is function for threads to execute if MLFB is selected
*/		
void *thread_MLFB(void *name){
	//lock himself until main wake
	//waiting for main to wake him up 
	pthread_mutex_lock(&signal);
	//infinite loop processing MLFB 
	while(1){
		//as long as there is stuff in WorkQueue, enqueue to the first queue
		if((pthread_mutex_trylock(&enqueue_m))==0){
			while(WorkQueue->head!=NULL){
				enqueue_8KB();
				pthread_mutex_unlock(&enqueue_m);
			}
		}
		pthread_mutex_unlock(&enqueue_m);
			//processign first queue 
			if(First_P->head!=NULL){
				pthread_mutex_lock(&process_m);
				struct RCB *rcb=dequeue(First_P);
				//process first 8KB
				if(rcb!=NULL){
					process_8KB(rcb);
				}
				pthread_mutex_unlock(&process_m);
			}
			//processing second queue 
			else if(Second_P->head!=NULL){
				pthread_mutex_lock(&first_m);
				struct RCB *rcb=dequeue(Second_P);
				if(rcb!=NULL){
					process_64KB(rcb);
				}
				pthread_mutex_unlock(&first_m);
			}
			//it is the Round Robin queue 
			else if(RR->head!=NULL){
				pthread_mutex_lock(&second_m);
				struct RCB *rcb=dequeue(RR);
				if(rcb!=NULL){
					processRR(rcb);
				}
				pthread_mutex_unlock(&second_m);
			}
			
	}
pthread_exit(0);
}
		
/*
*This is function for threads to execute if SJF is selected
*/		

void *thread_SJF(void *name){
	struct RCB *rcb;
	pthread_mutex_lock(&signal);
	
	while(1){
		
		if(WorkQueue->head != NULL)
			pthread_mutex_unlock(&signal);

		if(pthread_mutex_trylock(&enqueue_m)==0){
			if(WorkQueue->head != NULL){
				enqueueSJF();
				pthread_mutex_unlock(&enqueue_m);					
						
			}	
		}
		pthread_mutex_unlock(&enqueue_m);					
		//printQueue(SJF);
		if(SJF->head != NULL){
			pthread_mutex_lock(&process_m);
			rcb=dequeue(SJF);
			if(rcb!=NULL)
				processSJF(rcb);	
			pthread_mutex_unlock(&process_m);	
		}
		

		if(SJF->head == NULL && WorkQueue->head == NULL){
			//printf("\nThead %d waiting for work.\n",name);
			pthread_mutex_lock(&signal);			
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
 if( ( argc < 4 ) || ( sscanf( argv[1], "%d", &port ) < 1 ) ||
      ( strcmp(argv[2], "SJF") != 0 && strcmp(argv[2], "RR") != 0 &&
	strcmp(argv[2], "MLFB"))){
    printf( "usage: sms <port> <scheduler> <number of threads>\n" );
    return 0;
  }
  
  //converting number of threads into number 
  int num_of_thread=atoi(argv[3]);

 	network_init( port );                             /* init network module */
	pthread_mutex_init(&enqueue_m, NULL);
	pthread_mutex_init(&process_m, NULL);
	pthread_mutex_init(&signal, NULL);
	pthread_mutex_init(&first_m, NULL);
	pthread_mutex_init(&second_m, NULL);
	pthread_t threads[num_of_thread];
	//creating threads 
	for(int i = 0; i < num_of_thread; i++){
		if(strcmp(argv[2],"SJF")==0){
			if(i==0)
				pthread_mutex_lock(&signal);
				
			if(pthread_create(&threads[i],NULL,thread_SJF,(void*)i) != 0){
				printf("\nError creating thread %d\n", i);
				return 1;
			}
		}
		else if(strcmp(argv[2],"RR")==0){
			if(i==0)
				pthread_mutex_lock(&signal);
			if(pthread_create(&threads[i],NULL,thread_RR,(void*)i) != 0){
			printf("\nError creating thread %d\n", i);
			return 1;
			}
		}
		else{
			if(i==0)
				pthread_mutex_lock(&signal);
			if(pthread_create(&threads[i],NULL,thread_MLFB,(void*)i) != 0){
			printf("\nError creating thread %d\n", i);
			return 1;
			}
		}
	}
	
	for( ;; ) {                                       /* main loop */
		
		network_wait();                                /* wait for clients */
		
		for( fd = network_open(); fd >= 0; fd = network_open() ) { /* get clients */
		  serve_client( fd ); 
		  pthread_mutex_unlock(&signal);

		}

		
/*    switch(argv[2]){*/
/*    	case "SJF":*/
/*    		*/
/*			break;*/
/*		default:*/
/*    }*/
	}
	for(int i = 0; i < num_of_thread; i++){
		pthread_join(threads[i],NULL);
	}
  	
}
