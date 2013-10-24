// main.cc 
//	Bootstrap code to initialize the operating system kernel.
//
//	Allows direct calls into internal operating system functions,
//	to simplify debugging and testing.  In practice, the
//	bootstrap code would just initialize data structures,
//	and start a user program to print the login prompt.
//
// 	Most of this file is not needed until later assignments.
//
// Usage: nachos -d <debugflags> -rs <random seed #>
//		-s -x <nachos file> -c <consoleIn> <consoleOut>
//		-f -cp <unix file> <nachos file>
//		-p <nachos file> -r <nachos file> -l -D -t
//              -n <network reliability> -m <machine id>
//              -o <other machine id>
//              -z
//
//    -d causes certain debugging messages to be printed (cf. utility.h)
//    -rs causes Yield to occur at random (but repeatable) spots
//    -z prints the copyright message
//
//  USER_PROGRAM
//    -s causes user programs to be executed in single-step mode
//    -x runs a user program
//    -c tests the console
//
//  FILESYS
//    -f causes the physical disk to be formatted
//    -cp copies a file from UNIX to Nachos
//    -p prints a Nachos file to stdout
//    -r removes a Nachos file from the file system
//    -l lists the contents of the Nachos directory
//    -D prints the contents of the entire file system 
//    -t tests the performance of the Nachos file system
//
//  NETWORK
//    -n sets the network reliability
//    -m sets this machine's host id (needed for the network)
//    -o runs a simple test of the Nachos network software
//
//  NOTE -- flags are ignored until the relevant assignment.
//  Some of the flags are interpreted here; some in system.cc.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation 
// of liability and disclaimer of warranty provisions.

#define MAIN
#include "copyright.h"
#undef MAIN

#include "utility.h"
#include "system.h"


// External functions used by this file

extern void ThreadTest(void), Copy(char *unixFile, char *nachosFile);
extern void Print(char *file), PerformanceTest(void);
extern void StartProcess(char *file), ConsoleTest(char *in, char *out);
extern void MailTest(int networkID);
extern void StartJobs(List* jobL);
List* handleBatch(FILE *jobList); 
extern int threadCount;


//----------------------------------------------------------------------
// main
// 	Bootstrap the operating system kernel.  
//	
//	Check command line arguments
//	Initialize data structures
//	(optionally) Call test procedure
//
//	"argc" is the number of command line arguments (including the name
//		of the command) -- ex: "nachos -d +" -> argc = 3 
//	"argv" is an array of strings, one for each command line argument
//		ex: "nachos -d +" -> argv = {"nachos", "-d", "+"}
//----------------------------------------------------------------------

int
main(int argc, char **argv)
{
  threadCount = 0;
  totalWaitTime = 0;
  totalBurstTime = 0;
  startTime = -1;

    int argCount;			// the number of arguments 
					// for a particular command

    DEBUG('t', "Entering main");
    (void) Initialize(argc, argv);
    
#ifdef THREADS
    ThreadTest();
#endif

    for (argc--, argv++; argc > 0; argc -= argCount, argv += argCount) {
	argCount = 1;
        if (!strcmp(*argv, "-z"))               // print copyright
            printf (copyright);
#ifdef USER_PROGRAM
        if (!strcmp(*argv, "-x")) {        	// run a user program
	    ASSERT(argc > 1);
            StartProcess(*(argv + 1));
            argCount = 2;
        } else if (!strcmp(*argv, "-c")) {      // test the console
	    if (argc == 1)
	        ConsoleTest(NULL, NULL);
	    else {
		ASSERT(argc > 2);
	        ConsoleTest(*(argv + 1), *(argv + 2));
	        argCount = 3;
	    }
	    interrupt->Halt();		// once we start the console, then 
					// Nachos will loop forever waiting 
					// for console input
	} else if (!strcmp(*argv, "-F")) {
                FILE *inp;
                inp = fopen (*(argv + 1), "r");
                ASSERT(!(inp == NULL));
                List *jobL;
                jobL = handleBatch(inp);
                
                StartJobs(jobL);
        }
#endif // USER_PROGRAM
#ifdef FILESYS
	if (!strcmp(*argv, "-cp")) { 		// copy from UNIX to Nachos
	    ASSERT(argc > 2);
	    Copy(*(argv + 1), *(argv + 2));
	    argCount = 3;
	} else if (!strcmp(*argv, "-p")) {	// print a Nachos file
	    ASSERT(argc > 1);
	    Print(*(argv + 1));
	    argCount = 2;
	} else if (!strcmp(*argv, "-r")) {	// remove Nachos file
	    ASSERT(argc > 1);
	    fileSystem->Remove(*(argv + 1));
	    argCount = 2;
	} else if (!strcmp(*argv, "-l")) {	// list Nachos directory
            fileSystem->List();
	} else if (!strcmp(*argv, "-D")) {	// print entire filesystem
            fileSystem->Print();
	} else if (!strcmp(*argv, "-t")) {	// performance test
            PerformanceTest();
	}
#endif // FILESYS
#ifdef NETWORK
        if (!strcmp(*argv, "-o")) {
	    ASSERT(argc > 1);
            Delay(2); 				// delay for 2 seconds
						// to give the user time to 
						// start up another nachos
            MailTest(atoi(*(argv + 1)));
            argCount = 2;
        }
#endif // NETWORK
    }


    
    currentThread->Finish();	// NOTE: if the procedure "main" 
				// returns, then the program "nachos"
				// will exit (as any other normal program
				// would).  But there may be other
				// threads on the ready list.  We switch
				// to those threads by saying that the
				// "main" thread is finished, preventing
				// it from returning.
    return(0);			// Not reached...
}

