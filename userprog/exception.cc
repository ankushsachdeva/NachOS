// EXCEPTION
// exception.cc 
//	Entry point into the Nachos kernel from user programs.
//	There are two kinds of things that can cause control to
//	transfer back to here from user code:
//
//	syscall -- The user code explicitly requests to call a procedure
//	in the Nachos kernel.  Right now, the only function we support is
//	"Halt".
//
//	exceptions -- The user code does something that the CPU can't handle.
//	For instance, accessing memory that doesn't exist, arithmetic errors,
//	etc.  
//
//	Interrupts (which can also cause control to transfer from user
//	code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "console.h"
#include "synch.h"

//----------------------------------------------------------------------
// ExceptionHandler
// 	Entry point into the Nachos kernel.  Called when a user program
//	is executing, and either does a syscall, or generates an addressing
//	or arithmetic exception.
//
// 	For system calls, the following is the calling convention:
//
// 	system call code -- r2
//		arg1 -- r4
//		arg2 -- r5
//		arg3 -- r6
//		arg4 -- r7
//
//	The result of the system call, if any, must be put back into r2. 
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//	"which" is the kind of exception.  The list of possible exceptions 
//	are in machine.h.
//----------------------------------------------------------------------
static Semaphore *readAvail;
static Semaphore *writeDone;
static void ReadAvail(int arg) { readAvail->V(); }
static void WriteDone(int arg) { writeDone->V(); }

extern void StartProcess (char*);
extern int threadCount;

void
ForkStartFunction (int dummy)
{
   currentThread->Startup();
   machine->Run();
}

void completionStats()
{
  int sum = 0;
  double avg;
  for(int i = 1; i < thread_index; i++)
    sum += completionTimeArray[i];
  
  avg = ((double)sum)/(thread_index-1);

  double var = 0;
  for(int i = 1; i < thread_index; i++)
    var += (completionTimeArray[i] - avg)*(completionTimeArray[i] - avg);
  
  var /= (thread_index-1);
  
  int maxCompletion = 0;
  for(int i = 1; i < thread_index; i++)
    if(maxCompletion < completionTimeArray[i])
      maxCompletion = completionTimeArray[i];
  int minCompletion = 10000000;
  for(int i = 1; i < thread_index; i++)
    if(minCompletion > completionTimeArray[i])
      minCompletion = completionTimeArray[i];
  printf("Maximum Completion Time :: %d\n", maxCompletion);
  printf("Minimum Completion Time :: %d\n", minCompletion);
  printf("Average Completion Time :: %lf\n", avg);
  printf("Variance of Completion Time :: %lf\n", var);
  
}

static void ConvertIntToHex (unsigned v, Console *console)
{
   unsigned x;
   if (v == 0) return;
   ConvertIntToHex (v/16, console);
   x = v % 16;
   if (x < 10) {
      writeDone->P() ;
      console->PutChar('0'+x);
   }
   else {
      writeDone->P() ;
      console->PutChar('a'+x-10);
   }
}

