// 2020/운영체제/hw2/b5111021/김민수
// CPU Schedule Simulator Homework
// Student Number : B511021
// Name : 김민수
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <limits.h>

#define SEED 10

// process states
#define S_IDLE 0			
#define S_READY 1
#define S_BLOCKED 2
#define S_RUNNING 3
#define S_TERMINATE 4

int NPROC, NIOREQ, QUANTUM;

struct ioDoneEvent {
	int procid;
	int doneTime;
	int len;
	struct ioDoneEvent *prev;
	struct ioDoneEvent *next;
} *ioDoneEvent;

struct ioQueue {
	int len;
	struct ioDoneEvent *front;
	struct ioDoneEvent *rear;
} ioDoneEventQueue;

struct process {
	int id;
	int len;		// for queue
	int targetServiceTime;
	int serviceTime;
	int startTime;
	int endTime;
	char state;
	int priority;
	int saveReg0, saveReg1;
	struct process *prev;
	struct process *next;
} *procTable;

struct procQueue {
	int len;
	struct process *front;
	struct process *rear;
} readyQueue, blockedQueue;

struct process idleProc;
struct process *runningProc;

int cpuReg0, cpuReg1;
int currentTime = 0;
int *procIntArrTime, *procServTime, *ioReqIntArrTime, *ioServTime;

struct process *runProc = NULL;

void compute() {
	// DO NOT CHANGE THIS FUNCTION
	cpuReg0 = cpuReg0 + runningProc->id;
	cpuReg1 = cpuReg1 + runningProc->id;
	//printf("In computer proc %d cpuReg0 %d\n",runningProc->id,cpuReg0);
}

void initProcTable() {
	int i;
	for(i=0; i < NPROC; i++) {
		procTable[i].id = i;
		procTable[i].len = 0;
		procTable[i].targetServiceTime = procServTime[i];
		procTable[i].serviceTime = 0;
		procTable[i].startTime = 0;
		procTable[i].endTime = 0;
		procTable[i].state = S_IDLE;
		procTable[i].priority = 0;
		procTable[i].saveReg0 = 0;
		procTable[i].saveReg1 = 0;
		procTable[i].prev = NULL;
		procTable[i].next = NULL;
	}
}
struct process *ptr2;
struct ioDoneEvent *ptr;

