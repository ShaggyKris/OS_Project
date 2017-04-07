#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>


struct RCB{
	int sequence;
	int clientfd;
	FILE* file;
	int remainingBytes;
	int quantum;
	struct RCB* next;
};

struct Queue{
	struct RCB *head;
	struct RCB *tail;
	int size;
	char* name;
};

void init(void);
static void serve_client(int fd);
void enqueue(struct Queue *q, struct RCB *rcb);
struct RCB* dequeue(struct Queue *q);
struct RCB* dequeue_at(struct Queue *q, int position);
void printQueue(struct Queue *q);
void enqueueSJF(void);
void enqueueRR(void);
void *thread_SJF();
void *thread_RR();
void *thread_MLFB();
void processSJF(struct RCB *rcb);

void printQueue(struct Queue *q){
	if(q->head == NULL){
		printf("\nThere are no items in this list!\n");
		fflush(stdout);
	}
	struct RCB* rcb = q->head;
	while(rcb!=NULL){
		printf("\n\nQueue %s\tSequence: %d\tClientFD: %d\tRemaining Bytes: %d\n\n",q->name,rcb->sequence, rcb->clientfd, rcb->remainingBytes);
		fflush(stdout);
		rcb = rcb->next;
	}
}
