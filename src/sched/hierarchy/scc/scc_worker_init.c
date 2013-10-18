/*
 * scc_worker_init.c
 *
 *  Created on: 17 Jul 2013
 *      Author: administrator
 */

/*
 * The LPEL worker containing code for workers, master and wrappers
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <errno.h>
#include <float.h>

#include <pthread.h>
#include "arch/mctx.h"

#include "arch/atomic.h"

#include "../hrc_worker.h"
#include "../hrc_task.h"
#include "lpel_hwloc.h"
#include "lpelcfg.h"
#include "../hrc_stream.h"
#include "mailbox.h"
#include "lpel/monitor.h"
#include "lpel_main.h"
#include "scc.h"

void LpelStartMeasurement(void);
//#define _USE_WORKER_DBG__

#ifdef _USE_WORKER_DBG__
#define WORKER_DBG printf
#else
#define WORKER_DBG	//
#endif

static void cleanupMasterMb();

static int num_workers = -1;
static int num_wrappers = -1;
static masterctx_t *master;
static workerctx_t *worker;

/**
 * Initialise worker globally
 *
 *
 * @param size    size of the worker set, i.e., the total number of workers including master
 */
void LpelWorkersInit(int size) {

	int i,rank;
	assert(0 <= size);
	 
	/* local variables used in worker operations */
  num_workers = size - 1; //minus the master
  num_wrappers = SCCGetNumWrappers();
	initLocalVar(num_workers,num_wrappers);
  
  /*ini mailbox*/
  mailbox_t *mbox =  LpelMailboxCreate();
  
  if (SCCIsMaster()) {
    /** create master */
    master = (masterctx_t *) malloc(sizeof(masterctx_t));
    master->mailbox = mbox;
    master->ready_tasks = LpelTaskqueueInit ();
    master->ready_wrappers = LpelTaskqueueInit();
    master->num_workers = num_workers;
    master->num_wrappers = num_wrappers;
    /*master do not hold context for workers*/
    master->workers = NULL;
    /* allocate waiting table */
    master->waitworkers = (int *) malloc(num_workers * sizeof(int));
    for (i=0; i<num_workers; i++) {
      master->waitworkers[i] = 0;
    }
    master->waitwrappers = (int *) malloc(num_wrappers * sizeof(int));
    for (i=0; i<num_wrappers; i++) {
      master->waitwrappers[i] = 0;
    }
  } else{
    /*create single worker per core*/
    worker=(workerctx_t *) malloc(sizeof(workerctx_t));
    rank = SCCGetNodeRank();
    if ( rank > num_workers){
      worker->wid=(rank-(rank+rank))+1; //convert rank to negative 
    } else {
      worker->wid=rank-1;
    }
#ifdef USE_LOGGING
    if (MON_CB(worker_create)) {
      worker->mon = MON_CB(worker_create)(worker->wid);
    } else {
      worker->mon = NULL;
    }
#else
    worker->mon = NULL;
#endif
    /* mailbox */
    worker->mailbox = mbox;
    worker->free_sd = NULL;
    worker->free_stream = NULL;
    WORKER_DBG("workerInit: node physical location %d, rank %d, wid %d\n",SCCGetNodeID(), SCCGetNodeRank(),worker->wid);
  }
}


void setupMailbox(mailbox_t **mastermb, mailbox_t **workermbs) {
   int i;
   *mastermb = allMbox[0];
   WORKER_DBG("\nmastermb %p\n",*mastermb);
   for(i=0;i<(num_workers+num_wrappers);i++){
    workermbs[i] = allMbox[i+1];
    WORKER_DBG("workermbs[%d] %p\n",i,workermbs[i]);
   }
}

/*
 * clean up for both worker and master
 */
void LpelWorkersCleanup(void) {
	int i;

        if (SCCIsMaster()) {
          /* wait for the master to finish */
          (void) pthread_join(master->thread, NULL);
          /* clean up master's mailbox */
          cleanupMasterMb();
          LpelMailboxDestroy(master->mailbox);
          LpelTaskqueueDestroy(master->ready_tasks);
          LpelTaskqueueDestroy(master->ready_wrappers);

          /* free workers tables */
          free(master->waitworkers);
          free(master->waitwrappers);
          free(master);
          WORKER_DBG("CLEAN; master finished");
        } else {
          /* wait for the worker to finish */
          (void) pthread_join(worker->thread, NULL);
          LpelMailboxDestroy(worker->mailbox);
          LpelWorkerDestroyStream(worker);
          LpelWorkerDestroySd(worker);
          free(worker);
          WORKER_DBG("CLEAN; worker finished");
        }
        /* clean up local vars used in worker operations */
        cleanupLocalVar();
}


/*
 * Spawn master and workers
 */
void LpelWorkersSpawn(void) {
	if (SCCIsMaster()) {
    /* master spawn joinable thread*/
    (void) pthread_create(&master->thread, NULL, MasterThread, master);
    //LpelStartMeasurement();
  } else if(SCCGetNodeRank() > num_workers) { // +1 for master
    /* wrappers */
    (void) pthread_create(&worker->thread, NULL, WrapperThread, worker);
	} else {
    /* workers */
    (void) pthread_create(&worker->thread, NULL, WorkerThread, worker);
	}
}

void *Measurement(void *arg){
  int i=0,j=0;
  do{
     if(i++ > 50000) { j++; i=0; printf("%d\n",j); }
  }while(!MESSTOP);
  printf("i: %d, j: %d\n",i,j);
  return NULL;
}

void LpelStartMeasurement(void){
  lpel_task_t *measurementtask;
  measurementtask = LpelTaskCreate( -1, Measurement, NULL, 8192);
  //int *id = (int*) measurementtask + 2; // set task id
  //*id = 99999;
  LpelTaskStart(measurementtask);
}
  
/*
 * Terminate master and workers
 */
void LpelWorkersTerminate1(void) {
	workermsg_t msg;
	msg.type = WORKER_MSG_TERMINATE;
	LpelMailboxSend(master->mailbox, &msg);
}

/************************ Private functions ***********************************/
/*
 * clean up master's mailbox before terminating master
 * last messages including: task request from worker, and return zombie task
 */
static void cleanupMasterMb() {
	workermsg_t msg;
	lpel_task_t *t;
	while (LpelMailboxHasIncoming(master->mailbox)) {
		LpelMailboxRecv(master->mailbox, &msg);
		switch(msg.type) {
		case WORKER_MSG_REQUEST:
			break;
		case WORKER_MSG_RETURN:
			t = msg.body.task;
			WORKER_DBG("master: get returned task %d\n", t->uid);
	    assert(t->state == TASK_ZOMBIE);
			LpelTaskDestroy(t);
			break;
		default:
			assert(0);
			break;
		}
	}
}
