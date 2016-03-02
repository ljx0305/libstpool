/* 
 *	  Stpool is a portable and efficient tasks pool library, it can work on diferent 
 * platforms such as Windows, linux, unix and ARM.  
 *
 *    If you have any troubles or questions on using the library, contact me.
 *
 * 	  (Email: piggy_xrh@163.com  QQ: 1169732280)
 */

#include "msglog.h"
#include "timer.h"
#include "cpool_core.h"
#include "cpool_wait.h"
#include "cpool_factory.h"
#include "cpool_com_method.h"
#include "cpool_com_internal.h"
#include "cpool_rt_method.h"
#include "cpool_rt_internal.h"

static struct cpool_core_method_sets {
	const long  lflags;
	const cpool_core_method_t me;
} __core_me[] = {
	{	0,
		{
			"rt_me",
			0,
			cpool_rt_core_ctor,
			cpool_rt_core_notifyl,
			cpool_rt_core_gettask,
			cpool_rt_core_err_reasons,
			cpool_rt_core_finished,
			cpool_rt_core_dtor
	   }
	},
	{   eFUNC_F_PRIORITY,
		{
			"rt_pri_me",
			0,
			cpool_rt_core_ctor,
			cpool_rt_core_notifyl,
			cpool_rt_core_pri_gettask,
			cpool_rt_core_err_reasons,
			cpool_rt_core_pri_finished,
			cpool_rt_core_dtor
		}
	},
	{   eFUNC_F_DYNAMIC_THREADS,
		{
			"rt_dynamic_me",
			0,
			cpool_rt_core_ctor,
			cpool_rt_core_notifyl,
			cpool_rt_core_dynamic_gettask,
			cpool_rt_core_err_reasons,
			cpool_rt_core_dynamic_finished,
			cpool_rt_core_dtor
		}
	},
	{	eFUNC_F_DYNAMIC_THREADS|eFUNC_F_PRIORITY,
		{
			"rt_dynamic_pri_me",
			0,
			cpool_rt_core_ctor,
			cpool_rt_core_notifyl,
			cpool_rt_core_dynamic_pri_gettask,
			cpool_rt_core_err_reasons,
			cpool_rt_core_dynamic_pri_finished,
			cpool_rt_core_dtor
		}
	}
};

int  
cpool_rt_create_instance(cpool_rt_t *rtp, const char *core_desc, int max, int min, int priq_num, int suspend, long lflags)
{
	int idx;
	const cpool_core_method_t *me = NULL;
	long  core_flags = lflags & (eFUNC_F_DYNAMIC_THREADS|eFUNC_F_PRIORITY);

	/**
	 * Find the propriate methods for the Core
	 */
	for (idx=0; idx<sizeof(__core_me)/sizeof(*__core_me); idx++) {
		if (__core_me[idx].lflags == core_flags) {
			me = &__core_me[idx].me;
			break;
		}
	}

	if (!me) {
		MSG_log2(M_RT, LOG_ERR,
				"Can find the pripriate methods to support the request(%ld).",
				lflags);
		return -1;
	}

	/**
	 * If the pool is not created with eFUNC_F_DYNAMIC_THREADS, we
	 * should ignore the parameter \@min
	 */
	if (!(eFUNC_F_DYNAMIC_THREADS & lflags)) {
		min = max;
		core_flags = 0;
	} else
		core_flags = CORE_F_dynamic;

	/**
	 * Save the parameters 
	 */
	rtp->lflags = lflags;
	rtp->priq_num = priq_num;
	rtp->core->priv = rtp;

	/**
	 * Start creating the core 
	 */
	return cpool_core_ctor(rtp->core, core_desc, me, max, min, suspend, core_flags);
}

static void
cpool_rt_wakeup_task_wait(struct WWAKE_requester *r)
{
	cpool_rt_t *rtp = r->opaque;

	if (!r->b_interrupted) {
		r->b_interrupted = 1;
		rtp->tsk_need_notify = 0;
		OSPX_pthread_cond_broadcast(rtp->cond_task);
	}
}

