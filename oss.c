//OSS
#include <ctype.h>
#include <float.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define SetBit(A,k)     (A[(k/32)] |= (1 << (k%32)))
#define ClearBit(A,k)   (A[(k/32)] &= ~(1 << (k%32)))
#define TestBit(A,k)    (A[(k/32)] & (1 << (k%32)))

// status
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
int segment_id;
int segment2_id;
int segment3_id;

int s = 10; //global s value and default s value is 10

// PID holder
pid_t oss_pid;

// PID holder for process
pid_t up_pid;
// PCB pointers
PCB *pcb;
// time
ClockTime *current_time;
// pointer for output file and default filename
FILE *logname = NULL;
char *fname = "log.txt";
// job priority
int priority;
// context switch tracking (0 - No, 1 - Yes)
int *context = 0;
// clock variables
double *second;
double *nanoSecond;
double *rand_ns;
double *total;
double *time_in;
// scheduler variables
int *number_waiting;
int *current_job;
int *turn_to;
// Bit Vector
int A[1];

// loop counters
int i, j;

// function prototypes
void ctrlCHandler(int signal);
void cleanup();
void setup_mem();
void setup_bit_array();
void increment_clock();
void create_queue();
void switch_job(int new_job_nr);
void set_clock(double amount);
void schedule();
int findShortestQ1();
int findShortestQ2();
int findShortestQ3();
void print_stats();

int main(int argc, char* argv[]) {
	// CTRL+C signal handlers
	signal(SIGINT, ctrlCHandler);
	// init all memory
	setup_mem();
	*current_job = INT_MAX;
	// get PID for OSS 
	oss_pid = getpid();
	// random 
	srand(time(NULL));	
	
	int input;// s;
	
	char *fileRename = NULL;

	while ((input = getopt(argc, argv, "hs:l:")) != -1){
		switch(input){
			
			case 'h':
				printf("Usage:\n");
				printf("./oss  runs the program with default settings.\n");
				printf("./oss -l fileName changes fileName\n");
				printf("./oss -s x	runs the amount of processes default is 19\n");
				printf("./oss -h  displays help info\n");
			exit(0);
			break;
			
			//user processes 
			case 's':
				 s = atoi(optarg);
				
				if(s > 19 || s < 1){
					fprintf(stderr,"Invalid number of user processes (%s) entered. Minimum is 1. Maximum is 19. Integers only.\n", s);
					exit(1);
				}
				
				if(s == 1)
					printf("oss will spawn %i user process.\n", s);
				else if(s != 19)
					printf("oss will spawn %i user processes.\n", s);
			break;
			// filename config switch
			case 'l':
				//fileRename = optarg;
				printf("\nRename File\n");
				
			break;		
			// handle missing required arguments and unknown arguments
			case'?':
				if(optopt == 's' || optopt == 'f'){
					fprintf(stderr,"Switch -%c requires an additional argument.\n", optopt);
				}
				else if(isprint(optopt)){
					fprintf(stderr,"Invalid option -%c.\n", optopt);
					fprintf(stderr,"Use ./oss -h to display usage information.\n");
				}
			exit(1);
		}
	}
	
	//-----------------------
	
	
	if(strcmp(fname, "log.txt") == 0)
	{
		printf("Output will be written to log.txt.\n");
	}
	printf("\n-----------------------------------------------------------------------\n");
	
	printf("System time is %11.9f.\n", current_time->total);
	setup_bit_array();
	create_queue();
	//fork children
	for(i = 0; i < s; i++){
		up_pid = fork();

		//fork error
		if(up_pid == -1){
			fprintf(stderr, "Forking user process %i failed.\n", i);
		exit(1);
		}
		if(up_pid == 0){
			int myId = getpid();
			int currChild = i;
			char arg1[3];
			sprintf(arg1, "%i", currChild);
			char arg2[6];
			sprintf(arg2, "%i", myId);
			
			// shared memory ids
			char arg3[20];
			sprintf(arg3, "%i", segment_id);	// PCBs
			char arg4[20];
			sprintf(arg4, "%i", segment2_id);	// number_waiting 
			char arg5[20];
			sprintf(arg5, "%i", segment3_id);	// clock

			printf("User process: %i has PID: %s.\n", (atoi(arg1) + 1),  arg2);
			// execute user process with segment_id, child number and child_pid arguments
			execl("./user", "user", arg1, arg2, arg3, arg4, arg5, NULL);
		return 0;
		}
	usleep(95000);
	}
	schedule();
	for(j = 0; j < s; j++){
		wait(NULL);
	}

	printf("\ntotal time spent in system %11.9f.\n", current_time->total);
	print_stats();
	cleanup();

return 0;
} //main

