#ifndef __CPOOL_METHOD_H__
#define __CPOOL_METHOD_H__

/* 
 *	  Stpool is a portable and efficient tasks pool library, it can work on diferent 
 * platforms such as Windows, linux, unix and ARM.  
 *
 *    If you have any troubles or questions on using the library, contact me.
 *
 * 	  (Email: piggy_xrh@163.com  QQ: 1169732280)
 */

#include <stdlib.h>

/** task */
struct ctask;
typedef struct ctask ctask_t;

/** group */
struct ctask_group_stat;

/** pool */
struct cpool;
typedef struct cpool cpool_t;
typedef struct cpool_core cpool_com_t;

struct scheduler_attr;
struct thread_attr;

/** walk callback */
typedef long (*Visit_cb)(ctask_t *ptask, void *opaque);

/** Basic pool method sets */
typedef struct cpool_basic_method {
	/**
	 * pool interfaces 
	 */
	void  (*atexit)(cpool_com_t *ins, void (*__atexit)(cpool_com_t *, void *), void *opaque);
	long  (*addref)(cpool_com_t *ins);
	long  (*release)(cpool_com_t *ins);
	void  (*setattr)(cpool_com_t *ins, struct thread_attr *attr);
	void  (*getattr)(cpool_com_t *ins, struct thread_attr *attr);
	void  (*set_activetimeo)(cpool_com_t *ins, long acttimeo, long randtimeo);
	void  (*adjust)(cpool_com_t *ins, int max, int min);
	void  (*adjust_abs)(cpool_com_t *ins, int max, int min);
	int   (*flush)(cpool_com_t *ins);
	int   (*suspend)(cpool_com_t *ins, long ms);
	void  (*resume)(cpool_com_t *ins);
	int   (*remove_all)(cpool_com_t *ins, int dispatched_by_pool);
	int   (*mark_all)(cpool_com_t *ins, long lflags);
	int   (*mark_cb)(cpool_com_t *ins, Visit_cb wcb, void *wcb_arg);
	int   (*wait_all)(cpool_com_t *ins, long ms);
	
	/**
	 * Status
	 */
	struct cpool_stat *(*stat)(cpool_com_t *ins, struct cpool_stat *stat);
	char *(*scheduler_map_dump)(cpool_com_t *ins, char *buff, size_t bufflen);

	/**
	 * Task cache 
	 */
	ctask_t *(*cache_get)(cpool_com_t *ins);
	void (*cache_put)(cpool_com_t *ins, ctask_t *ptask);

	/**
	 * Task interfaces 
	 */
	const size_t task_size;
	int   (*task_init)(cpool_com_t *ins, ctask_t *ptask);
	void  (*task_deinit)(cpool_com_t *ins, ctask_t *ptask);
	int   (*task_queue)(cpool_com_t *ins, ctask_t *ptask);
	int   (*task_remove)(cpool_com_t *ins, ctask_t *ptask, int dispatched_by_pool);
	void  (*task_mark)(cpool_com_t *ins, ctask_t *ptask, long lflags);
	void  (*task_detach)(cpool_com_t *ins, ctask_t *ptask);
	long  (*task_stat)(cpool_com_t *ins, ctask_t *ptask, long *vm);

} cpool_basic_method_t;

/** Extended pool method sets */
typedef struct cpool_extend_method {
	void  (*throttle_enable)(cpool_com_t *ins, int enable);
	int   (*throttle_wait)(cpool_com_t *ins, long ms);	
	int   (*wait_any)(cpool_com_t *ins, long ms);
	int   (*wait_cb)(cpool_com_t *ins, Visit_cb wcb, void *wcb_arg, long ms);
	
	int   (*wait_any2)(cpool_com_t *ins, ctask_t *entry[], int n, long ms);
	int   (*task_wsync)(cpool_com_t *ins, ctask_t *ptask);
	int   (*task_wait)(cpool_com_t *ins, ctask_t *ptask, long ms);
} cpool_extend_method_t;

/** Advance pool method sets */
typedef struct cpool_advance_method {
	int   (*group_create)(cpool_com_t *ins, const char *desc, int pri_q_num, int suspend);
	void  (*group_delete)(cpool_com_t *ins, int gid);
	int   (*group_id)(cpool_com_t *ins, const char *desc);
	char *(*group_desc)(cpool_com_t *ins, int gid, char *desc_buff, size_t len);
	int   (*group_stat)(cpool_com_t *ins, int gid, struct ctask_group_stat *gstat);
	int   (*group_stat_all)(cpool_com_t *ins, struct ctask_group_stat **gstat);
	int   (*group_suspend)(cpool_com_t *ins, int gid, long ms);
	int   (*group_suspend_all)(cpool_com_t *ins, long ms);
	void  (*group_resume)(cpool_com_t *ins, int gid);
	void  (*group_resume_all)(cpool_com_t *ins);
	int   (*group_setattr)(cpool_com_t *ins, int gid, struct scheduler_attr *attr);
	int   (*group_getattr)(cpool_com_t *ins, int gid, struct scheduler_attr *attr);
	void  (*group_throttle_enable)(cpool_com_t *ins, int gid, int enable);
	int   (*group_throttle_wait)(cpool_com_t *ins, int gid, long ms);
	int   (*group_remove_all)(cpool_com_t *ins, int gid, int dispatched_by_pool);
	int   (*group_mark_all)(cpool_com_t *ins, int gid, long lflags);
	int   (*group_mark_cb)(cpool_com_t *ins, int gid, Visit_cb wcb, void *wcb_arg);
	int   (*group_wait_all)(cpool_com_t *ins, int gid, long ms);
	int   (*group_wait_cb)(cpool_com_t *ins, int gid, Visit_cb wcb, void *wcb_arg, long ms);
	int   (*group_wait_any)(cpool_com_t *ins, int gid, long ms);
} cpool_advance_method_t;

#endif