void procExecSim(struct process *(*scheduler)()) {
	int pid, qTime=0, cpuUseTime = 0, nproc=0, termProc = 0, nioreq=0;
	char schedule = 0, nextState = S_IDLE;
	int nextForkTime, nextIOReqTime;
	idleProc.id = -1;
	
	nextForkTime = procIntArrTime[nproc];
	nextIOReqTime = ioReqIntArrTime[nioreq];
	int i, num=0;
	runningProc = &idleProc;
	cpuReg0 = cpuReg1 = 0;
	while(1) {
		currentTime++;
		//printf("%d processing num %d\n",currentTime,runningProc->id);
		qTime++;
		runningProc->serviceTime++;
		if (runningProc != &idleProc ) cpuUseTime++;
		//printf("cpuusetime %d\n",cpuUseTime);
		// MUST CALL compute() Inside While loop
		compute(); 
		if (currentTime == nextForkTime) { /* CASE 2 : a new process created */
		//	printf("new process %d\n",procTable[nproc].id);
			num++;
			procTable[nproc].startTime = currentTime;
			procTable[nproc].state = S_READY;
			if(readyQueue.len == 0){
				readyQueue.front = readyQueue.rear = &procTable[nproc];
			}
			else{
				readyQueue.rear->next = &procTable[nproc];
				readyQueue.rear = &procTable[nproc];
			}
		//	printf("red queue: %d\n",readyQueue.front);
			readyQueue.len++;
			if(nproc<NPROC){
				nproc++;
				nextForkTime += procIntArrTime[nproc];
			}
			if(nproc == NPROC){
				nextForkTime = -1;
			}
			
			schedule = 1;
		}
		if (qTime == QUANTUM ) { /* CASE 1 : The quantum expires */
		//	printf("qtime \n");
		//	printf("runningProc: %d address: %d state: %d idleaddress: %d\n",runningProc->id,runningProc,runningProc->state, &idleProc);
			if(runningProc == &idleProc){
			}
			else{
				runningProc->priority--;
				schedule = 1;
			}
	/*		printf("ready len: %d\n",readyQueue.len);
			if(readyQueue.len != 0){
				printf("%d\n",readyQueue.front);
	ptr2 = readyQueue.front;
			printf("ready queue: ");runningProc->serviceTime == runningProc->targetServiceTime
			int i;
			for(i=0;i<readyQueue.len-1;i++){
				printf("%d ",ptr2->id);
				ptr2 = ptr2->next;
			}
			printf("%d\n",ptr2->id);
			}

			printf("running Proc: %d\n",runningProc->id);
			printf("%d %d\n",runningProc->serviceTime,runningProc->targetServiceTime);
			printf("cpuuse time: %d next iorequestTime: %d\n",cpuUseTime,nextIOReqTime);
			*/
		}
		while (ioDoneEventQueue.len != 0 && ioDoneEventQueue.front->doneTime == currentTime) { /* CASE 3 : IO Done Event */
	//	printf("io done event\n");
	//		printf("iodone len %d\n",ioDoneEventQueue.len);
			if(blockedQueue.front->state == S_TERMINATE){//printf("process %d was terminated\n",blockedQueue.front->id);
				if(blockedQueue.len == 1){
					blockedQueue.front = blockedQueue.rear = NULL;

					ioDoneEventQueue.front = ioDoneEventQueue.rear = NULL;
				}
				else
				{
					blockedQueue.front = blockedQueue.front->next;

					ioDoneEventQueue.front = ioDoneEventQueue.front->next;
				}
				schedule = 1;

				if(termProc == NPROC){
					runningProc->saveReg0 = cpuReg0;
					runningProc->saveReg1 = cpuReg1;
					break;
				}
			}
			else{//printf("process %d move readyqueue\n",blockedQueue.front->id);
				blockedQueue.front->state = S_READY;
				if(readyQueue.len == 0){
					readyQueue.front =readyQueue.rear = blockedQueue.front;
				}
				else{
					readyQueue.rear->next = blockedQueue.front;
					readyQueue.rear = blockedQueue.front;
				}
				readyQueue.len++;
				if(blockedQueue.len == 1){
					blockedQueue.front = blockedQueue.rear = NULL;

					ioDoneEventQueue.front = ioDoneEventQueue.rear = NULL;
				}
				else{
					blockedQueue.front = blockedQueue.front->next;

					ioDoneEventQueue.front = ioDoneEventQueue.front->next;
				}
			}
			blockedQueue.len--;
			schedule = 1;
			
			ioDoneEventQueue.len--;

			/*
			if(blockedQueue.len !=0){
			ptr2 = blockedQueue.front;
			printf("block queue: ");
			for(i=0;i<blockedQueue.len-1;i++){
				printf("%d ",ptr2->id);
				ptr2 = ptr2->next;
			}
			printf("%d\n",ptr2->id);
			}
			ptr = ioDoneEventQueue.front;
			printf("io done queue: ");
			for(i=0;i<ioDoneEventQueue.len-1;i++){
				printf("%d ",ptr);
				ptr = ptr->next;
			}*/
		//	printf("%d\n",ptr);
		//		printf("%d\n",readyQueue.front);
/*	ptr2 = readyQueue.front;
			printf("ready queue: ");
			int i;
			for(i=0;i<readyQueue.len-1;i++){
				printf("%d ",ptr2->id);
				ptr2 = ptr2->next;
			}
			printf("%d\n",ptr2->id);
*/
		}
		if (/*runningProc != &idleProc &&*/ cpuUseTime == nextIOReqTime) { /* CASE 5: reqest IO operations (only when the process does not terminate) */
	//	printf("io request\n");
	//		printf("process %d is io request\n",runningProc->id);
			ioDoneEvent[nioreq].procid = runningProc->id;
			ioDoneEvent[nioreq].doneTime = currentTime + ioServTime[nioreq];
			ioDoneEvent[nioreq].next = NULL;
			runningProc->state = S_BLOCKED;
			if(qTime != QUANTUM){
				runningProc->priority++;
			}
			//printf("donetime %d\n",ioDoneEvent[nioreq].doneTime);

			//printf("main time: %d\n",ioDoneEvent[nioreq].doneTime);
			//printf("array time: ");
			/*ptr = ioDoneEventQueue.front;
			for(i=0;i<ioDoneEventQueue.len;i++){
				printf("%d ",ptr->doneTime);
				ptr = ptr->next;
			}
			printf("\n");
*/
			if(ioDoneEventQueue.len == 0){
				ioDoneEventQueue.front = ioDoneEventQueue.rear = &ioDoneEvent[nioreq];

				blockedQueue.front = blockedQueue.rear = runningProc;
			}
			else if(ioDoneEventQueue.len == 1){
				if(ioDoneEvent[nioreq].doneTime < ioDoneEventQueue.front->doneTime){
					ioDoneEvent[nioreq].next = ioDoneEventQueue.front;
					ioDoneEventQueue.front = &ioDoneEvent[nioreq];
					
					runningProc->next = blockedQueue.front;
					blockedQueue.front = runningProc;
				}
				else{
					ioDoneEventQueue.front->next = ioDoneEventQueue.rear = &ioDoneEvent[nioreq];

					blockedQueue.front->next = blockedQueue.rear = runningProc;
				}
			}
			else{
				qTime = 0;
				ptr = ioDoneEventQueue.front;
				ptr2 = blockedQueue.front;
				if(ioDoneEvent[nioreq].doneTime < ptr->doneTime){
					ioDoneEvent[nioreq].next = ptr;
					ioDoneEventQueue.front = &ioDoneEvent[nioreq];

					runningProc->next = ptr2;
					blockedQueue.front = runningProc;
				}
				else{
					for(i=1; i<=ioDoneEventQueue.len; i++){
						if(i<ioDoneEventQueue.len){
						if(ioDoneEvent[nioreq].doneTime >= ptr->doneTime && ioDoneEvent[nioreq].doneTime < ptr->next->doneTime){
							ioDoneEvent[nioreq].next =ptr->next;
							ptr->next = &ioDoneEvent[nioreq];
						
							runningProc->next = ptr2->next;
				qTime = 0;
							ptr2->next = runningProc;
							break;
						}
						ptr = ptr->next;
						ptr2 = ptr2->next;
						}
						else{
							if(ioDoneEvent[nioreq].doneTime >= ptr->doneTime){
							ptr->next = &ioDoneEvent[nioreq];
							ioDoneEventQueue.rear = &ioDoneEvent[nioreq];
	
							ptr2->next = runningProc;
							blockedQueue.rear = runningProc;
							}
						}
					}
				}
			}
			
			ioDoneEventQueue.len++;
			if(nioreq<NIOREQ){
				nioreq++;
/*				
				printf("%d\n",readyQueue.front);
	ptr2 = readyQueue.front;
			printf("ready queue: ");
			int i;
			for(i=0;i<readyQueue.len-1;i++){
				printf("%d ",ptr2->id);
				ptr2 = ptr2->next;
			}
			printf("%d\n",ptr2->id);
*/
				nextIOReqTime += ioReqIntArrTime[nioreq];
			}
			if(nioreq == NIOREQ){
				nextIOReqTime = -1;
			}
			blockedQueue.len++;
			schedule = 1;
/*
			if(blockedQueue.len != 0){	
			ptr2 = blockedQueue.front;
			printf("block queue: ");
			for(i=0;i<blockedQueue.len-1;i++){
				printf("%d ",ptr2->id);
				ptr2 = ptr2->next;
			}
			printf("%d\n",ptr2->id);
			}
			if(ioDoneEventQueue.len !=0){
			ptr = ioDoneEventQueue.front;
			printf("io done queue: ");
			for(i=0;i<ioDoneEventQueue.len-1;i++){
				printf("%d ",ptr->doneTime);
				ptr = ptr->next;
			}
			printf("%d\n",ptr->doneTime);
			}
*/		
		}
		if (runningProc->serviceTime == runningProc->targetServiceTime) { /* CASE 4 : the process job done and terminates */
	//		printf("process done\n");
	//		printf("process %d is terminated\n",runningProc->id);
			runningProc->state = S_TERMINATE;
			runningProc->endTime = currentTime;
			termProc++;
	//		printf(" termi num: %d\n",termProc);
			if(termProc == NPROC){
				runningProc->saveReg0 = cpuReg0;
				runningProc->saveReg1 = cpuReg1;
				break;
			}
			schedule = 1;
		}
		// call scheduler() if needed
		if(schedule == 1){
			runningProc->saveReg0 = cpuReg0;
			runningProc->saveReg1 = cpuReg1;
			runningProc = scheduler();
			if(runningProc != &idleProc){
			cpuReg0 = runningProc->saveReg0;
			cpuReg1 = runningProc->saveReg1;
			runningProc->state = S_RUNNING;
			}
			schedule = 0;
			qTime = 0;
		}
	} // while loop
}

