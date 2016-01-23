/* 
 *	  Stpool is a portable and efficient tasks pool library, it can work on diferent 
 * platforms such as Windows, linux, unix and ARM.  
 *
 *    If you have any troubles or questions on using the library, contact me.
 *
 * 	  (Email: piggy_xrh@163.com  QQ: 1169732280)
 */

#include "ospx.h"
#include "ospx_errno.h"
#include "msglog.h"
#include "cpool_factory.h"
#include "cpool_core.h"
#include "cpool_gp_method.h"
#include "cpool_gp_internal.h"
#include "cpool_gp_wait_internal.h"

int  
cpool_gp_create_instance(cpool_gp_t *gpool, const char *desc, int max, int min, int pri_q_num, int suspend, long lflags)
{
	static cpool_core_method_t __me = {
		"Group",
		sizeof(ctask_trace_t),
		cpool_gp_core_ctor,
		cpool_gp_core_notifyl,
		cpool_gp_core_gettask,
		cpool_gp_core_err_reasons,
		cpool_gp_core_finished,
		cpool_gp_core_dtor
	};
	
	long  core_flags = CORE_F_dynamic;
	const cpool_core_method_t * me = &__me;
	/**
	 * The pool must support the DYNAMIC attributes
	 */
	if (!(eFUNC_F_DYNAMIC_THREADS & lflags)) {
		MSG_log(M_GROUP, LOG_ERR,
				"Unsupported parameters. lflags(%p)\n",
				lflags);

		return eERR_NSUPPORT;
	}

	/**
	 * Find the propriate methods for the Core
	 */
	if (!me) {
		MSG_log(M_GROUP, LOG_ERR,
				"Can find the pripriate methods to support the request(%ld).\n",
				lflags);
		return -1;
	}

	/**
	 * Save the parameters 
	 */
	gpool->lflags = lflags;
	gpool->priq_num = pri_q_num;
	gpool->core->priv = gpool;

	/**
	 * Start creating the core 
	 */
	return cpool_core_ctor(gpool->core, desc, me, max, min, suspend, core_flags);
}

int   
cpool_gp_task_queue(cpool_core_t *core, ctask_t *ptask)
{
	int e = 0, gid = ptask->gid;
	cpool_gp_t *gpool = core->priv;
	ctask_entry_t *entry;
	
	//if (!memcmp(ptask->task_desc, "Js", 2))
	//	printf("queue msgcallback:vm(%p)-stat(%p) gid(%d)\n", ptask->f_vmflags, ptask->f_stat, gid);

	/**
	 * Remove the REMOVE flags
	 */
	ptask->f_vmflags &= ~eTASK_VM_F_REMOVE_FLAGS;
		
	/**
	 * Check the pool status
	 */
	if (core->status & CORE_F_destroying)
		return eERR_DESTROYING;
	
	if (gid < 0 || gid > gpool->num)
		return eERR_GROUP_NOT_FOUND;
	
	OSPX_pthread_mutex_lock(&core->mut);
	/**
	 * Has the task been marked Re-scheduled ? (FIX a BUG: stpool_task_queue >= 2)
	 */
	if (ptask->f_stat & (eTASK_STAT_F_WAITING|eTASK_STAT_F_WPENDING)) {
		assert ((ptask->f_stat & eTASK_STAT_F_WAITING) || 
				(ptask->f_stat & (eTASK_STAT_F_SCHEDULING|eTASK_STAT_F_DISPATCHING)));
		OSPX_pthread_mutex_unlock(&core->mut);
		return 0;
	}
	/**
	 * Check the vmflags
	 */
	if (ptask->f_vmflags & eTASK_VM_F_DISABLE_QUEUE) {
		OSPX_pthread_mutex_unlock(&core->mut);
		return eTASK_ERR_DISABLE_QUEUE;
	}
	
	/**
	 * Check the global throttle
	 */
	if (gpool->throttle_on) {
		OSPX_pthread_mutex_unlock(&core->mut);
		
		return eERR_THROTTLE;
	}
	entry = gpool->entry + gid;
	/**
	 * Process the task's scheduling priority
	 */
	__cpool_com_task_nice_preprocess(&entry->c, ptask);

	/**
	 * Check the entry status 
	 */
	if (unlikely(entry->lflags & (SLOT_F_FREE|SLOT_F_DESTROYING|SLOT_F_THROTTLE))) {
		e = entry->lflags;
		OSPX_pthread_mutex_unlock(&core->mut);
		
		if ((SLOT_F_FREE|SLOT_F_DESTROYING) & e)
			return eERR_GROUP_NOT_FOUND;
		return eERR_GROUP_THROTTLE;
	}
			
	if (likely(!ptask->f_stat)) {
		++ gpool->n_qtraces;
		++ entry->n_qtraces;
		list_add_tail(&TASK_CAST_TRACE(ptask)->trace_link, entry->trace_q);
		
		__cpool_gp_task_pri_queuel(gpool, entry, TASK_CAST_TRACE(ptask));
		if (likely(!entry->paused))
			__cpool_gp_entry_consumer_notifyl(gpool, entry);
	} else {
		assert (ptask->f_stat & (eTASK_STAT_F_DISPATCHING|eTASK_STAT_F_SCHEDULING));
		ptask->f_stat |= eTASK_STAT_F_WPENDING;
	}
	OSPX_pthread_mutex_unlock(&core->mut);
	
	return 0;
}