int   
cpool_rt_suspend(cpool_core_t *core, long ms)
{
	int e = 0;
	uint64_t us_clock = us_startr();
	cpool_rt_t *rtp = core->priv;
	
	DECLARE_WWAKE_REQUEST(r, 
						WWAKE_id(), 
						cpool_rt_wakeup_task_wait, 
						rtp);
#ifndef NDEBUG
	int times = 0;
#endif

	/**
	 * Regist us at the WAKE sub system
	 */
	WWAKE_add(&r);

	OSPX_pthread_mutex_lock(&core->mut);
	/**
	 * Suspend the core
	 */
	cpool_core_suspendl(core);

	for (;;) {
		if (cpool_core_all_donel(core) && !rtp->tsks_held_by_dispatcher) {
			e = 0;
			break;
		} 
#ifndef NDEBUG
		if (++ times > 1) 
			MSG_log(M_RT, LOG_TRACE,
				"n_qths(%d) n_qths_wait(%d) n_qths_waked(%d) free(%d) npendings(%d) paused(%d) GC(%p) GC_status(%p)\n",
				core->n_qths, core->n_qths_wait, core->n_qths_waked, core->nthreads_real_free, core->npendings, 
				core->paused, core->GC, core->GC ? (void *)core->GC->status : NULL);
#endif
		if (r.b_interrupted) {
			e = eERR_INTERRUPTED;
			break;
		}

		/**
		 * Check the timeout
		 */
		if (ms >= 0) {
			if (ms > 0)
				ms -= us_endr(us_clock) / 1000;

			if (ms <= 0) {
				e = eERR_TIMEDOUT;
				break;
			}
		}

		rtp->tsk_need_notify = 1;
		++ rtp->tsk_wref;
		OSPX_pthread_cond_timedwait(rtp->cond_task, &core->mut, ms);
		-- rtp->tsk_wref;
		
		/**
		 * Check the sync env 
		 */
		if (!rtp->tsk_wref && rtp->ref_sync) {
			rtp->ref_sync = 0;
			OSPX_pthread_cond_broadcast(rtp->cond_sync);
		}
	}
	OSPX_pthread_mutex_unlock(&core->mut);
	/**
	 * Unregist
	 */
	WWAKE_erase_direct(&r);
		
	return e;
}

int   
cpool_rt_remove_all(cpool_core_t *core, int dispatched_bypool)
{
	return cpool_rt_mark_all(core, dispatched_bypool ? eTASK_VM_F_REMOVE_BYPOOL : eTASK_VM_F_REMOVE);
}

int   
cpool_rt_wait_all(cpool_core_t *core, long ms)
{
	int e = 0;
	uint64_t us_clock = us_startr();
	cpool_rt_t *rtp = core->priv;
	
	DECLARE_WWAKE_REQUEST(r, 
						WWAKE_id(), 
						cpool_rt_wakeup_task_wait, 
						rtp);
#ifndef NDEBUG
	int times = 0;
#endif

	/**
	 * Regist us at the WAKE sub system
	 */
	WWAKE_add(&r);

	OSPX_pthread_mutex_lock(&core->mut);
	for (;;) {
		if (cpool_core_all_donel(core) && !core->npendings && !rtp->tsks_held_by_dispatcher) {
			e = 0;
			break;
		} 
#ifndef NDEBUG
		if (++ times > 1) 
			MSG_log(M_RT, LOG_TRACE,
				"n_qths(%d) n_qths_wait(%d) n_qths_waked(%d) free(%d) npendings(%d) paused(%d) GC(%p) GC_status(%p)\n",
				core->n_qths, core->n_qths_wait, core->n_qths_waked, core->nthreads_real_free, core->npendings, 
				core->paused, core->GC, core->GC ? (void *)core->GC->status : NULL);
#endif
		if (r.b_interrupted) {
			e = eERR_INTERRUPTED;
			break;
		}

		/**
		 * Check the timeout
		 */
		if (ms >= 0) {
			if (ms > 0)
				ms -= us_endr(us_clock) / 1000;

			if (ms <= 0) {
				e = eERR_TIMEDOUT;
				break;
			}
		}

		rtp->tsk_need_notify = 1;
		++ rtp->tsk_wref;
		OSPX_pthread_cond_timedwait(rtp->cond_task, &core->mut, ms);
		-- rtp->tsk_wref;
		
		/**
		 * Check the sync env 
		 */
		if (!rtp->tsk_wref && rtp->ref_sync) {
			rtp->ref_sync = 0;
			OSPX_pthread_cond_broadcast(rtp->cond_sync);
		}
	}
	OSPX_pthread_mutex_unlock(&core->mut);
	/**
	 * Unregist
	 */
	WWAKE_erase_direct(&r);
		
	return e;
}

