//user
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/sem.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

//status
enum status {IN_QUEUE, IN_PROGRESS, COMPLETE};

typedef struct pcb_node{
	int pid;		
	int priority; 		
	int spriority; 	
	double submit_time; 	
	double start_time;  	
	double end_time; 	
	double time_left;	
	double burst_time;
	double time_spent;
	int cycles;
	int status; 		
	int quantum;
} PCB;

// time struct 
typedef struct {
	unsigned int second;
	unsigned int nanoSecond;
	double total;
} ClockTime;

// shmem ids 
int shmem_id;
int shmem2_id;
int shmem3_id;

int who;
int myId;

// PCB pointer
PCB *pcb;
ClockTime *current_time;
int *number_waiting;

int i; //counter

FILE *logname = NULL;
char *filename = "log.txt";
// clock variables
double *second;
double *nanoSecond;
double *rand_ns;
double *total;

// function prototypes
void setup_mem();
void ctrlCHandler(int sig);
void increment_clock();


int main(int argc, char **argv){
	int quantum_use;
	if(argc < 2){
		printf("\n-----------------------------------------------------------------------\n");
                fprintf(stderr, "* %s is intended to be called by OSS with arguments passed from OSS. *\n", argv[0]);
                printf("\n-----------------------------------------------------------------------\n");
        exit(1);
        }
	// CTRL+C signal handler
	signal(SIGINT, ctrlCHandler);
	// seed random
	srand(time(NULL));

	// user process number
	who = (atoi(argv[1]) + 1);
	// user process PID
	myId = atoi(argv[2]);
	// shared memory ids
	shmem_id = atoi(argv[3]);	// PCBs
	shmem2_id = atoi(argv[4]);	// number waiting
	shmem3_id = atoi(argv[5]);	// clock
	
	setup_mem();
	
wait:	while(pcb[who].status != IN_PROGRESS){
	}
	
	while(pcb[who].status == IN_PROGRESS){
		
		int skip = 0;
		
		quantum_use = (rand() % 2);
		if(skip == 0 && quantum_use == 1 && ((pcb[who].quantum * .001) < pcb[who].time_left)){
			fprintf(stderr, "Job %i running full quantum %11.9f < time requirement with PID %i.\n", who, (pcb[who].quantum * .001), myId);
			
			if(pcb[who].start_time == 0)
				pcb[who].start_time = current_time->total;
			pcb[who].burst_time = (pcb[who].quantum * .001);
			pcb[who].time_left -= (pcb[who].quantum * .001);
			pcb[who].time_spent += (pcb[who].quantum * .001);
			pcb[who].status = IN_QUEUE;
			pcb[who].cycles++;
			skip = 1;
			goto wait;
		}
		
		if(skip == 0 && quantum_use == 0 && ((pcb[who].quantum * .001) < pcb[who].time_left)){

			int partial = (rand() % pcb[who].quantum);
			fprintf(stderr, "Job %i running partial quantum %11.9f < time requirement with PID %i.\n", who, (pcb[who].quantum * .001), myId);
			if(pcb[who].start_time == 0)
				pcb[who].start_time = current_time->total;
			pcb[who].burst_time = (partial * .001);
			pcb[who].time_left -= (partial * .001);
			pcb[who].time_spent += (partial * .001);
			pcb[who].status = IN_QUEUE;
			pcb[who].cycles++;
			skip = 1;
			goto wait;
		}
		if(skip == 0 && quantum_use == 1 && ((pcb[who].quantum * .001) >= pcb[who].time_left)){
			fprintf(stderr, "Job %i running full quantum %11.9f >= time requirement with PID %i.\n", who, (pcb[who].quantum * .001), myId);
			if(pcb[who].start_time == 0)
				pcb[who].start_time = current_time->total;
			pcb[who].burst_time = pcb[who].time_left;
			pcb[who].time_spent += pcb[who].time_left;
			pcb[who].time_left = 0;
			pcb[who].status = COMPLETE;
			pcb[who].end_time = current_time->total;
			*number_waiting = (*number_waiting - 1);
			skip = 1;
		}
		
		if(skip == 0 && quantum_use == 0 && ((pcb[who].quantum * .001) >= pcb[who].time_left)){
			// calculate and store partial quantum amount
			int partial = (rand() % pcb[who].quantum);
			fprintf(stderr, "Job %i running partial quantum %11.9f >= time requirement with PID %i.\n", who, (pcb[who].quantum * .001), myId);
			// if partial amount end up less than time left
			if((partial * .001) < pcb[who].time_left){	
				// start the clock on process if it hasn't run before
				if(pcb[who].start_time == 0)
					pcb[who].start_time = current_time->total;
				// set burst time equal to partial amount
				pcb[who].burst_time = (partial * .001);
				// subtract partial amount from time left
				pcb[who].time_left -= (partial * .001);
				// add partial amount to total time spent
				pcb[who].time_spent += (partial * .001);
				// place back in line
				pcb[who].status = IN_QUEUE;
				// increment number of processor cycles
				pcb[who].cycles++;
				// indicate run has taken place
				skip = 1;
				// go back to waiting condition
				goto wait;
			}
			else{ 
				if(pcb[who].start_time == 0)
					pcb[who].start_time = current_time->total;
				pcb[who].burst_time = pcb[who].time_left;
				pcb[who].time_spent += pcb[who].time_left;
				pcb[who].time_left = 0;
				pcb[who].status = COMPLETE;
				pcb[who].end_time = current_time->total;
				*number_waiting = (*number_waiting - 1);
				skip = 1;
			}
		}
		// reset skip
		skip = 0;

		increment_clock();
	}
return 0;
}//main

void setup_mem(){
	// attach to PCB
	if((pcb = (PCB *)shmat(shmem_id, NULL, 0)) == (void *) -1){
		perror("Failed to attach to PCB shared memory segment.\n");
	exit(1);
	}
	else
		printf("User process %i with PID %i now attached to PCB shared memory segment.\n", who, myId);
	

	// attach to number waiting 
	if((number_waiting = (int *)shmat(shmem2_id, NULL, 0)) == (void *) -1){
		perror("Failed to attach to number_waiting shared memory segment.\n");
	exit(1);
	}
	else
		printf("User process %i with PID %i now attached to number_waiting shared memory segment.\n", who, myId);
	
	// attach to clock
	if((current_time = (ClockTime *)shmat(shmem3_id, NULL, 0)) == (void *) -1){
		perror("Failed to attach to monitor shared memory segment.\n");
	exit(1);
	}
	else
		printf("User process %i with PID %i now attached to clock shared memory segment.\n", who, myId);
}		

// Ctrl+C handler
void ctrlCHandler(int sig){
	// sleep added to clean up message display order
	sleep(1);
	fprintf(stderr, "\nSIGINT detected in the child process. Process %i is dying.\n", getpid());
exit(0);
}

void increment_clock(){
	// increment random 0-1000 nanoSecond
	*rand_ns = rand() % 1001;
	*nanoSecond += *rand_ns;
	// increment seconds when nanoSecond roll over
	if(*nanoSecond >= 1000000000){
		*second += 1;
		*nanoSecond = *nanoSecond - 1000000000;
	}
	// set values in time structure
	current_time->second = *second;
	current_time->nanoSecond = *nanoSecond;
	// implementation of total time which may prove easier to use
	*total = *second + (*nanoSecond * .000000001);
	current_time->total = *total;
}