int   
cpool_gp_task_remove(cpool_core_t *core, ctask_t *ptask, int dispatched_by_pool)
{
	int ok = 0; 
	long rmflags = eTASK_VM_F_REMOVE;
	cpool_gp_t *gpool = core->priv;
	LIST_HEAD(rmq);
	struct list_head *q = &rmq;

	assert (ptask->pool && ptask->pool->ins == core);
	
	if (!ptask->f_stat)
		return 0;
	
	if (ptask->f_stat & (eTASK_STAT_F_DISPATCHING|eTASK_STAT_F_SCHEDULING))
		return 1;
	
	assert (ptask->gid >= 0 && ptask->gid < gpool->num);
	
	if (dispatched_by_pool) {
		if (ptask->task_err_handler)
			q = NULL;
		rmflags = eTASK_VM_F_REMOVE_BYPOOL;
	}

	OSPX_pthread_mutex_lock(&core->mut);
	if (ptask->f_stat & eTASK_STAT_F_WPENDING)
		ptask->f_stat &= ~eTASK_STAT_F_WPENDING;
	
	else if (ptask->f_stat & eTASK_STAT_F_WAITING) {
		__cpool_gp_task_removel(gpool, gpool->entry + ptask->gid, TASK_CAST_TRACE(ptask), q); 
		
		if (!q && cpool_core_need_ensure_servicesl(core))
			cpool_core_ensure_servicesl(core, NULL);
		ptask->f_vmflags |= rmflags;
		ok = 1;
	}
	OSPX_pthread_mutex_unlock(&core->mut);
	
	if (!list_empty(&rmq))
		__cpool_gp_task_dispatcher(gpool, &rmq);

	return ok;
}

void  
cpool_gp_task_detach(cpool_core_t *core, ctask_t *ptask)
{
	ctask_trace_t *ptask0 = TASK_CAST_TRACE(ptask);
	thread_t *self = ptask0->thread;
	cpool_gp_t *gpool = core->priv;
	ctask_entry_t *entry; 

	assert (ptask0 && gpool && ptask0->task_run); 
	assert (ptask0->f_stat & (eTASK_STAT_F_DISPATCHING|eTASK_STAT_F_SCHEDULING));
	
	/**
	 * It is not neccessary to call @detach_task for the routine tasks 
	 */
	if (ptask0->f_vmflags & eTASK_VM_F_CACHE) {
		MSG_log(M_GROUP, LOG_WARN,
			"Skip deatching a routine task. (%s/%p/%s)\n",
			ptask->task_desc, ptask, core->desc);
		return;
	}

	__cpool_com_task_nice_adjust(ptask);
	OSPX_pthread_mutex_lock(&core->mut);	
	/**
	 * Pass the detached status to the external module 
	 */
	if (ptask0->pdetached)
		*ptask0->pdetached = 1;
	entry = gpool->entry + ptask0->gid;
		
	/**
	 * Remove the trace record and reset the status of the task 
	 */
	list_del(&ptask0->trace_link);
	-- gpool->n_qtraces;
	-- entry->n_qtraces;
	++ entry->ntasks_processed;
		
	/**
	 * Wake up the waiters who is waiting on the task
	 */
	__cpool_gp_w_wakeup_taskl(gpool, entry, ptask0);
	
	/**
	 * Free the temple task object if it is useless 
	 */
	if (likely((ptask0->f_vmflags & eTASK_VM_F_CACHE) && !ptask0->ref)) 
		smcache_addl_dir(gpool->core->cache_task, ptask0);
	else 
		ptask0->f_stat = 0;
	
	/**
	 * We decrease the @ndispatchings if the task has been 
	 * requested to be removed from the pending queue 
	 */
	if (likely(self)) {
		if (likely(TASK_TYPE_NORMAL == self->task_type)) {
			-- entry->nrunnings;
	
			/**
			 * If the task is scheduled by our core, we sholud try to update 
			 * the core's effective tasks number 
			 */
			if (unlikely(!entry->receive_benifits) && !entry->paused && 
				(entry->npendings_eff + entry->nrunnings) < entry->limit_tasks &&
				entry->npendings_eff < entry->npendings) {
				++ core->npendings;
				++ entry->npendings_eff;
			}
		} else {
			-- entry->ndispatchings;
			-- gpool->ndispatchings;
		}

		if (list_empty(&self->dispatch_q))
			cpool_core_thread_status_changel(core, self, THREAD_STAT_COMPLETE);		
	
	} else {
		assert (gpool->ndispatchings > 0 && entry->ndispatchings > 0);
		-- gpool->ndispatchings;
		-- entry->ndispatchings;
	}
	assert (entry->ndispatchings >= 0 && entry->nrunnings >= 0);

	assert (core->npendings >= 0 && core->n_qdispatchs >= 0 &&
			entry->n_qtraces >= 0 && gpool->n_qtraces >= entry->n_qtraces &&
			gpool->n_qtraces >= gpool->ndispatchings + core->n_qdispatchs + gpool->npendings);
		
	/**
	 * Check the task's reference 
	 */
	if (ptask0->ref) {
#ifndef NDEBUG
		MSG_log(M_GROUP, LOG_DEBUG,
			"Waiters on %s/%p  is %d.\n",
			ptask0->task_desc, ptask0, ptask0->ref);
#endif
		/**
		 * We make sure that the WAIT functions work well 
		 */
		ptask0->f_vmflags |= eTASK_VM_F_DETACHED;
		for (;ptask0->ref;) 
			OSPX_pthread_cond_wait(gpool->entry[ptask0->gid].cond_sync, &core->mut);
		ptask0->f_vmflags &= ~eTASK_VM_F_DETACHED;
	}	
	OSPX_pthread_mutex_unlock(&core->mut);
	
	ptask0->thread = NULL;
}