struct cpool_stat *
cpool_rt_stat(cpool_core_t *core, struct cpool_stat *stat)
{
	struct cpool_core_stat core_stat;
	cpool_rt_t *rtp = core->priv;

	bzero(stat, sizeof(*stat));
	
	OSPX_pthread_mutex_lock(&core->mut);	
	cpool_core_statl(core, &core_stat);
	stat->waiters = rtp->tsk_wref + rtp->ev_wref;
	stat->curtasks_removing = rtp->tsks_held_by_dispatcher;
	stat->throttle_on = rtp->throttle_on;
	OSPX_pthread_mutex_unlock(&core->mut);	
		
	return __cpool_rt_stat_conv(rtp, &core_stat, stat);
}

int
cpool_rt_task_init(cpool_core_t *core, ctask_t *ptask)
{
	if (eTASK_VM_F_CACHE & ptask->f_vmflags) 
		return 0;

	MSG_log(M_RT, LOG_ERR,
			"Only routine tasks are supported by this pool(%p). Core(%s/%p). task(%s/%p)\n",
			core, core->desc, core->priv, ptask->task_desc, ptask);
	
	return eERR_NSUPPORT;
}

static inline int
__cpool_rt_task_queue_preprocess(cpool_rt_t *rtp, ctask_t *ptask)
{
	assert (eTASK_VM_F_CACHE & ptask->f_vmflags &&
			(!ptask->f_stat || eTASK_STAT_F_SCHEDULING & ptask->f_stat));

	/**
	 * Remove the REMOVE flags
	 */
	ptask->f_vmflags &= ~eTASK_VM_F_REMOVE_FLAGS;

	if (CORE_F_destroying & cpool_core_statusl(rtp->core)) 
		return eERR_DESTROYING;
	
	if (eTASK_VM_F_DISABLE_QUEUE & ptask->f_vmflags)
		return eTASK_ERR_DISABLE_QUEUE;
	
	if (rtp->throttle_on)
		return eERR_THROTTLE;

	if (ptask->f_stat) 
		/**
		 * If the task is in progress, we try to mark it Re-scheduled
		 */
		ptask->f_stat |= eTASK_STAT_F_WPENDING;
	else
		ptask->f_stat = eTASK_STAT_F_WAITING;

	if (rtp->c.priq_num) {
		assert (rtp->lflags & eFUNC_F_PRIORITY);
		__cpool_com_task_nice_preprocess(&rtp->c, ptask);
	}

	return 0;
}

int   
cpool_rt_task_queue(cpool_core_t *core, ctask_t *ptask)
{
	uint8_t f_stat;
	int e, reschedule = 0;
	cpool_rt_t *rtp = core->priv;
	
	/**
	 * FIX BUGS: If user calls @stpool_task_queue in the Walk functions,
	 * it'll take us into crash if the app is linked with the debug library.
	 */
	assert (eTASK_VM_F_CACHE & ptask->f_vmflags &&
			(!ptask->f_stat || (eTASK_STAT_F_WAITING|eTASK_STAT_F_SCHEDULING) & ptask->f_stat));

	if (ptask->f_stat) {
		if (ptask->f_stat & (eTASK_STAT_F_WAITING|eTASK_STAT_F_WPENDING))
			return 0;
		reschedule = 1;
	}
	
	/**
	 * Save the task status
	 */
	f_stat = ptask->f_stat;
	if ((e = __cpool_rt_task_queue_preprocess(rtp, ptask))) {
		/**
	 	 * We recover the task's status if we fail to deliver the task into the pool
	     */
		ptask->f_stat = f_stat;
		
		return e;
	}
		
	if (!reschedule) {
		OSPX_pthread_mutex_lock(&core->mut);
		/**
		 * Queue the task 
		 */
		if (rtp->lflags & eFUNC_F_PRIORITY)
			__cpool_com_priq_insert(&rtp->c, ptask);
		else
			list_add_tail(&ptask->link, &rtp->ready_q);
		++ core->npendings;
		
		/**
		 * Notify the Core to schedule the tasks
		 */
		if (rtp->lflags & eFUNC_F_DYNAMIC_THREADS) {
			if (cpool_core_need_ensure_servicesl(core) && !core->paused)
				cpool_core_ensure_servicesl(core, NULL);
		
		} else if (cpool_core_waitq_sizel(core) && !core->paused)
			cpool_core_wakeup_n_sleeping_threadsl(core, 1);
		OSPX_pthread_mutex_unlock(&rtp->core->mut);
	}
	
	return 0;
}

