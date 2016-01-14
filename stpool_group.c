/*
 *  COPYRIGHT (C) 2014 - 2020, piggy_xrh
 * 
 *	  Stpool is a portable and efficient tasks pool library, it can work on diferent 
 * platforms such as Windows, linux, unix and ARM.  
 *
 *    If you have any troubles or questions on using the library, contact me.
 *
 * 	  (Email: piggy_xrh@163.com  QQ: 1169732280)
 */

#include <stdlib.h>
#include <string.h>

#include "msglog.h"
#include "timer.h"
#include "cpool_factory.h"
#include "stpool_internal.h"
#include "stpool_group.h"

EXPORT void 
stpool_task_set_gid(struct sttask *ptask, int gid)
{
	assert (TASK_CAST_DOWN(ptask)->gid == gid ||
	        !TASK_CAST_DOWN(ptask)->f_stat);
	
	if (TASK_CAST_DOWN(ptask)->pool && 
		(TASK_CAST_DOWN(ptask)->f_stat || TASK_CAST_DOWN(ptask)->ref)) {
		ctask_t *ptask0 = TASK_CAST_DOWN(ptask);
		
		if (ME_ADV_HAS(ptask0->pool, group_create)) { 
			/**
			 * Try synchronize the env
			 */
			if (ME_EX_HAS(ptask0->pool, task_wsync))
				ME_EX_CALL(ptask0->pool, task_wsync)(ptask0->pool->ins, ptask0);
			
			if (ptask0->f_stat || ptask0->ref)
				MSG_log(M_POOL, LOG_WARN,
						"It is unsafe to change the task's gid since it is not free now or\n"
						"its current reference is not zero. tsk(%s/%p) ref(%d) vmflags(%p) stat(%p).\n",
						ptask->task_name, ptask, ptask0->ref, ptask0->f_vmflags, ptask0->f_stat);
		}
	}
	TASK_CAST_DOWN(ptask)->gid = gid;
}

EXPORT int  
stpool_task_gid(struct sttask *ptask)
{
	return TASK_CAST_DOWN(ptask)->gid;
}

EXPORT const char *
stpool_task_gname2(struct sttask *ptask, char *name_buffer, int len)
{
	stpool_t *p = TASK_CAST_DOWN(ptask)->pool;

	if (!p)
		return NULL;

	return stpool_group_name2(p, TASK_CAST_DOWN(ptask)->gid, name_buffer, len);
}

EXPORT int  
stpool_task_gthrottle_wait(struct sttask *ptask, long ms)
{
	stpool_t *pool = TASK_CAST_DOWN(ptask)->pool;

	if (!pool) {
		MSG_log(M_POOL, LOG_WARN,
				"tsk(%s/%p): Firstly, you should call @stpool_task_set_pool to specify its destination\n",
				ptask->task_name, ptask);
		return POOL_TASK_ERR_DESTINATION;
	}

	return stpool_group_throttle_wait(pool, TASK_CAST_DOWN(ptask)->gid, ms);
}

EXPORT int  
stpool_task_pgthrottle_wait(struct sttask *ptask, long ms)
{
	int e;
	uint64_t us_now = 0;
	long us_elapsed;
	stpool_t *pool = TASK_CAST_DOWN(ptask)->pool;
	
	if (!pool) {
		MSG_log(M_POOL, LOG_WARN,
				"tsk(%s/%p): Firstly, you should call @stpool_task_set_pool to specify its destination\n",
				ptask->task_name, ptask);
		return POOL_TASK_ERR_DESTINATION;
	}
	
	if (ms > 0)
		us_now = us_startr();
	
	if (!(e = stpool_throttle_wait(pool, ms))) {
		if (ms > 0) {
			us_elapsed = us_endr(us_now);
			
			if (us_elapsed >= ms * 1000)
				ms = 0;
			else
				ms -= us_elapsed / 1000;
		}
		e = stpool_group_throttle_wait(pool, TASK_CAST_DOWN(ptask)->gid, ms);
	}
	
	return e;
}

EXPORT int  
stpool_group_create(stpool_t *pool, const char *name, struct gscheduler_attr *attr, int pri_q_num, int suspend)
{
	int group_id = -1;

	if (ME_ADV_HAS(pool, group_create)) {
		group_id = ME_ADV_CALL(pool, group_create)(pool->ins, name, pri_q_num, suspend);

		if (-1 != group_id && attr)
			stpool_group_setattr(pool, group_id, attr);
	}

	return group_id;
}

