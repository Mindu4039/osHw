// 2020/운영체제/hw3/B511021/김민수
// Virual Memory Simulator Homework
// One-level page table system with FIFO and LRU
// Two-level page table system with LRU
// Inverted page table with a hashing system 
// Submission Year: 2020
// Student Name: 김민수
// Student Number: B511021
//
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#define PAGESIZEBITS 12			// page size = 4Kbytes
#define VIRTUALADDRBITS 32		// virtual address space size = 4Gbytes

struct procEntry {
	char *traceName;			// the memory trace name
	int pid;					// process (trace) id
	int ntraces;				// the number of memory traces
	int num2ndLevelPageTable;	// The 2nd level page created(allocated);
	int numIHTConflictAccess; 	// The number of Inverted Hash Table Conflict Accesses
	int numIHTNULLAccess;		// The number of Empty Inverted Hash Table Accesses
	int numIHTNonNULLAcess;		// The number of Non Empty Inverted Hash Table Accesses
	int numPageFault;			// The number of page faults
	int numPageHit;				// The number of page hits
	struct pageTableEntry *firstLevelPageTable;
	FILE *tracefp;
}procTable[20];

int **valid;

struct frame {
	unsigned long int checkNum;
	int pid;
	int valid;
	struct frame *prev;
	struct frame *next;
	struct frame *prevH;
	struct frame *nextH;
}*hashTable, *frames;

struct frame *fQueue, *temp;

struct frame * ptr;
unsigned long int i, j, k;
int firLevBits, phyMemSizeBits, numProcess;
unsigned long int nFrame;

unsigned long int addr;
unsigned long int comAddr;
char rw;
int check;

void insert(struct frame *F){
	if(fQueue == NULL){
		fQueue = F;
		fQueue->next = fQueue->prev = fQueue;
	}
	else{
		F->prev = fQueue->prev;
		F->next = fQueue;
		fQueue->prev->next = F;
		fQueue->prev = F;

	}
}

void change(struct frame * F){
	if(F == fQueue){
		fQueue = fQueue->next;
	}
	else if(F == fQueue->prev){
	}
	else{
		F->next->prev = F->prev;
		F->prev->next = F->next;
		insert(F);
	}
}


void pop(){
	fQueue->checkNum = comAddr;
	fQueue->pid = procTable[i].pid;
	fQueue->prev->next = fQueue->next;
	fQueue->next->prev = fQueue->prev;
	temp = fQueue;
	fQueue = fQueue->next;
	insert(temp);
}

void insertHash(struct frame *F, struct frame *H){
	if(H->nextH == NULL || H->nextH == H){
		H->nextH = H->prevH = F;
		F->nextH = F->prevH = H;
	}
	else{
		F->nextH = H->nextH;
		H->nextH->prevH = F;
		F->prevH = H;
		H->nextH = F;
	}
}

void popHash(){
	if(fQueue->nextH == fQueue->prevH){
		fQueue->prevH->prevH = fQueue->prevH->nextH = NULL;
	}else{
	fQueue->nextH->prevH = fQueue->prevH;
	fQueue->prevH->nextH = fQueue->nextH;
	}
}

