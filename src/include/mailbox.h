#ifndef _MAILBOX_H_
#define _MAILBOX_H_

#include "workermsg.h"

typedef struct mailbox_t mailbox_t;

mailbox_t *LpelMailboxCreate(void);
mailbox_t *LpelMailboxCreateW(void);
void LpelMailboxDestroy(mailbox_t *mbox);
void LpelMailboxSend(mailbox_t *mbox, workermsg_t *msg);
void LpelMailboxRecv(mailbox_t *mbox, workermsg_t *msg);
int  LpelMailboxHasIncoming(mailbox_t *mbox);

#ifdef USE_SCC
mailbox_t **allmbox;
void LpelMailboxInit(int node_id_num, int num_worker);
#endif /*USE_SCC*/

#endif /* _MAILBOX_H_ */