int   
cpool_rt_task_remove(cpool_core_t *core, ctask_t *ptask, int dispatched_by_pool)
{
	assert (ptask->pool->ins == core);
	/**
	 * This interface is only be allowed to be called in the ctask_t::task_run
	 * or in the ctask_t::task_err_handler.
	 */
	assert (ptask->pool && ptask->f_stat && ptask->pool->ins == core &&
			eTASK_STAT_F_SCHEDULING & ptask->f_stat);
	
	ptask->f_stat &= ~eTASK_STAT_F_WPENDING;
	return 0;
}

void
cpool_rt_task_mark(cpool_core_t *core, ctask_t *ptask, long lflags)
{
	assert (ptask->pool->ins == core);
	/**
	 * This interface is only be allowed to be called in the ctask_t::task_run
	 * or in the ctask_t::task_err_handler.
	 */
	assert (ptask->pool && ptask->f_stat && ptask->pool->ins == core &&
			eTASK_STAT_F_SCHEDULING & ptask->f_stat);

	lflags &= eTASK_VM_F_USER_FLAGS;
	
	if (eTASK_VM_F_REMOVE_FLAGS & lflags) 
		ptask->f_stat &= ~eTASK_STAT_F_WPENDING;
	
	__cpool_com_task_mark(ptask, lflags); 
}

long  
cpool_rt_task_stat(cpool_core_t *core, ctask_t *ptask, long *vm)
{
	assert (ptask->pool->ins == core);

	/**
	 * This interface is only be allowed to be called in the ctask_t::task_run
	 * or in the ctask_t::task_err_handler.
	 */
	assert (ptask->pool && ptask->f_stat && ptask->pool->ins == core &&
			eTASK_STAT_F_SCHEDULING & ptask->f_stat);
	
	if (vm) 
		*vm = ptask->f_vmflags; 
		
	return ptask->f_stat;
}

int   
cpool_rt_mark_all(cpool_core_t *core, long lflags)
{
	int  neffs = 0;
	long lflags0;
	LIST_HEAD(rmq);
	cpriq_t *priq;
	ctask_t *ptask;
	cpool_rt_t *rtp = core->priv;
	
	/**
	 * Filt the flags
	 */
	if (!(lflags &= eTASK_VM_F_USER_FLAGS))
		return 0;
	lflags0 = lflags & ~eTASK_VM_F_REMOVE_FLAGS;
		
	OSPX_pthread_mutex_lock(&core->mut);
	if (core->npendings) {
		if (lflags & eTASK_VM_F_REMOVE_FLAGS) {
			neffs = core->npendings;

			/**
			 * Remove all pending tasks
			 */
			if (rtp->lflags & eFUNC_F_PRIORITY)
				__cpool_rt_priq_remove_all(rtp, &rmq);
			else
				__cpool_rt_remove_all(rtp, &rmq);	
			
			assert (core->npendings == 0);
			/**
			 * Update the task counter
			 */
			OSPX_interlocked_add(&rtp->tsks_held_by_dispatcher, (long)neffs);
		} else {
			/**
			 * Deal with other flags
			 */
			if (rtp->lflags & eFUNC_F_PRIORITY) 
				list_for_each_entry(priq, &rtp->ready_q, cpriq_t, link) {
					list_for_each_entry(ptask, &priq->task_q, ctask_t, link) {
						neffs += __cpool_com_task_mark(ptask, lflags0);
					}
				}
			else 
				list_for_each_entry(ptask, &rtp->ready_q, ctask_t, link) {
					neffs += __cpool_com_task_mark(ptask, lflags0);
				}
		}
	}
	OSPX_pthread_mutex_unlock(&core->mut);
	
	/**
	 * Dispatch the removed tasks
	 */
	if (!list_empty(&rmq)) {
		list_for_each_entry(ptask, &rmq, ctask_t, link) {
			ptask->f_vmflags |= (eTASK_VM_F_REMOVE_FLAGS & lflags);
			if (lflags0) 
				__cpool_com_task_mark(ptask, lflags0);
		}
		
		__cpool_rt_task_dispatch(rtp, &rmq, eTASK_VM_F_REMOVE_BYPOOL & lflags);
	}	

	return neffs;
}