void oneLevelVMSim(int sType) {
	frames = (struct frame *) malloc(sizeof(struct frame) * (1L << (phyMemSizeBits - PAGESIZEBITS)));
	if(sType == 0){
		i = 0;
		while(fscanf(procTable[i].tracefp, "%x %c", &addr, &rw) != EOF){
			comAddr = addr & 0xfffff000;
			for(j = 0; j < nFrame; j++){
				if(frames[j].valid == 0){
					frames[j].checkNum = comAddr;
					frames[j].pid = procTable[i].pid;
					frames[j].valid = 1;
					check = 1;
					insert(&frames[j]);
					procTable[i].numPageFault++;
					break;
				}
				else{
					if((frames[j].checkNum == comAddr) && (frames[j].pid == procTable[i].pid) ){
						check = 1;
						procTable[i].numPageHit++;
						break;
					}
				}
			}
			if(check == 0){
				pop();
				procTable[i].numPageFault++;
			}
			procTable[i].ntraces++;
			check = 0;
			//printf("One-level processid %d tracenumber %d virtual addr %x physical addr %x\n", procTable[i].pid, procTable[i].ntraces, addr, fQueue->checkNum);
			i = (i + 1) % numProcess;

		}
	}
	if(sType == 1){
		i = 0;
		while(fscanf(procTable[i].tracefp, "%x %c", &addr, &rw) != EOF){
			comAddr = addr & 0xfffff000;
			for(j = 0; j < nFrame; j++){
				if(frames[j].valid == 0){
					frames[j].checkNum = comAddr;
					frames[j].pid = procTable[i].pid;
					frames[j].valid = 1;
					check = 1;
					insert(&frames[j]);
					procTable[i].numPageFault++;
					break;
				}
				else{
					if((frames[j].checkNum == comAddr) && (frames[j].pid == procTable[i].pid) ){
						change(&frames[j]);
						check = 1;
						procTable[i].numPageHit++;
						break;
					}
				}
			}
			if(check == 0){
				pop();
				procTable[i].numPageFault++;
			}
			procTable[i].ntraces++;
			check = 0;
			//printf("One-level processid %d tracenumber %d virtual addr %x physical addr %x\n", procTable[i].pid, procTable[i].ntraces, addr, fQueue->checkNum);
			i = (i + 1) % numProcess;

		}
	}

	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
	}
}

void twoLevelVMSim() {
	frames = (struct frame *) malloc(sizeof(struct frame) * (1L << (phyMemSizeBits - PAGESIZEBITS)));
	valid = (int **) malloc(sizeof(int *) * numProcess);
	for(i = 0; i < numProcess; i++){
		valid[i] = (int *)malloc(sizeof(int) * (1L << firLevBits));
	}
	unsigned long int compareNum;
	i = 0;
	while(fscanf(procTable[i].tracefp, "%x %c", &addr, &rw) != EOF){
		compareNum = (addr >> (32 - firLevBits));
		if(valid[i][compareNum] == 0){
			valid[i][compareNum] = 1;
			procTable[i].num2ndLevelPageTable++;
		}
		comAddr = addr & 0xfffff000;
		for(j = 0; j < nFrame; j++){
			if(frames[j].valid == 0){
				frames[j].checkNum = comAddr;
				frames[j].pid = procTable[i].pid;
				frames[j].valid = 1;
				check = 1;
				insert(&frames[j]);
				procTable[i].numPageFault++;
				break;
			}
			else{
				if((frames[j].checkNum == comAddr) && (frames[j].pid == procTable[i].pid) ){
					change(&frames[j]);
					check = 1;
					procTable[i].numPageHit++;
					break;
				}
			}
		}
		if(check == 0){
			pop();
			procTable[i].numPageFault++;
		}
		procTable[i].ntraces++;
		check = 0;
		//printf("One-level processid %d tracenumber %d virtual addr %x physical addr %x\n", procTable[i].pid, procTable[i].ntraces, addr, fQueue->checkNum);
		i = (i + 1) % numProcess;

	}

	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of second level page tables allocated %d\n",i,procTable[i].num2ndLevelPageTable);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
	}
}