//RR,SJF(Modified),SRTN,Guaranteed Scheduling(modified),Simple Feed Back Scheduling
struct process* RRschedule() {
//printf("RR schedule\n");
	if(runningProc->state == S_RUNNING){
		if(readyQueue.len == 0){
			readyQueue.front = readyQueue.rear = runningProc;
		}else{
			readyQueue.rear->next = runningProc;
			readyQueue.rear = runningProc;
		}
		runningProc->state = S_READY;
		readyQueue.len++;
	}
	
	if(readyQueue.len == 0){
		runProc = &idleProc;
	}
	else{
		runProc = readyQueue.front;
		if(readyQueue.len == 1){
			readyQueue.front = readyQueue.rear = NULL;
		}
		else{
			readyQueue.front = readyQueue.front->next;
		}
		readyQueue.len--;
	}

	return runProc;
}
struct process* SJFschedule() {
	int i;
	struct process *temp, *pTemp,*result, *pResult;
	if(runningProc->state == S_RUNNING){
		if(readyQueue.len == 0){
			readyQueue.front = readyQueue.rear = runningProc;
		}else{
			readyQueue.rear->next = runningProc;
			readyQueue.rear = runningProc;
		}
		runningProc->state = S_READY;
		readyQueue.len++;
	}

	if(readyQueue.len == 0){
		runProc = &idleProc;
	}
	else {
		if(readyQueue.len == 1){
			runProc = readyQueue.front;
			readyQueue.front = readyQueue.rear = NULL;
			readyQueue.len--;
		}
		else{
			result = readyQueue.front;
			pResult= pTemp = readyQueue.front;
			temp = readyQueue.front->next;
			for(i=2;i<=readyQueue.len;i++){
				if(temp->targetServiceTime < result->targetServiceTime){
					result = temp;
					pResult = pTemp;
					
				}
				pTemp = temp;
				temp = temp->next;
			}
			if(result == readyQueue.front){
				runProc = readyQueue.front;
				readyQueue.front = readyQueue.front->next;
				readyQueue.len--;
			}
			else if(result == readyQueue.rear){
				runProc = readyQueue.rear;
				readyQueue.rear = pResult;
				readyQueue.len--;
			}
			else{
				runProc = result;
				pResult->next = result->next;
				readyQueue.len--;
			}
		}
	}