void
cpool_gp_task_mark(cpool_core_t *core, ctask_t *ptask, long lflags)
{
	cpool_gp_t *gpool = core->priv;

	lflags &= eTASK_VM_F_USER_FLAGS;
	
	if (!lflags) 
		return;
	
	if (lflags == (lflags & eTASK_VM_F_REMOVE_FLAGS))
		cpool_gp_task_remove(core, ptask, lflags & eTASK_VM_F_REMOVE_BYPOOL);
	
	else {
		long rmflags = lflags & eTASK_VM_F_REMOVE_FLAGS;
		LIST_HEAD(rmq);
		struct list_head *q = (lflags & eTASK_VM_F_REMOVE_BYPOOL && ptask->task_err_handler) ? NULL : &rmq;

		OSPX_pthread_mutex_lock(&core->mut);
		if (rmflags & eTASK_VM_F_REMOVE_FLAGS) {
			if (ptask->f_stat & eTASK_STAT_F_WPENDING)
				ptask->f_stat &= ~eTASK_STAT_F_WPENDING;

			else if (ptask->f_stat & eTASK_STAT_F_WAITING) {
				__cpool_gp_task_removel(gpool, gpool->entry + ptask->gid, TASK_CAST_TRACE(ptask), q); 
				
				ptask->f_vmflags |= rmflags;
				if (!q && cpool_core_need_ensure_servicesl(core))
					cpool_core_ensure_servicesl(core, NULL);
			}
		}
		__cpool_com_task_mark(ptask, lflags);
		OSPX_pthread_mutex_unlock(&core->mut);

		if (!list_empty(&rmq))
			__cpool_gp_task_dispatcher(core->priv, &rmq);
	}
}

long  
cpool_gp_task_stat(cpool_core_t *core, ctask_t *ptask, long *vm)
{
	long f_stat = 0, f_vm = 0, entry_stat = 0;
	cpool_gp_t *gpool = core->priv;
	
	assert (ptask->pool && ptask->pool->ins == core);

	/**
	 * Check the group id and the task's status
	 */
	if (ptask->gid >= 0 && ptask->gid < gpool->num && ptask->f_stat) {
		OSPX_pthread_mutex_lock(&core->mut);
		entry_stat = gpool->entry[ptask->gid].lflags;
		f_stat = ptask->f_stat;
		f_vm = ptask->f_vmflags;
		OSPX_pthread_mutex_unlock(&core->mut);
			
		/**
		 * Parse the status
		 */
		if (entry_stat & SLOT_F_FREE) 
			f_stat = 0;
	}
	
	if (vm)
		*vm = f_vm;
	
	return f_stat;
}