EXPORT int  
stpool_group_id(stpool_t *pool, const char *name)
{
	if (!strcmp(name, stpool_desc(pool)))
		return 0;

	if (!ME_ADV_HAS(pool, group_id)) 
		return -1;
	
	return ME_ADV_CALL(pool, group_id)(pool->ins, name);
}

EXPORT const char *
stpool_group_name2(stpool_t *pool, int gid, char *name_buffer, int len)
{
	if (!gid && !ME_ADV_HAS(pool, group_create)) {
		const char *desc = stpool_desc(pool);

		if (name_buffer) {
			strncat(name_buffer, desc, len);
			return name_buffer;
		}
		return desc;
	}

	if (!ME_ADV_HAS(pool, group_desc)) 
		return NULL;
	
	return ME_ADV_CALL(pool, group_desc)(pool->ins, gid, name_buffer, len);
}

EXPORT struct sttask_group_stat *
stpool_group_stat(stpool_t *pool, int gid, struct sttask_group_stat *stat)
{
	struct ctask_group_stat gstat;

	if (!ME_ADV_HAS(pool, group_create)) {
		struct pool_stat pstat;

		if (gid)
			return NULL;
		stpool_stat(pool, &pstat);
		
		stat->gid = 0;
		stat->desc = (char *)pstat.desc;
		stat->desc_length = strlen(stat->desc);
		stat->attr.limit_paralle_tasks = pstat.maxthreads;
		stat->attr.receive_benifits = 1;
		stat->created = pstat.created;
		stat->waiters = pstat.waiters;
		stat->suspended = pstat.suspended;
		stat->throttle_enabled = pstat.throttle_enabled;
		stat->priq_num = pstat.priq_num;
		stat->npendings = pstat.curtasks_pending;
		stat->nrunnings = pstat.curthreads_active;
		stat->ndispatchings = pstat.curtasks_removing;
		
		return stat;
	}
	
	ME_ADV_CALL(pool, group_stat)(pool->ins, gid, &gstat);

	assert (sizeof(*stat) == sizeof(gstat));
	memcpy(stat, &gstat, sizeof(*stat));

	return stat;
}

EXPORT int  
stpool_group_stat_all(stpool_t *pool, struct sttask_group_stat **stat)
{
	int n = 0;
	struct ctask_group_stat *gstat;

	if (!ME_ADV_HAS(pool, group_create)) {
		*stat = malloc(sizeof(struct sttask_group_stat));
		
		if (*stat) {
			stpool_group_stat(pool, 0, *stat);
			n = 1;
		}

	} else {
		n = ME_ADV_CALL(pool, group_stat_all)(pool->ins, &gstat);
		*stat = (struct sttask_group_stat *)gstat;
	}
	
	return n;
}

EXPORT void 
stpool_group_setattr(stpool_t *pool, int gid, struct gscheduler_attr *attr)
{
	struct scheduler_attr attr0;
	
	if (!gid && !ME_ADV_HAS(pool, group_create)) {
		if (attr->limit_paralle_tasks <= 0)
			attr->limit_paralle_tasks = 1;

		stpool_adjust_abs(pool, attr->limit_paralle_tasks, 0);
		return;
	}

	if (!ME_ADV_HAS(pool, group_setattr))
		return;
	
	MSG_log(M_POOL, LOG_INFO,
		   "{\"%s\"/%p} Configuring the group(%d)'s scheduling attributes(%d) ...\n",
		   pool->desc, pool, gid, attr->limit_paralle_tasks);
		
	memcpy(&attr0, attr, sizeof(*attr));
	ME_ADV_CALL(pool, group_setattr)(pool->ins, gid, &attr0);
}

EXPORT struct gscheduler_attr *
stpool_group_getattr(stpool_t *pool, int gid, struct gscheduler_attr *attr)
{
	struct scheduler_attr attr0;
	
	if (!gid && !ME_ADV_HAS(pool, group_create)) {
		struct pool_stat pstat;

		stpool_stat(pool, &pstat);
		attr->limit_paralle_tasks = pstat.maxthreads;
		attr->receive_benifits = 1;
		return attr;
	}

	if (!ME_ADV_HAS(pool, group_getattr) || ME_ADV_CALL(pool, group_getattr)(pool->ins, gid, &attr0)) 
		return NULL;

	memcpy(attr, &attr0, sizeof(*attr));
	return attr;
}

