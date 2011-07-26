#ifndef _SCHEDULER_H_
#define _SCHEDULER_H_

#include "lpel_name.h"

struct lpel_task_t;

#define SCHED_NUM_PRIO  2

typedef struct schedctx schedctx_t;

typedef struct {
  int prio;
} sched_task_t;


schedctx_t *LPEL_FUNC(SchedCreate)( int wid);
void LPEL_FUNC(SchedDestroy)( schedctx_t *sc);

void LPEL_FUNC(SchedMakeReady)( schedctx_t* sc, struct lpel_task_t *t);
struct lpel_task_t *LPEL_FUNC(SchedFetchReady)( schedctx_t *sc);



#endif /* _SCHEDULER_H_ */
