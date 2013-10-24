// progtest.cc 
//	Test routines for demonstrating that Nachos can load
//	a user program and execute it.  
//
//	Also, routines for testing the Console hardware device.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "console.h"
#include "addrspace.h"
#include "synch.h"

void StartFun(int k){
        currentThread->Startup();
        machine->Run();
        ASSERT(FALSE);
}

//----------------------------------------------------------------------
// StartProcess
// 	Run a user program.  Open the executable, load it into
//	memory, and jump to it.
//----------------------------------------------------------------------

void
StartProcess(char *filename)
{
    OpenFile *executable = fileSystem->Open(filename);
    AddrSpace *space;

    if (executable == NULL) {
	printf("Unable to open file %s\n", filename);
	return;
    }
    space = new AddrSpace(executable);    
    currentThread->space = space;

    delete executable;			// close file

    space->InitRegisters();		// set the initial register values
    space->RestoreState();		// load page table register
      // currentThread->current_burst_init_value=stats->totalTicks;

    machine->Run();			// jump to the user progam
    ASSERT(FALSE);			// machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}

// Data structures needed for the console test.  Threads making
// I/O requests wait on a Semaphore to delay until the I/O completes.

static Console *console;
static Semaphore *readAvail;
static Semaphore *writeDone;

//----------------------------------------------------------------------
// ConsoleInterruptHandlers
// 	Wake up the thread that requested the I/O.
//----------------------------------------------------------------------

static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

//----------------------------------------------------------------------
// ConsoleTest
// 	Test the console by echoing characters typed at the input onto
//	the output.  Stop when the user types a 'q'.
//----------------------------------------------------------------------

void 
ConsoleTest (char *in, char *out)
{
    char ch;

    console = new Console(in, out, ReadAvail, WriteDone, 0);
    readAvail = new Semaphore("read avail", 0);
    writeDone = new Semaphore("write done", 0);
    
    for (;;) {
	readAvail->P();		// wait for character to arrive
	ch = console->GetChar();
	console->PutChar(ch);	// echo it!
	writeDone->P() ;        // wait for write to finish
	if (ch == 'q') return;  // if q, quit
    }
}


/////////////////////////////////////////////////////////////////////////


static void
TimerInterruptHandler(int dummy)
{
    TimeSortedWaitQueue *ptr;
    if (interrupt->getStatus() != IdleMode) {
        // Check the head of the sleep queue
        while ((sleepQueueHead != NULL) && (sleepQueueHead->GetWhen() <= (unsigned)stats->totalTicks)) {
           sleepQueueHead->GetThread()->Schedule();
           ptr = sleepQueueHead;
           sleepQueueHead = sleepQueueHead->GetNext();
           delete ptr;
        }
        //printf("[%d] Timer interrupt.\n", stats->totalTicks);
        interrupt->YieldOnReturn();
    }
    //printf("AAAA\n");
    // pre-emptive scheduling
    if (scheduling_algorithm >= 3)
    {
      DEBUG('j', "S1cheduling :: %d\n", schedulingQuantum);
        if (currentThread->current_burst_init_value + schedulingQuantum >= stats->totalTicks)
        {
            //currentThread->Yield();
            interrupt->YieldOnReturn();
        }
        
    }
    
}


void
StartJobs(List *jobList)
{
  ListElement *elem = jobList->getFirst();
  
 
  while(elem != NULL)
  {
          //printf("%s :: ", elem->item);
          //printf("%d\n", elem->key);
          OpenFile *executable = fileSystem->Open((char *)elem->item);
          AddrSpace *space;
          
          if (executable == NULL) {
                  printf("Unable to open file %s\n", elem->item);
                  //return;
          }
          else{
                  printf("%s :: ", elem->item);
                  printf("%d\n", elem->key);

                  Thread* newThread;
                  newThread = new Thread((char *)elem->item, true, elem->key);
                // update priority
                  newThread->basePriority = elem->key;
                  space = new AddrSpace(executable);    
                  newThread->space = space;
                  
                  delete executable;			// close file
                  
                  newThread->space->InitRegisters();		// set the initial register values
                  // load page table register
                  newThread->SaveUserState();
                  newThread->StackAllocate(StartFun, 0);
                  interrupt->SetLevel(IntOff);
                  newThread->Schedule();
                  interrupt->SetLevel(IntOn);
                  
          }
          elem = elem->next;
          printf("Done\n");
  }
  if(scheduling_algorithm>=3){
    printf("BBBB\n");
    timer = new Timer(TimerInterruptHandler, 0, FALSE);
  }

  
  currentThread->Finish();
  //machine->Run();			// jump to the user progam
  ASSERT(FALSE);			// machine->Run never returns;
					// the address space exits
					// by doing the syscall "exit"
}