	return runProc;
}
struct process* SRTNschedule() {
	int i;
	struct process *temp, *pTemp,*result, *pResult;
	if(runningProc->state == S_RUNNING){
		if(readyQueue.len == 0){
			readyQueue.front = readyQueue.rear = runningProc;
		}else{
			readyQueue.rear->next = runningProc;
			readyQueue.rear = runningProc;
		}
		runningProc->state = S_READY;
		readyQueue.len++;
	}

	if(readyQueue.len == 0){
		runProc = &idleProc;
	}
	else {
		if(readyQueue.len == 1){
			runProc = readyQueue.front;
			readyQueue.front = readyQueue.rear = NULL;
			readyQueue.len--;
		}
		else{
			result = readyQueue.front;
			pResult= pTemp = readyQueue.front;
			temp = readyQueue.front->next;
			for(i=2;i<=readyQueue.len;i++){
				if(temp->targetServiceTime - temp->serviceTime < result->targetServiceTime- result->serviceTime){
					result = temp;
					pResult = pTemp;
					
				}
				pTemp = temp;
				temp = temp->next;
			}
			if(result == readyQueue.front){
				runProc = readyQueue.front;
				readyQueue.front = readyQueue.front->next;
				readyQueue.len--;
			}
			else if(result == readyQueue.rear){
				runProc = readyQueue.rear;
				readyQueue.rear = pResult;
				readyQueue.len--;
			}
			else{
				runProc = result;
				pResult->next = result->next;
				readyQueue.len--;
			}
		}
	}