// Ctrl+C handler
void ctrlCHandler(int signal){
	if(getpid() == oss_pid){
		fprintf(stderr, "\nCtrl-C in OSS\n");
		fprintf(stderr, "Cleaning up....\n");
		// call cleanup to terminate and clean up
		cleanup();
	}
exit(signal);
}

// Print to log.out 
void print_stats(){
	int k;
	double proc_time;
	logname = fopen(fname, "a");
	
	setbuf(logname, NULL);
	fprintf(logname, "User Process Stats\n");
	fprintf(logname,"\n-----------------------------------------------------------------------\n");
	for(k = 1; k <= s; k++){
	fprintf(logname, "User Process %i\n", k);
	fprintf(logname,"\n-----------------------------------------------------------------------\n");
		fprintf(logname, "information for process %i:\n", k); 
		fprintf(logname, "Process started with a priority of %i\n", pcb[k].spriority);
		fprintf(logname, "Process finished with a priority of %i\n", pcb[k].priority);
		fprintf(logname, "submission time: %11.9f seconds\n", pcb[k].submit_time);
		fprintf(logname, "Ending time: %11.9f seconds\n", pcb[k].end_time);
		fprintf(logname, "--------------------------------------------------\n");
		fprintf(logname, "Cumulative processing time (initial time requirement): %11.9f seconds\n", pcb[k].time_spent);
		fprintf(logname, "Cumulative wait time: %11.9f seconds\n", ((pcb[k].end_time - pcb[k].submit_time) - pcb[k].time_spent));
		fprintf(logname, "Total time in system for process %i: %11.9f seconds\n", k, (pcb[k].end_time - pcb[k].submit_time));
		fprintf(logname, "Process acquired the processor %i times.\n", (pcb[k].cycles + 1));
		fprintf(logname,"\n-----------------------------------------------------------------------\n");
		proc_time += pcb[k].time_spent;
	}
	fprintf(logname,"\n-----------------------------------------------------------------------\n");
	fprintf(logname, "Total accumulated running time: %11.9f seconds\n", current_time->total);	
	fprintf(logname, "Total processor time: %11.9f seconds\n", proc_time);
	fprintf(logname, "Time processor spent idle: %11.9f seconds\n\n", (current_time->total - proc_time));
	fprintf(logname,"\n-----------------------------------------------------------------------\n");
}

void setup_bit_array(){
	
	A[0] = 0;                   
} 

void setup_mem(){
	int i;
	second = malloc(sizeof(double));
	nanoSecond = malloc(sizeof(double));
	rand_ns = malloc(sizeof(double));
	total = malloc(sizeof(double));
	time_in = malloc(sizeof(double));
	current_job = malloc(sizeof(int));
	context = malloc(sizeof(int));
	turn_to = malloc(sizeof(int));
	number_waiting = malloc(sizeof(int));
	
	// alocate  memory for jobs	
	pcb = (PCB *)malloc(s * sizeof(PCB));
	// allocate memory for logical clock
	current_time = malloc(sizeof(ClockTime));
	
	// allocate shared memory for PCB
	if((segment_id = shmget(IPC_PRIVATE, (sizeof(PCB)*18), S_IRUSR | S_IWUSR)) ==  -1){
		fprintf(stderr, "Failed to get shared memory segment for PCBs.\n");
	exit(1);
	}
	else
		printf("\nShared memory segment for PCBs created with ID: %i.\n", segment_id);
	
	// attach to newly allocated shared memory and notify status
	if((pcb = (PCB *)shmat(segment_id, NULL, 0)) == (void *) -1){
		fprintf(stderr,"Failed to attach to shared memory segment %i for PCBs.\n", segment_id);
	exit(1);
	}
	else
	printf("Shared memory segment %d for PCBs attached at address %p.\n\n", segment_id, pcb);
	
	// allocate shared memory segment for number_waiting
	if((segment2_id = shmget(IPC_PRIVATE, (sizeof(int)), S_IRUSR | S_IWUSR)) ==  -1){
		fprintf(stderr, "Failed to get shared memory segment for number_waiting.\n");
	exit(1);
	}
	else
		printf("Shared memory segment for number_waiting created with ID: %i.\n", segment2_id);

	// attach to newly allocated shared memory and notify status
	if((number_waiting = (int *)shmat(segment2_id, NULL, 0)) == (void *) -1){
		fprintf(stderr,"Failed to attach to shared memory segment %i for number_waiting.\n", segment2_id);
	exit(1);
	}
	else
	printf("Shared memory segment %d for number_waiting attached at address %p.\n\n", segment2_id, number_waiting);
	
	// allocate shared memory segment for clock
	if((segment3_id = shmget(IPC_PRIVATE, (sizeof(ClockTime)), S_IRUSR | S_IWUSR)) ==  -1){
		fprintf(stderr, "Failed to get shared memory segment for logical clock.\n");
	exit(1);
	}
	else
		printf("Shared memory segment for logical clock created with ID: %i.\n", segment3_id);

	// attach to newly allocated shared memory and notify status
	if((current_time = (ClockTime *)shmat(segment3_id, NULL, 0)) == (void *) -1){
		fprintf(stderr,"Failed to attach to shared memory segment %i for logical clock.\n", segment3_id);
	exit(1);
	}
	else
	printf("Shared memory segment %d for logical clock attached at address %p.\n\n", segment3_id, current_time);
	
}