void
ExceptionHandler(ExceptionType which)
{
    int type = machine->ReadRegister(2);
    int memval, vaddr, printval, tempval, exp;
    unsigned printvalus;	// Used for printing in hex
    if (!initializedConsoleSemaphores) {
       readAvail = new Semaphore("read avail", 0);
       writeDone = new Semaphore("write done", 1);
       initializedConsoleSemaphores = true;
    }
    Console *console = new Console(NULL, NULL, ReadAvail, WriteDone, 0);
    int exitcode;		// Used in SC_Exit
    unsigned i;
    char buffer[1024];		// Used in SC_Exec
    int waitpid;		// Used in SC_Join
    int whichChild;		// Used in SC_Join
    Thread *child;		// Used by SC_Fork
    unsigned sleeptime;		// Used by SC_Sleep

    if ((which == SyscallException) && (type == SC_Halt)) {
	DEBUG('a', "Shutdown, initiated by user program.\n");
   	interrupt->Halt();
    }
    else if ((which == SyscallException) && (type == SC_Exit)) {
       exitcode = machine->ReadRegister(4);
       printf("[pid %d]: Exit called. Code: %d %s\n", currentThread->GetPID(), exitcode, currentThread->name);

       threadCount--;
       printf("Thread Count : %d\n",threadCount);
       if(threadCount==1){
         printf("WaitTime :: %d , TotalBurst :: %d , ", currentThread->totalWait, currentThread->totalBurst);
         printf("ExecutionTime :: %d\n", stats->totalTicks-currentThread->startingTime);


         completionTimeArray[currentThread->GetPID()] = stats->totalTicks;
         ASSERT(currentThread->current_burst_init_value >= 0);
         
         int lastBurst = (stats->totalTicks - currentThread->current_burst_init_value);
         currentThread->totalBurst += lastBurst;


         //
         totalWaitTime += currentThread->totalWait;
         num_waits++;
         totalBurstTime += currentThread->totalBurst;

         if(lastBurst>0) {
             num_bursts++;
             if(lastBurst < min_burst) min_burst = lastBurst;
         }

         if(lastBurst>max_burst) max_burst=lastBurst;
         //
         simulationTime = stats->totalTicks - startTime;
         printf("Total Simulation time :: %d\nBurst Time :: %ld\n", simulationTime, totalBurstTime);
         printf("Maximum Burst :: %d Minimum Burst :: %d\n", max_burst, min_burst);
         printf("Number of bursts:: %d  Average Burst Time :: %d\n", num_bursts, totalBurstTime/ num_bursts);
         printf("Start Time :: %d\n", startTime);
         printf("Total Wait Time :: %d\n", totalWaitTime);
         printf("Average Wait Time :: %d\n", totalWaitTime/ num_waits);
         printf("Burst Efficiency :: %lf\n", ((double)totalBurstTime/simulationTime)*100); 
         if(scheduling_algorithm==2)
           printf("Burst Time Estimation Error :: %lf\n",(double)burstErrorEstimation/totalBurstTime);

         completionStats();
         interrupt->Halt();
       }

       // We do not wait for the children to finish.
       // The children will continue to run.
       // We will worry about this when and if we implement signals.
       exitThreadArray[currentThread->GetPID()] = true;

       // Find out if all threads have called exit
       for (i=0; i<thread_index; i++) {
          if (!exitThreadArray[i]) break;
       }
       currentThread->Exit(i==thread_index, exitcode);
    }
    else if ((which == SyscallException) && (type == SC_Exec)) {
       // Copy the executable name into kernel space
       vaddr = machine->ReadRegister(4);
       machine->ReadMem(vaddr, 1, &memval);
       i = 0;
       while ((*(char*)&memval) != '\0') {
          buffer[i] = (*(char*)&memval);
          i++;
          vaddr++;
          machine->ReadMem(vaddr, 1, &memval);
       }
       buffer[i] = (*(char*)&memval);
       StartProcess(buffer);
    }
    else if ((which == SyscallException) && (type == SC_Join)) {
       waitpid = machine->ReadRegister(4);
       // Check if this is my child. If not, return -1.
       whichChild = currentThread->CheckIfChild (waitpid);
       if (whichChild == -1) {
          printf("[pid %d] Cannot join with non-existent child [pid %d].\n", currentThread->GetPID(), waitpid);
          machine->WriteRegister(2, -1);
          // Advance program counters.
          machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
          machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
          machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
       }
       else {
          exitcode = currentThread->JoinWithChild (whichChild);
          machine->WriteRegister(2, exitcode);
          // Advance program counters.
          machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
          machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
          machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
       }
    }
    else if ((which == SyscallException) && (type == SC_Fork)) {
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
       
       child = new Thread("Forked thread");
       child->space = new AddrSpace (currentThread->space);  // Duplicates the address space
       child->SaveUserState ();		     		      // Duplicate the register set
       child->ResetReturnValue ();			     // Sets the return register to zero
       child->StackAllocate (ForkStartFunction, 0);	// Make it ready for a later context switch
       child->Schedule ();
       machine->WriteRegister(2, child->GetPID());		// Return value for parent
    }
    else if ((which == SyscallException) && (type == SC_Yield)) {
       currentThread->Yield();
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SC_PrintInt)) {

       printval = machine->ReadRegister(4);
       if (printval == 0) {
          writeDone->P() ;
          console->PutChar('0');
       }
       else {
          if (printval < 0) {
             writeDone->P() ;
             console->PutChar('-');
             printval = -printval;
          }
          tempval = printval;
          exp=1;
          while (tempval != 0) {
             tempval = tempval/10;
             exp = exp*10;
          }
          exp = exp/10;
          while (exp > 0) {
             writeDone->P() ;
             console->PutChar('0'+(printval/exp));
             printval = printval % exp;
             exp = exp/10;
          }
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SC_PrintChar)) {
        writeDone->P() ;        // wait for previous write to finish
        console->PutChar(machine->ReadRegister(4));   // echo it!
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SC_PrintString)) {
       vaddr = machine->ReadRegister(4);
       machine->ReadMem(vaddr, 1, &memval);
       while ((*(char*)&memval) != '\0') {
          writeDone->P() ;
          console->PutChar(*(char*)&memval);
          vaddr++;
          machine->ReadMem(vaddr, 1, &memval);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SC_GetReg)) {
       machine->WriteRegister(2, machine->ReadRegister(machine->ReadRegister(4))); // Return value
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SC_GetPA)) {
       vaddr = machine->ReadRegister(4);
       machine->WriteRegister(2, machine->GetPA(vaddr));  // Return value
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SC_GetPID)) {
       machine->WriteRegister(2, currentThread->GetPID());
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SC_GetPPID)) {
       machine->WriteRegister(2, currentThread->GetPPID());
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SC_Sleep)) {
       sleeptime = machine->ReadRegister(4);
       if (sleeptime == 0) {
          // emulate a yield
          currentThread->Yield();
       }
       else {
          currentThread->SortedInsertInWaitQueue (sleeptime+stats->totalTicks);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SC_Time)) {
       machine->WriteRegister(2, stats->totalTicks);
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    }
    else if ((which == SyscallException) && (type == SC_PrintIntHex)) {
       printvalus = (unsigned)machine->ReadRegister(4);
       writeDone->P() ;
       console->PutChar('0');
       writeDone->P() ;
       console->PutChar('x');
       if (printvalus == 0) {
          writeDone->P() ;
          console->PutChar('0');
       }
       else {
          ConvertIntToHex (printvalus, console);
       }
       // Advance program counters.
       machine->WriteRegister(PrevPCReg, machine->ReadRegister(PCReg));
       machine->WriteRegister(PCReg, machine->ReadRegister(NextPCReg));
       machine->WriteRegister(NextPCReg, machine->ReadRegister(NextPCReg)+4);
    } else {
	printf("Unexpected user mode exception %d %d\n", which, type);
	ASSERT(FALSE);
    }
}