	return runProc;
}
struct process* GSschedule() {
	int i;
	struct process *temp, *pTemp,*result, *pResult;
	if(runningProc->state == S_RUNNING){
		if(readyQueue.len == 0){
			readyQueue.front = readyQueue.rear = runningProc;
		}else{
			readyQueue.rear->next = runningProc;
			readyQueue.rear = runningProc;
		}
		runningProc->state = S_READY;
		readyQueue.len++;
	}

	if(readyQueue.len == 0){
		runProc = &idleProc;
	}
	else {
		if(readyQueue.len == 1){
			runProc = readyQueue.front;
			readyQueue.front = readyQueue.rear = NULL;
			readyQueue.len--;
		}
		else{
			result = readyQueue.front;
			pResult= pTemp = readyQueue.front;
			temp = readyQueue.front->next;
			for(i=2;i<=readyQueue.len;i++){
				if((double)(temp->serviceTime) / (double)(temp->targetServiceTime) < (double)(result->serviceTime) / (double)(result->targetServiceTime)){
					result = temp;
					pResult = pTemp;
					
				}
				pTemp = temp;
				temp = temp->next;
			}
			if(result == readyQueue.front){
				runProc = readyQueue.front;
				readyQueue.front = readyQueue.front->next;
				readyQueue.len--;
			}
			else if(result == readyQueue.rear){
				runProc = readyQueue.rear;
				readyQueue.rear = pResult;
				readyQueue.len--;
			}
			else{
				runProc = result;
				pResult->next = result->next;
				readyQueue.len--;
			}
		}
	}
	return runProc;
}
struct process* SFSschedule() {
	int i;
	struct process *temp, *pTemp,*result, *pResult;
	if(runningProc->state == S_RUNNING){
		if(readyQueue.len == 0){
			readyQueue.front = readyQueue.rear = runningProc;
		}else{
			readyQueue.rear->next = runningProc;
			readyQueue.rear = runningProc;
		}
		runningProc->state = S_READY;
		readyQueue.len++;
	}

	if(readyQueue.len == 0){
		runProc = &idleProc;
	}
	else {
		if(readyQueue.len == 1){
			runProc = readyQueue.front;
			readyQueue.front = readyQueue.rear = NULL;
			readyQueue.len--;
		}
		else{
			result = readyQueue.front;
			pResult= pTemp = readyQueue.front;
			temp = readyQueue.front->next;
			for(i=2;i<=readyQueue.len;i++){
				if(temp->priority > result->priority){
					result = temp;
					pResult = pTemp;
					
				}
				pTemp = temp;
				temp = temp->next;
			}
			if(result == readyQueue.front){
				runProc = readyQueue.front;
				readyQueue.front = readyQueue.front->next;
				readyQueue.len--;
			}
			else if(result == readyQueue.rear){
				runProc = readyQueue.rear;
				readyQueue.rear = pResult;
				readyQueue.len--;
			}
			else{
				runProc = result;
				pResult->next = result->next;
				readyQueue.len--;
			}
		}
	}
	return runProc;
}