EXPORT int
stpool_group_suspend(stpool_t *pool, int gid, long ms)
{
	int e;

	MSG_log(M_POOL, LOG_INFO,
		"{\"%s\"/%p} suspend group(%d) ...(%ld ms)\n",
		pool->desc, pool, gid, ms);
	
	if (!gid && !ME_ADV_HAS(pool, group_create))
		return stpool_suspend(pool, ms);

	if (!ME_ADV_HAS(pool, group_suspend))
		return POOL_ERR_NSUPPORT;

	if ((e=ME_ADV_CALL(pool, group_suspend)(pool->ins, gid, ms)))
		return __stpool_liberror(e);
	
	return 0;
}

EXPORT int
stpool_group_suspend_all(stpool_t *pool, long ms)
{
	int e;

	MSG_log(M_POOL, LOG_INFO,
		"{\"%s\"/%p} suspend all groups ...(%d ms)\n",
		pool->desc, pool, ms);
	
	if (!ME_ADV_HAS(pool, group_create))
		return stpool_suspend(pool, ms);

	if (!ME_ADV_HAS(pool, group_suspend_all))
		return POOL_ERR_NSUPPORT;
	
	if ((e=ME_ADV_CALL(pool, group_suspend_all)(pool->ins, ms)))
		return __stpool_liberror(e);
	
	return 0;
}

EXPORT void 
stpool_group_resume(stpool_t *pool, int gid)
{
	MSG_log(M_POOL, LOG_INFO,
		"{\"%s\"/%p} resume group(%d) ...\n",
		pool->desc, pool, gid);
	
	if (!gid && !ME_ADV_HAS(pool, group_create))
		stpool_resume(pool);

	else if (ME_ADV_HAS(pool, group_resume))
		ME_ADV_CALL(pool, group_resume)(pool->ins, gid);
}

EXPORT void 
stpool_group_resume_all(stpool_t *pool)
{
	MSG_log(M_POOL, LOG_INFO,
		"{\"%s\"/%p} resume all groups ...\n",
		pool->desc, pool);
	
	if (!ME_ADV_HAS(pool, group_create))
		stpool_resume(pool);

	else if (ME_ADV_HAS(pool, group_resume_all))
		ME_ADV_CALL(pool, group_resume_all)(pool->ins);
}

EXPORT int  
stpool_group_add_routine(stpool_t *pool, int gid, const char *name,
					void (*task_run)(struct sttask *ptask),
					void (*task_err_handler)(struct sttask *ptask, long reasons),
					void *task_arg, struct schattr *attr)
{	
	int e;
	ctask_t *ptask;
	
	if (!ME_ADV_HAS(pool, group_create)) {
		if (gid)
			return POOL_ERR_GROUP_NOT_FOUND;	
	
		return stpool_add_routine(pool, name, task_run, task_err_handler, task_arg, attr);
	}
	
	/**
	 * We try to get a task object from the cache, and
	 * @__stpool_cache_put(x) is should be called later 
	 * to recycle it
	 */
	ptask = __stpool_cache_get(pool);
	if (!ptask)
		return POOL_ERR_NOMEM;
	
	__stpool_task_INIT(ptask, name, task_run, task_err_handler, task_arg);
	ptask->gid = gid;
	
	if (attr)
		stpool_task_setschattr(TASK_CAST_UP(ptask), attr);
	
	/**
	 * Pay the task object back to cache if we fail 
	 * to add it into the pool's pending queue 
	 */
	if ((e = stpool_task_queue(TASK_CAST_UP(ptask)))) 
		__stpool_cache_put(pool, ptask);

	return e;
}

EXPORT int  
stpool_group_remove_all(stpool_t *pool, int gid, int dispatched_by_pool)
{
	long lflags = dispatched_by_pool ? eTASK_VM_F_REMOVE_BYPOOL : eTASK_VM_F_REMOVE;
	
	if (!gid && !ME_ADV_HAS(pool, group_create))
		return stpool_remove_all(pool, dispatched_by_pool);

	if (ME_ADV_HAS(pool, group_remove_all)) {
		MSG_log(M_POOL, LOG_INFO,
			"{\"%s\"/%p} Marking all group(%d)'s tasks (%d)...\n",
			pool->desc, pool, gid, dispatched_by_pool);

		return ME_ADV_CALL(pool, group_remove_all)(pool->ins, gid, dispatched_by_pool);
	
	} else if (ME_ADV_HAS(pool, group_mark_all))
		
		return ME_ADV_CALL(pool, group_mark_all)(pool->ins, gid, lflags);
	else
		return 0;
}