int   
cpool_gp_suspend(cpool_com_t *core, long ms)
{
	int e = 0;
	cpool_gp_t *gpool = core->priv;
	
	OSPX_pthread_mutex_lock(&core->mut);
	/**
	 * Suspend the Core
	 */
	cpool_core_suspendl(core);
	/**
	 * Should we wait for both the dispatching tasks and the scheduling tasks ? 
	 */
	if (gpool->ndispatchings || core->n_qdispatchs || core->nthreads_running) {
		if (!ms)
			e = eERR_TIMEDOUT;
		else
			e = __cpool_gp_w_wait_cbl(gpool, -1, WAIT_CLASS_POOL|WAIT_TYPE_TASK, __cpool_gp_wcb_paused, NULL, ms);
	}
	OSPX_pthread_mutex_unlock(&core->mut);

	return e;
}

int  
cpool_gp_wait_all(cpool_core_t *core, long ms)
{
	int e;
	cpool_gp_t *gpool = core->priv;
	
	OSPX_pthread_mutex_lock(&core->mut);
	e = __cpool_gp_w_wait_cbl(gpool, -1, WAIT_CLASS_POOL|WAIT_TYPE_TASK_ALL, NULL, NULL, ms);
	OSPX_pthread_mutex_unlock(&core->mut);

	return e;
}

int   
cpool_gp_remove_all(cpool_core_t *core, int dispatched_by_pool)
{
	return cpool_gp_mark_all(core, dispatched_by_pool ? eTASK_VM_F_REMOVE_BYPOOL : eTASK_VM_F_REMOVE);
}

int   
cpool_gp_mark_all(cpool_core_t *core, long lflags)
{
	return cpool_gp_mark_cb(core, NULL, (void *)lflags);
}

int   
cpool_gp_mark_cb(cpool_core_t *core, Visit_cb cb, void *cb_arg)
{
	int neffs;
	LIST_HEAD(rmq);
	
	OSPX_pthread_mutex_lock(&core->mut);
	neffs = __cpool_gp_entry_mark_cbl(core->priv, NULL, cb, cb_arg, &rmq);
	OSPX_pthread_mutex_unlock(&core->mut);
	
	/**
	 * If there are tasks who has completion routine, we call the
	 * @__cpool_gp_task_dispatcher to dispatch them 
	 */
	if (!list_empty(&rmq)) 
		__cpool_gp_task_dispatcher(core->priv, &rmq);
	
	return neffs;
}

int   
cpool_gp_wait_cb(cpool_core_t *core, Visit_cb cb, void *cb_arg, long ms)
{
	int e;

	OSPX_pthread_mutex_lock(&core->mut);
	e = __cpool_gp_w_wait_cbl(core->priv, -1, WAIT_CLASS_POOL|WAIT_TYPE_TASK, cb, cb_arg, ms);
	OSPX_pthread_mutex_unlock(&core->mut);
	
	return e;
}

struct cpool_stat *
cpool_gp_stat(cpool_core_t *core, struct cpool_stat *stat)
{
	int idx = 0;
	struct cpool_core_stat core_stat;
	cpool_gp_t *gpool = core->priv;
		
	OSPX_pthread_mutex_lock(&core->mut);	
	cpool_core_statl(core, &core_stat);
	stat->waiters = gpool->tsk_wref + gpool->ev_wref;
	stat->tasks_peak = gpool->ntasks_peak;
	stat->tasks_added = gpool->seq;
	stat->tasks_processed = gpool->ntasks_processed0;
	for (; idx<gpool->num; idx++) {
		if (gpool->entry[idx].lflags & SLOT_F_FREE)
			continue;
		stat->tasks_processed += gpool->entry[idx].ntasks_processed;
	}
	stat->curtasks_pending = gpool->npendings;
	stat->curtasks_removing = gpool->ndispatchings;
	stat->throttle_on = gpool->throttle_on;
	OSPX_pthread_mutex_unlock(&core->mut);	
	
	stat->desc = core_stat.desc;
	stat->created = core_stat.start;
	stat->acttimeo = core_stat.acttimeo;
	stat->randtimeo = core_stat.randtimeo;
	stat->priq_num = gpool->priq_num;
	stat->ref = core_stat.user_ref;
	stat->suspended = core_stat.paused;
	stat->maxthreads = core_stat.max;
	stat->minthreads = core_stat.min;
	stat->curthreads = core_stat.n_qths;
	stat->curthreads_active = core_stat.nths_running;
	stat->curthreads_dying  = core_stat.nths_dying;
	stat->threads_peak = core_stat.nths_peak;
	stat->curtasks_scheduling = stat->curthreads_active;
	stat->tasks_removed = -1;

	return stat;
}

char *
cpool_gp_scheduler_map_dump(cpool_core_t *core, char *buff, size_t bufflen)
{
	OSPX_pthread_mutex_lock(&core->mut);
	buff = __cpool_gp_entry_dumpl(core->priv, buff, bufflen);
	OSPX_pthread_mutex_unlock(&core->mut);
	
	return buff;
}