void printResult() {
	// DO NOT CHANGE THIS FUNCTION
	int i;
	long totalProcIntArrTime=0,totalProcServTime=0,totalIOReqIntArrTime=0,totalIOServTime=0;
	long totalWallTime=0, totalRegValue=0;
	for(i=0; i < NPROC; i++) {
		totalWallTime += procTable[i].endTime - procTable[i].startTime;
		/*
		printf("proc %d serviceTime %d targetServiceTime %d saveReg0 %d\n",
			i,procTable[i].serviceTime,procTable[i].targetServiceTime, procTable[i].saveReg0);
		*/
		totalRegValue += procTable[i].saveReg0+procTable[i].saveReg1;
		/* printf("reg0 %d reg1 %d totalRegValue %d\n",procTable[i].saveReg0,procTable[i].saveReg1,totalRegValue);*/
	}
	for(i = 0; i < NPROC; i++ ) { 
		totalProcIntArrTime += procIntArrTime[i];
		totalProcServTime += procServTime[i];
	}
	for(i = 0; i < NIOREQ; i++ ) { 
		totalIOReqIntArrTime += ioReqIntArrTime[i];
		totalIOServTime += ioServTime[i];
	}
	
	printf("Avg Proc Inter Arrival Time : %g \tAverage Proc Service Time : %g\n", (float) totalProcIntArrTime/NPROC, (float) totalProcServTime/NPROC);
	printf("Avg IOReq Inter Arrival Time : %g \tAverage IOReq Service Time : %g\n", (float) totalIOReqIntArrTime/NIOREQ, (float) totalIOServTime/NIOREQ);
	printf("%d Process processed with %d IO requests\n", NPROC,NIOREQ);
	printf("Average Wall Clock Service Time : %g \tAverage Two Register Sum Value %g\n", (float) totalWallTime/NPROC, (float) totalRegValue/NPROC);
	
}

