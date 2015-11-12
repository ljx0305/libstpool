#ifndef __CPOOL_COMMON_METHOD_H__
#define __CPOOL_COMMON_METHOD_H__

/* 
 *	  Stpool is a portable and efficient tasks pool library, it can work on diferent 
 * platforms such as Windows, linux, unix and ARM.  
 *
 *    If you have any troubles or questions on using the library, contact me.
 *
 * 	  (Email: piggy_xrh@163.com  QQ: 1169732280)
 */

#include "cpool_method.h"

/** Basic Method */
void  cpool_com_atexit(cpool_com_t *ins, void (*__atexit)(cpool_com_t *ins, void *), void *opaque);
void  cpool_com_adjust(cpool_com_t *ins, int max, int min);
void  cpool_com_adjust_abs(cpool_com_t *ins, int max, int min);
int   cpool_com_flush(cpool_com_t *ins);
void  cpool_com_resume(cpool_com_t *ins);
long  cpool_com_addref(cpool_com_t *ins);
long  cpool_com_release(cpool_com_t *ins);
void  cpool_com_setattr(cpool_com_t *ins, struct thread_attr *attr);
void  cpool_com_getattr(cpool_com_t *ins, struct thread_attr *attr);
void  cpool_com_set_activetimeo(cpool_com_t *ins, long acttimeo, long randtimeo);

ctask_t *cpool_com_cache_get(cpool_com_t *ins);
void  cpool_com_cache_put(cpool_com_t *ins, ctask_t *ptask);

#endif