List* handleBatch(FILE *jobList)
{
        List *elems = new List();
        
        printf("Starting\n");
        char line[1000];
        fgets(line,1000, jobList);
        int ll=0;
        while(line[ll] != 0) ll++;
        if(ll==3)
            scheduling_algorithm=10;
        else
            scheduling_algorithm=line[0]-'0';
        if(scheduling_algorithm==3||scheduling_algorithm==7){
            schedulingQuantum=testloopAvgBurstLength/4;
        }
        else if(scheduling_algorithm==4||scheduling_algorithm==8){
            schedulingQuantum=testloopAvgBurstLength/2;
        }
        else if(scheduling_algorithm==5||scheduling_algorithm==9){
            schedulingQuantum=3*testloopAvgBurstLength/4;
        }

        // for testing
        // schedulingQuantum = 100;
        TimerTicks = schedulingQuantum;

        DEBUG('l',"Starting reading of batch file.Will be using scheduling_algorithm %d\n",scheduling_algorithm);
        while(fgets(line, 1000, jobList) != NULL)
        {
                
                // Finding length
                int ll = 0;
                while(line[ll] != 0) ll++;
                ll -= 2;
                while(line[ll] == ' ') ll--;
                ll++;
                
                // Has number
                bool hasNum = false;
                for(int i = ll - 1; i >= 0; i--)
                {
                        if(line[i] <= '9' && line[i] >= '0')
                                hasNum = true;
                        else if(line[i] == ' ') break;
                        else
                        {
                                hasNum = false;
                                break;
                        }
                }

                // 
                int priority = 100;
                int stop = ll+1;
                int start = 0;
                if(hasNum)
                {
                        priority = 0;
                        int ten = 1;
                        for(int i = ll - 1; i >= 0; i--)
                        {
                                if(!(line[i] <= '9' && line[i] >= '0')) break;
                                priority = priority + (line[i] - '0')*ten;
                                ten *= 10;
                                stop = i;
                        }
                }

                while(line[start] == ' ') start++;
                /*
                for(int i = start; i < stop; i++)
                        putchar(line[i]);
                putchar('\n');
                printf("%d\n", priority);
                */
                
                char *curr = new char[stop];
                for(int i = 0; i < stop-1; i++)
                        curr[i] = line[i];

                if(scheduling_algorithm >= 7 )
                    elems->SortedInsert(curr, priority+50);
                else
                    elems->SortedInsert(curr, priority);
        }
        /*
        ListElement *elem = elems->getFirst();
        while(elem != NULL)
        {
                printf("%s :: ", elem->item);
                printf("%d\n", elem->key);
                elem = elem->next;
        }
        */
        return elems;
}