void invertedPageVMSim() {
	frames = (struct frame *) malloc(sizeof(struct frame) * (1L << (phyMemSizeBits - PAGESIZEBITS)));
	hashTable = (struct frame *) malloc(sizeof(struct frame) * (1L << (phyMemSizeBits - PAGESIZEBITS)));
	i = 0;
	j = 0;
	int checkNode = 0;
	check = 0;
	unsigned long int hashIndex;
	while(fscanf(procTable[i].tracefp, "%x %c", &addr, &rw) != EOF){
		comAddr = addr & 0xfffff000;
		hashIndex = ((comAddr>>12) + (unsigned long int)procTable[i].pid) % nFrame;
		ptr = &hashTable[hashIndex];
		while(ptr->nextH != &hashTable[hashIndex] && ptr->nextH != NULL){
			checkNode = 1;
			ptr = ptr->nextH;
			procTable[i].numIHTConflictAccess++;
			if(ptr->checkNum == comAddr && ptr->pid == procTable[i].pid){
				change(ptr);
				check = 1;
				procTable[i].numPageHit++;
				break;
			}
		}
		if(check == 0){
			if(j < nFrame){
				frames[j].checkNum = comAddr;
				frames[j].pid = procTable[i].pid;
				frames[j].valid = 1;
				insert(&frames[j]);
				procTable[i].numPageFault++;
				insertHash(&frames[j], &hashTable[hashIndex]);
				j++;
			}
			else{
				popHash();
				insertHash(fQueue, &hashTable[hashIndex]);
				pop();
				procTable[i].numPageFault++;
			}
		}
		if(checkNode == 1){
			procTable[i].numIHTNonNULLAcess++;
			checkNode = 0;
		}
		else{
			procTable[i].numIHTNULLAccess++;
		}
		check = 0;
		procTable[i].ntraces++;
		//printf("One-level processid %d tracenumber %d virtual addr %x physical addr %x\n", procTable[i].pid, procTable[i].ntraces, addr, fQueue->checkNum);
		i = (i + 1) % numProcess;
	}

	for(i=0; i < numProcess; i++) {
		printf("**** %s *****\n",procTable[i].traceName);
		printf("Proc %d Num of traces %d\n",i,procTable[i].ntraces);
		printf("Proc %d Num of Inverted Hash Table Access Conflicts %d\n",i,procTable[i].numIHTConflictAccess);
		printf("Proc %d Num of Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNULLAccess);
		printf("Proc %d Num of Non-Empty Inverted Hash Table Access %d\n",i,procTable[i].numIHTNonNULLAcess);
		printf("Proc %d Num of Page Faults %d\n",i,procTable[i].numPageFault);
		printf("Proc %d Num of Page Hit %d\n",i,procTable[i].numPageHit);
		assert(procTable[i].numPageHit + procTable[i].numPageFault == procTable[i].ntraces);
		assert(procTable[i].numIHTNULLAccess + procTable[i].numIHTNonNULLAcess == procTable[i].ntraces);
	}
}

int main(int argc, char *argv[]) {
	int i,c, simType;
	int optind = 1;
	numProcess = argc - 4;

	if(argc < 5){
		printf("Error: We cant execute the file.\n");
		exit(1);
	}
	
	// initialize procTable for Memory Simulations
	for(i = 0; i < numProcess; i++) {
		procTable[i].traceName = argv[i + optind + 3];
		procTable[i].pid = i;

		// opening a tracefile for the process
		printf("process %d opening %s\n",i,argv[i + optind + 3]);
		procTable[i].tracefp = fopen(argv[i + optind + 3],"r");
		if (procTable[i].tracefp == NULL) {
			printf("ERROR: can't open %s file; exiting...",argv[i+optind+3]);
			exit(1);
		}
	}

	simType = atoi(argv[1]);
	firLevBits = atoi(argv[2]);
	phyMemSizeBits = atoi(argv[3]);
	nFrame = 1L << (phyMemSizeBits - PAGESIZEBITS);

	printf("Num of Frames %d Physical Memory Size %ld bytes\n",nFrame, (1L<<phyMemSizeBits));
	
	if (simType == 0) {
		printf("=============================================================\n");
		printf("The One-Level Page Table with FIFO Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		oneLevelVMSim(simType);
	}
	
	if (simType == 1) {
		printf("=============================================================\n");
		printf("The One-Level Page Table with LRU Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		oneLevelVMSim(simType);
	}
	
	if (simType == 2) {
		printf("=============================================================\n");
		printf("The Two-Level Page Table Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		twoLevelVMSim();
	}
	
	if (simType == 3) {
		printf("=============================================================\n");
		printf("The Inverted Page Table Memory Simulation Starts .....\n");
		printf("=============================================================\n");
		invertedPageVMSim();
	}

	return(0);
}