void cleanup(){
	// kill processes
	signal(SIGINT, SIG_IGN);
	// Get rid of the kids
	kill(-oss_pid, SIGINT);
	// sleep allows clean stdout/stderr display
	usleep(50000);

	// detach and release PCB shared memory
	printf("\nDetaching and releasing PCB shared memory segment %d and freeing up address %p.\n", segment_id, pcb);
	// detach/catch detach error
	if(shmdt(pcb) == -1){
		fprintf(stderr, "Unable to detach PCB shared memory %d @ %p.\n", segment_id, pcb);
	}
	else	// report success
		printf("PCB memory successfully detached.\n");
	// remove segment
	if((shmctl(segment_id, IPC_RMID, NULL)) == -1){
		fprintf(stderr,"Failed to release PCB shared memory segment.\n");
	}
	else	// report success
		printf("PCB shared memory segment successfully released.\n\n");

	// detach and release number_waiting shared memory
	printf("Detaching and releasing number_waiting shared memory segment %d and freeing up address %p.\n", segment2_id, number_waiting);
	// detach/catch detach error
	if(shmdt(number_waiting) == -1){
		fprintf(stderr, "Unable to detach number_waiting shared memory %d @ %p.\n", segment2_id, number_waiting);
	}
	else	// report success
		printf("number_waiting memory successfully detached.\n");
	// remove segment
	if((shmctl(segment2_id, IPC_RMID, NULL)) == -1){
		fprintf(stderr,"Failed to release number_waiting shared memory segment.\n");
	}
	else	// report success
		printf("number_waiting shared memory segment successfully released.\n\n");

	// detach and release ClockTime shared memory
	printf("Detaching and releasing logical clock shared memory segment %d and freeing up address %p.\n", segment3_id, current_time);
	// detach/catch detach error
	if(shmdt(current_time) == -1){
		fprintf(stderr, "Unable to detach logical clock shared memory %d @ %p.\n", segment3_id, current_time);
	}
	else	// report success
		printf("Logical clock memory successfully detached.\n");
	// remove segment
	if((shmctl(segment3_id, IPC_RMID, NULL)) == -1){
		fprintf(stderr,"Failed to release logical clock shared memory segment.\n");
	}
	else	// report success
		printf("Logical clock shared memory segment successfully released.\n\n");

exit(0);
}

//---


