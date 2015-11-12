#ifndef __CPOOL_GP_METHOD_H__
#define __CPOOL_GP_METHOD_H__

/* 
 *	  Stpool is a portable and efficient tasks pool library, it can work on diferent 
 * platforms such as Windows, linux, unix and ARM.  
 *
 *    If you have any troubles or questions on using the library, contact me.
 *
 * 	  (Email: piggy_xrh@163.com  QQ: 1169732280)
 */

#include "cpool_method.h"
#include "cpool_core_struct.h"
#include "cpool_gp_struct.h"

/** Method sets for the Core */
int  cpool_gp_core_ctor(cpool_core_t *core);
void cpool_gp_core_notifyl(cpool_core_t *core, eEvent_t event);
void cpool_gp_core_dtor(cpool_core_t *core);
int  cpool_gp_core_gettask(cpool_core_t *core, thread_t *self);
void cpool_gp_core_finished(cpool_core_t *core, thread_t *self, basic_task_t *ptask, long eReasons);
long cpool_gp_core_err_reasons(basic_task_t *ptask);

/** Method sets for the BASIC interfaces */
int   cpool_gp_suspend(cpool_com_t *core, long ms);
int   cpool_gp_remove_all(cpool_core_t *core, int dispatched_by_pool);
int   cpool_gp_mark_all(cpool_core_t *core, long lflags);
int   cpool_gp_wait_all(cpool_core_t *core, long ms);
int   cpool_gp_wait_cb(cpool_core_t *core, Visit_cb cb, void *cb_arg, long ms);
int   cpool_gp_mark_cb(cpool_core_t *core, Visit_cb cb, void *cb_arg);

struct cpool_stat *cpool_gp_stat(cpool_core_t *core, struct cpool_stat *stat);
char * cpool_gp_scheduler_map_dump(cpool_core_t *core, char *buff, size_t bufflen);

int   cpool_gp_task_queue(cpool_core_t *core, ctask_t *ptask);
int   cpool_gp_task_remove(cpool_core_t *core, ctask_t *ptask, int dispatched_by_pool);
void  cpool_gp_task_detach(cpool_core_t *core, ctask_t *ptask);
void  cpool_gp_task_mark(cpool_core_t *core, ctask_t *ptask, long lflags);
long  cpool_gp_task_stat(cpool_core_t *core, ctask_t *ptask, long *vm);

/** Method sets for the EXTENTED interfaces */
void  cpool_gp_throttle_ctl(cpool_core_t *core, int on);
int   cpool_gp_throttle_wait(cpool_core_t *core, long ms);
int   cpool_gp_wait_any(cpool_core_t *core, long ms);
int   cpool_gp_task_wsync(cpool_core_t *core, ctask_t *ptask);
int   cpool_gp_task_wait(cpool_core_t *core, ctask_t *ptask, long ms);
int   cpool_gp_task_wait_any(cpool_core_t *core, ctask_t *entry[], int n, long ms);

/** Method sets for the ADVACNED interfaces */
int   cpool_gp_entry_create(cpool_com_t *ins, const char *desc, int pri_q_num, int suspend);
void  cpool_gp_entry_delete(cpool_com_t *ins, int gid);
int   cpool_gp_entry_id(cpool_com_t *ins, const char *desc);
char *cpool_gp_entry_desc(cpool_com_t *ins, int gid, char *desc_buff, size_t len);
int   cpool_gp_entry_stat(cpool_com_t *core, int gid, struct ctask_group_stat *gstat);
int   cpool_gp_entry_stat_all(cpool_com_t *core, struct ctask_group_stat **gstat);
int   cpool_gp_entry_suspend(cpool_com_t *ins, int gid, long ms);
int   cpool_gp_entry_suspend_all(cpool_com_t *ins, long ms);
void  cpool_gp_entry_resume(cpool_com_t *ins, int gid);
void  cpool_gp_entry_resume_all(cpool_com_t *ins);
int   cpool_gp_entry_setattr(cpool_com_t *ins, int gid, struct scheduler_attr *attr);
int   cpool_gp_entry_getattr(cpool_com_t *ins, int gid, struct scheduler_attr *attr);
void  cpool_gp_entry_throttle_ctl(cpool_com_t *ins, int gid, int enable);
int   cpool_gp_entry_throttle_wait(cpool_com_t *ins, int gid, long ms);
int   cpool_gp_entry_remove_all(cpool_com_t *ins, int gid, int dispatched_by_pool);
int   cpool_gp_entry_mark_all(cpool_com_t *ins, int gid, long lflags);
int   cpool_gp_entry_mark_cb(cpool_com_t *ins, int gid, Visit_cb wcb, void *wcb_arg);
int   cpool_gp_entry_wait_all(cpool_com_t *ins, int gid, long ms);
int   cpool_gp_entry_wait_cb(cpool_com_t *ins, int gid, Visit_cb wcb, void *wcb_arg, long ms);
int   cpool_gp_entry_wait_any(cpool_com_t *ins, int gid, long ms);

/** Create a routine pool instance */
int  cpool_gp_create_instance(cpool_gp_t *gpool, const char *desc, int max, int min, int pri_q_num, int suspend, long lflags);

#endif