int main(int argc, char *argv[]) {
	// DO NOT CHANGE THIS FUNCTION
	int i;
	int totalProcServTime = 0, ioReqAvgIntArrTime;
	int SCHEDULING_METHOD, MIN_INT_ARRTIME, MAX_INT_ARRTIME, MIN_SERVTIME, MAX_SERVTIME, MIN_IO_SERVTIME, MAX_IO_SERVTIME, MIN_IOREQ_INT_ARRTIME;
	
	if (argc < 12) {
		printf("%s: SCHEDULING_METHOD NPROC NIOREQ QUANTUM MIN_INT_ARRTIME MAX_INT_ARRTIME MIN_SERVTIME MAX_SERVTIME MIN_IO_SERVTIME MAX_IO_SERVTIME MIN_IOREQ_INT_ARRTIME\n",argv[0]);
		exit(1);
	}
	
	SCHEDULING_METHOD = atoi(argv[1]);
	NPROC = atoi(argv[2]);
	NIOREQ = atoi(argv[3]);
	QUANTUM = atoi(argv[4]);
	MIN_INT_ARRTIME = atoi(argv[5]);
	MAX_INT_ARRTIME = atoi(argv[6]);
	MIN_SERVTIME = atoi(argv[7]);
	MAX_SERVTIME = atoi(argv[8]);
	MIN_IO_SERVTIME = atoi(argv[9]);
	MAX_IO_SERVTIME = atoi(argv[10]);
	MIN_IOREQ_INT_ARRTIME = atoi(argv[11]);
	
	printf("SIMULATION PARAMETERS : SCHEDULING_METHOD %d NPROC %d NIOREQ %d QUANTUM %d \n", SCHEDULING_METHOD, NPROC, NIOREQ, QUANTUM);
	printf("MIN_INT_ARRTIME %d MAX_INT_ARRTIME %d MIN_SERVTIME %d MAX_SERVTIME %d\n", MIN_INT_ARRTIME, MAX_INT_ARRTIME, MIN_SERVTIME, MAX_SERVTIME);
	printf("MIN_IO_SERVTIME %d MAX_IO_SERVTIME %d MIN_IOREQ_INT_ARRTIME %d\n", MIN_IO_SERVTIME, MAX_IO_SERVTIME, MIN_IOREQ_INT_ARRTIME);
	
	srandom(SEED);
	
	// allocate array structures
	procTable = (struct process *) malloc(sizeof(struct process) * NPROC);
	ioDoneEvent = (struct ioDoneEvent *) malloc(sizeof(struct ioDoneEvent) * NIOREQ);
	procIntArrTime = (int *) malloc(sizeof(int) * NPROC);
	procServTime = (int *) malloc(sizeof(int) * NPROC);
	ioReqIntArrTime = (int *) malloc(sizeof(int) * NIOREQ);
	ioServTime = (int *) malloc(sizeof(int) * NIOREQ);

	// initialize queues
	readyQueue.front = readyQueue.rear = NULL;
	
	blockedQueue.front = blockedQueue.rear = NULL;
	ioDoneEventQueue.front = ioDoneEventQueue.rear = NULL;
	ioDoneEventQueue.len  = readyQueue.len = blockedQueue.len = 0;
	
	// generate process interarrival times
	for(i = 0; i < NPROC; i++ ) { 
		procIntArrTime[i] = random()%(MAX_INT_ARRTIME - MIN_INT_ARRTIME+1) + MIN_INT_ARRTIME;
	}
	
	// assign service time for each process
	for(i=0; i < NPROC; i++) {
		procServTime[i] = random()% (MAX_SERVTIME - MIN_SERVTIME + 1) + MIN_SERVTIME;
		totalProcServTime += procServTime[i];	
	}
	
	ioReqAvgIntArrTime = totalProcServTime/(NIOREQ+1);
	assert(ioReqAvgIntArrTime > MIN_IOREQ_INT_ARRTIME);
	
	// generate io request interarrival time
	for(i = 0; i < NIOREQ; i++ ) { 
		ioReqIntArrTime[i] = random()%((ioReqAvgIntArrTime - MIN_IOREQ_INT_ARRTIME)*2+1) + MIN_IOREQ_INT_ARRTIME;
	}
	
	// generate io request service time
	for(i = 0; i < NIOREQ; i++ ) { 
		ioServTime[i] = random()%(MAX_IO_SERVTIME - MIN_IO_SERVTIME+1) + MIN_IO_SERVTIME;
	}
	
#ifdef DEBUG
	// printing process interarrival time and service time
	printf("Process Interarrival Time :\n");
	for(i = 0; i < NPROC; i++ ) { 
		printf("%d ",procIntArrTime[i]);
	}
	printf("\n");
	printf("Process Target Service Time :\n");
	for(i = 0; i < NPROC; i++ ) { 
		printf("%d ",procTable[i].targetServiceTime);
	}
	printf("\n");
#endif
	
	// printing io request interarrival time and io request service time
	printf("IO Req Average InterArrival Time %d\n", ioReqAvgIntArrTime);
	printf("IO Req InterArrival Time range : %d ~ %d\n",MIN_IOREQ_INT_ARRTIME,
			(ioReqAvgIntArrTime - MIN_IOREQ_INT_ARRTIME)*2+ MIN_IOREQ_INT_ARRTIME);
			
#ifdef DEBUG		
	printf("IO Req Interarrival Time :\n");
	for(i = 0; i < NIOREQ; i++ ) { 
		printf("%d ",ioReqIntArrTime[i]);
	}
	printf("\n");
	printf("IO Req Service Time :\n");
	for(i = 0; i < NIOREQ; i++ ) { 
		printf("%d ",ioServTime[i]);
	}
	printf("\n");
#endif
	
	struct process* (*schFunc)();
	switch(SCHEDULING_METHOD) {
		case 1 : schFunc = RRschedule; break;
		case 2 : schFunc = SJFschedule; break;
		case 3 : schFunc = SRTNschedule; break;
		case 4 : schFunc = GSschedule; break;
		case 5 : schFunc = SFSschedule; break;
		default : printf("ERROR : Unknown Scheduling Method\n"); exit(1);
	}
	initProcTable();
	procExecSim(schFunc);
	printResult();
	
}