EXPORT int  
stpool_group_mark_all(stpool_t *pool, int gid, long lflags)
{
	MSG_log(M_POOL, LOG_INFO,
			"{\"%s\"/%p} Marking all group(%d)'s tasks with %p ...\n",
			pool->desc, pool, gid, lflags);
	
	if (!gid && !ME_ADV_HAS(pool, group_create))
		return stpool_mark_all(pool, lflags);

	if (!ME_ADV_HAS(pool, group_mark_all))
		return POOL_ERR_NSUPPORT;
	
	return ME_ADV_CALL(pool, group_mark_all)(pool->ins, gid, lflags);
}

EXPORT int  
stpool_group_mark_cb(stpool_t *pool, int gid, Walk_cb wcb, void *wcb_arg)
{
	if (!gid && !ME_ADV_HAS(pool, group_create))
		return stpool_mark_cb(pool, wcb, wcb_arg);

	if (!ME_ADV_HAS(pool, group_mark_cb))
		return POOL_ERR_NSUPPORT;
	
	return ME_ADV_CALL(pool, group_mark_cb)(pool->ins, gid, (Visit_cb)wcb, wcb_arg);
}

EXPORT int  
stpool_group_wait_all(stpool_t *pool, int gid, long ms)
{
	MSG_log(M_POOL, LOG_INFO,
			"{\"%s\"/%p} Start waiting for all group(%d) tasks's being done ... (%ld ms)\n",
			pool->desc, pool, gid, ms);
	
	if (!gid && !ME_ADV_HAS(pool, group_create))
		return stpool_wait_all(pool, ms);

	if (!ME_ADV_HAS(pool, group_wait_all))
		return POOL_ERR_NSUPPORT;
	
	return ME_ADV_CALL(pool, group_wait_all)(pool->ins, gid, ms);
}

EXPORT int  
stpool_group_wait_cb(stpool_t *pool, int gid, Walk_cb wcb, void *wcb_arg, long ms)
{
	if (!gid && !ME_ADV_HAS(pool, group_create))
		return stpool_wait_cb(pool, wcb, wcb_arg, ms);

	if (!ME_ADV_HAS(pool, group_wait_cb)) 
		return POOL_ERR_NSUPPORT;
	
	return ME_ADV_CALL(pool, group_wait_cb)(pool->ins, gid, (Visit_cb)wcb, wcb_arg, ms);
}

EXPORT int  
stpool_group_wait_any(stpool_t *pool, int gid, long ms)
{
	if (!gid && !ME_ADV_HAS(pool, group_create))
		return stpool_wait_any(pool, ms);

	if (!ME_ADV_HAS(pool, group_wait_any))
		return POOL_ERR_NSUPPORT;
	
	return ME_ADV_CALL(pool, group_wait_any)(pool->ins, gid, ms);
}

EXPORT void
stpool_group_throttle_enable(stpool_t *pool, int gid, int enable)
{
	MSG_log(M_POOL, LOG_INFO,
			"{\"%s\"/%p} %s the group(%d)'s throttle ...\n",
			pool->desc, pool, enable ? "ENABLING" : "DISABLING", gid);
	
	if (!gid && !ME_ADV_HAS(pool, group_create))
		stpool_throttle_enable(pool, enable);

	if (ME_ADV_HAS(pool, group_throttle_enable))
		ME_ADV_CALL(pool, group_throttle_enable)(pool->ins, gid, enable);
}

EXPORT int 
stpool_group_throttle_wait(stpool_t *pool, int gid, long ms)
{
	if (!gid && !ME_ADV_HAS(pool, group_create))
		return stpool_throttle_wait(pool, ms);

	if (!ME_ADV_HAS(pool, group_throttle_wait))
		return 0;
	
	return ME_ADV_CALL(pool, group_throttle_wait)(pool->ins, gid, ms);
}

EXPORT void 
stpool_group_delete(stpool_t *pool, int gid)
{
	if (ME_ADV_HAS(pool, group_delete))
		ME_ADV_CALL(pool, group_delete)(pool->ins, gid);
}
	
