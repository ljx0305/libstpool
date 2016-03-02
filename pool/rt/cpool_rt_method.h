#ifndef __CPOOL_RT_METHOD_H__
#define __CPOOL_RT_METHOD_H__

/* 
 *	  Stpool is a portable and efficient tasks pool library, it can work on diferent 
 * platforms such as Windows, linux, unix and ARM.  
 *
 *    If you have any troubles or questions on using the library, contact me.
 *
 * 	  (Email: piggy_xrh@163.com  QQ: 1169732280)
 */

#include "cpool_method.h"
#include "cpool_rt_struct.h"

/** Method sets for the Core */
int  cpool_rt_core_ctor(cpool_core_t *core);
void cpool_rt_core_notifyl(cpool_core_t *core, eEvent_t event);
void cpool_rt_core_dtor(cpool_core_t *core);
long cpool_rt_core_err_reasons(basic_task_t *ptask);
int  cpool_rt_core_gettask(cpool_core_t *core, thread_t *self);
void cpool_rt_core_finished(cpool_core_t *core, thread_t *self, basic_task_t *ptask, long eReasons);
int  cpool_rt_core_pri_gettask(cpool_core_t *core, thread_t *self);
void cpool_rt_core_pri_finished(cpool_core_t *core, thread_t *self, basic_task_t *ptask, long eReasons);
int  cpool_rt_core_dynamic_gettask(cpool_core_t *core, thread_t *self);
void cpool_rt_core_dynamic_finished(cpool_core_t *core, thread_t *self, basic_task_t *ptask, long eReasons);
int  cpool_rt_core_dynamic_pri_gettask(cpool_core_t *core, thread_t *self);
void cpool_rt_core_dynamic_pri_finished(cpool_core_t *core, thread_t *self, basic_task_t *ptask, long eReasons);

/** Method sets for the BASIC interfaces */
int   cpool_rt_remove_all(cpool_core_t *core, int dispatched_by_pool);
int   cpool_rt_suspend(cpool_core_t *core, long ms);
int   cpool_rt_mark_all(cpool_core_t *core, long lflags);
int   cpool_rt_mark_cb(cpool_core_t *core, Visit_cb cb, void *cb_arg);
int   cpool_rt_wait_all(cpool_core_t *core, long ms);

struct cpool_stat *cpool_rt_stat(cpool_core_t *core, struct cpool_stat *stat);
char *cpool_rt_scheduler_map_dump(cpool_core_t *core, char *buff, size_t bufflen);

int   cpool_rt_task_init(cpool_core_t *core, ctask_t *ptask);
int   cpool_rt_task_queue(cpool_core_t *core, ctask_t *ptask);
int   cpool_rt_task_remove(cpool_core_t *core, ctask_t *ptask, int dispatched_by_pool);
void  cpool_rt_task_mark(cpool_core_t *core, ctask_t *ptask, long lflags);
long  cpool_rt_task_stat(cpool_core_t *core, ctask_t *ptask, long *vm);

/** Method sets for the EXTENTED interfaces */
void  cpool_rt_throttle_ctl(cpool_core_t *core, int on);
int   cpool_rt_throttle_wait(cpool_core_t *core, long ms);

/** Create a routine pool instance */
int  cpool_rt_create_instance(cpool_rt_t *rtp, const char *desc, int max, int min, int pri_q_num, int suspend, long lflags);

#endif
