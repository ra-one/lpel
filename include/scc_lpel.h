#ifndef _SCC_LPEL_H
#define _SCC_LPEL_H_

#ifdef USE_SCC
#include "scc.h"
#define malloc SCCMallocPtr
#define valloc SCCMallocPtr
#define free SCCFreePtr
#endif /*USE_SCC*/

#endif /* _SCC_LPEL_H_ */