#define CPOOL_RT_MARK_CB(ptask, cb, cb_arg, lflags, neffs, rmq, n, ok)	\
	do {\
		if ((lflags = cb(ptask, cb_arg)) && (lflags &= eTASK_VM_F_USER_FLAGS)) { \
			/**
			 * If the user wants to remove the task, we'll remove it from
			 * from the pending queue and call its error handler with code
			 * eReason_removed later.
			 */ \
			if (eTASK_VM_F_REMOVE_FLAGS & lflags) { \
				(ptask)->f_vmflags |= (eTASK_VM_F_REMOVE_FLAGS & lflags); \
				/**
				 * Only tasks who has been marked with eTASK_VM_REMOVE_BYPOOL and
				 * has the task error handler can be dispatched by the pool
				 */ \
				if (eTASK_VM_F_REMOVE_BYPOOL & lflags && ptask->task_err_handler) { \
					list_add_tail(&ptask->link, &core->dispatch_q); \
					++ core->n_qdispatchs; \
				} else { \
					++ n; \
					list_add_tail(&ptask->link, rmq); \
				} \
				ok = 1; \
			} \
			if (__cpool_com_task_mark(ptask, lflags) || ok) {\
				++ neffs; \
				ok = 0; \
			}\
		} \
	} while (0)

int   
cpool_rt_mark_cb(cpool_core_t *core, Visit_cb cb, void *cb_arg)
{
	int  neffs = 0, ok = 0;
	long lflags, task_counter = 0;
	LIST_HEAD(rmq);
	ctask_t *ptask, *n;
	cpriq_t *priq, *npriq;
	cpool_rt_t *rtp = core->priv;
	
	OSPX_pthread_mutex_lock(&core->mut);
	if (rtp->lflags & eFUNC_F_PRIORITY) {
		/**
		 * Scan the priority queue
		 */
		list_for_each_entry_safe(priq, npriq, &rtp->ready_q, cpriq_t, link) {
			list_for_each_entry_safe(ptask, n, &priq->task_q, ctask_t, link) {
				CPOOL_RT_MARK_CB(ptask, cb, cb_arg, lflags, neffs, &rmq, task_counter, ok);	
			}
		}
	} else
		list_for_each_entry_safe(ptask, n, &rtp->ready_q, ctask_t, link) {
			CPOOL_RT_MARK_CB(ptask, cb, cb_arg, lflags, neffs, &rmq, task_counter, ok);	
		}

	if (task_counter)
		OSPX_interlocked_add(&rtp->tsks_held_by_dispatcher, task_counter);
	/**
	 * Try to create threads to schedule the dispatching tasks
	 */
	if (core->n_qdispatchs && cpool_core_need_ensure_servicesl(core))
		cpool_core_ensure_servicesl(core, NULL);
	OSPX_pthread_mutex_unlock(&core->mut);
	
	/**
	 * Dispatch the removed tasks
	 */
	if (!list_empty(&rmq))
		__cpool_rt_task_dispatch(rtp, &rmq, 0);
	
	return neffs;
}