void increment_clock(){
	//increment random 0-1000 nanoSecond
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

//set clock desired amount ahead 
void set_clock(double amount){
	// increment nanoseconds
	if(amount + *nanoSecond < 1000000000){
		current_time->total += amount;
		*nanoSecond += amount;
	}
	else{
		current_time->total += amount;
		*second += 1;
		*nanoSecond = *nanoSecond - 1000000000;
	}
	
}	

//queue function
void create_queue(){
	int i;
	//fill que with jobs and priority of 1-3
	for(i = 1; i <= s; i++){
		*time_in = current_time->total;
		double time_needed = (((double) rand() / (double) RAND_MAX) * 150000) + 10000;
		double time_adv = (((double) rand() / (double) RAND_MAX) * 50000) + 50000;
		int pri = ((rand() % 3) +1); // priority 1 - 3 for queues
		
		pcb[i].pid = i; pcb[i].priority = pri; pcb[i].submit_time = *time_in; pcb[i].start_time = 0; 
		pcb[i].end_time = 0; pcb[i].time_left = (time_needed * (double).000001);pcb[i].spriority = pri;
		pcb[i].burst_time = 0; pcb[i].time_spent = 0; pcb[i].status = IN_QUEUE;
		printf("New job: %i started with priority: %i needing %11.9f seconds of CPU time.\n", pcb[i].pid, pcb[i].priority, pcb[i].time_left);
		*number_waiting = (*number_waiting + 1);
		
		set_clock((time_adv * .00001));
		
		SetBit(A, i);
		
		
		/* 
		pcb[i].pid = i; pcb[i].priority = priority; pcb[i].submit_time = *time_in; pcb[i].start_time = 0; 
		pcb[i].end_time = 0; pcb[i].time_needed = (time_left* (double).000001);pcb[i].spriority = priority;
		pcb[i].burst_time = 0; pcb[i].time_spent = 0; pcb[i].status = IN_QUEUE;
		*/
		
		
	}
	for(i = 1; i <= s; i++){
		if(TestBit(A, i))
			printf("Process %i stored in bit vector.\n", i);
	}
}

/* Context switch */
void switch_job(int new_job_nr){
	if(*context == 1 && (*current_job == new_job_nr)){ // switching current job for current job (nothing to do)
		printf("\nNo context switch. Job %i was picked to run again.\n", pcb[new_job_nr].pid);
		return;
	}
	else if(*context == 1){
		printf("\nContext switch taking place. Job %i is now running in place of job %i.\n", pcb[new_job_nr].pid, pcb[*current_job].pid);
		*current_job = new_job_nr;
		*context = 1;
	}
	else{
		*current_job = new_job_nr;
		*context = 1;
	}
}

//shortest job in queue 1 
int findShortestQ1(){
	int i;
	double shortest = DBL_MAX;
	int shortest_index = INT_MAX;
	for(i = 1; i <= s; i++){
		if(pcb[i].status == IN_QUEUE && pcb[i].time_left < shortest && pcb[i].time_left > 0 && pcb[i].priority == 1 && pcb[i].submit_time <= current_time->total){
			shortest = pcb[i].time_left;
			shortest_index = i;
		}
	}
return shortest_index;
}

//shortest job in queue 2 
int findShortestQ2(){
	int i;
	double shortest = DBL_MAX;
	int shortest_index = INT_MAX;
	for(i = 1; i <= s; i++){
		if(pcb[i].status == IN_QUEUE && pcb[i].time_left < shortest && pcb[i].time_left > 0 && pcb[i].priority == 2 && pcb[i].submit_time <= current_time->total){
			shortest = pcb[i].time_left;
			shortest_index = i;
		}
	}
return shortest_index;
}

//shortest job in queue 3 
int findShortestQ3(){
	int i;
	int rr_index = INT_MAX;
	for(i = s; i >= 1; i--){
		if(pcb[i].priority == 3 && pcb[i].status == IN_QUEUE && pcb[i].submit_time <= current_time->total)
		rr_index = i;
	}
return rr_index;
}

// Scheduling algorithm 
void schedule(){
	//run while active processes remain
	while(*number_waiting > 0){
		if(*turn_to == 0){
			int shortest_index;
			//find shortest job
			shortest_index = findShortestQ1();
			if(shortest_index < INT_MAX){
				//perform context switch
				switch_job(shortest_index);
				// assign time slice
				pcb[shortest_index].quantum = 45;
				printf("\nRunning shortest index in queue 1: %i with a submit time of %11.9f and time requirement of %11.9f\n", 
					shortest_index, pcb[shortest_index].submit_time, pcb[shortest_index].time_left);
				// change status to indicate now running
				pcb[shortest_index].status = IN_PROGRESS;
				
				usleep(95000); // slow down terminal output
				
				set_clock(pcb[shortest_index].burst_time);
				if((pcb[shortest_index].cycles == 2))
					pcb[shortest_index].priority++;
				*turn_to = 1;
			}
			else
				*turn_to = 1;
		}
		if(*turn_to == 1){
			int shortest_index;
			shortest_index = findShortestQ2();
			if(shortest_index < INT_MAX){
				switch_job(shortest_index);
				pcb[shortest_index].quantum = 35;
				printf("\nRunning shortest index in queue 2: %i with a submit time of %11.9f and time requirement of %11.9f\n", 
					shortest_index, pcb[shortest_index].submit_time, pcb[shortest_index].time_left);
				pcb[shortest_index].status = IN_PROGRESS;
				usleep(95000);
				set_clock(pcb[shortest_index].burst_time);
				if((pcb[shortest_index].cycles == 3))
					pcb[shortest_index].priority++;
				if(pcb[shortest_index].cycles < 3)
					pcb[shortest_index].priority--;
				*turn_to = 2;
			}
			else
				*turn_to = 2;
		}
		// Round Robin algorithm 
		if (*turn_to == 2){
			int rr_index;
			rr_index = findShortestQ3();
			if(rr_index < INT_MAX){
				switch_job(rr_index);
				pcb[rr_index].quantum = 15;
				printf("\nRunning shortest index in queue 3: %i with a submit time of %11.9f and time requirement of %11.9f\n", 
					rr_index, pcb[rr_index].submit_time, pcb[rr_index].time_left);
				pcb[rr_index].status = IN_PROGRESS;
				usleep(95000);
				set_clock(pcb[rr_index].burst_time);
				if(pcb[rr_index].cycles == 1)
					pcb[rr_index].priority--;
				*turn_to = 0;
			}
			else 
				*turn_to = 0;
		}
	}
}