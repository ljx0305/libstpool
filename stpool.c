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
#include <stdio.h>
#include <assert.h>

#include "ospx.h"
#include "ospx_compatible.h"
#include "timer.h"
#include "msglog.h"
#include "objpool.h"
#include "cpool_wait.h"
#include "cpool_factory.h"
#include "stpool.h"
#include "stpool_internal.h"

static size_t ___objlen = 0;
smcache_t *___smc = NULL;

/************************************************************************************/
/*****************************Interfaces about the task *****************************/
/************************************************************************************/
static void 
__stpool_task_size_init()
{
	const char *fac_desc;
	const cpool_factory_t *fac;
	
	/**
	 * We visit all of the factories to get the max size of task object
	 */
	for (fac=first_factory(&fac_desc); fac; fac=next_factory(&fac_desc)) {
		if (!(fac->efuncs & eFUNC_F_TASK_EX))
			continue;
		
		___objlen = max(___objlen, fac->method->me.task_size);
	}
	
	if (!___objlen)
		___objlen = sizeof(ctask_t);
}

EXPORT size_t
stpool_task_size()
{
	static OSPX_pthread_once_t ___octl = OSPX_PTHREAD_ONCE_INIT;
	
	if (!___objlen)
		OSPX_pthread_once(&___octl, __stpool_task_size_init);
	
	return ___objlen;
}

EXPORT int
stpool_task_init(struct sttask *ptask, stpool_t *pool,
				const char *name, void (*run)(struct sttask *ptask),
				void (*err_handler)(struct sttask *ptask, long reasons),
				void *arg) 
{	
	__stpool_task_INIT(TASK_CAST_DOWN(ptask), name, run, err_handler, arg);

	return stpool_task_set_p(ptask, pool);
}

EXPORT void
stpool_task_deinit(struct sttask *ptask)
{
	cpool_t *pool = TASK_CAST_DOWN(ptask)->pool;

	if (pool && ME_HAS(pool, task_deinit))
		ME_CALL(pool, task_deinit)(pool->ins, TASK_CAST_DOWN(ptask));
}

EXPORT struct sttask *
stpool_task_new(stpool_t *pool, const char *name,
				void (*run)(struct sttask *ptask),
				void (*err_handler)(struct sttask *ptask, long reasons),
				void *arg) 
{
	int e;
	ctask_t *ptask; 
	
	/**
	 * Does the pool support the Waitable tasks ?
	 */
	if (pool && !(eFUNC_F_TASK_EX & pool->efuncs)) {
		MSG_log(M_POOL, LOG_ERR,
				"Only ROUTINE tasks are supported by this pool(%s/%p) efuncs(%p).\n",
				pool->desc, pool, pool->efuncs);
		return NULL;
	}

	if (!(ptask = __stpool_cache_get(NULL)))
		return NULL;
	
	__stpool_task_INIT(ptask, name, run, err_handler, arg);
	if (pool && (e = __stpool_task_set_p(ptask, pool))) {
		MSG_log2(M_POOL, LOG_ERR,
			"__task_set_p: code(%d).",
			e);
		return NULL;
	}

	return TASK_CAST_UP(ptask);
}

EXPORT struct sttask *
stpool_task_clone(struct sttask *ptask, int clone_schattr) 
{
	struct sttask *nptask;

	nptask = stpool_task_new(TASK_CAST_DOWN(ptask)->pool, ptask->task_name,
				ptask->task_run, ptask->task_err_handler, ptask->task_arg);
	if (nptask) {
		if (clone_schattr) {
			struct schattr attr;
			
			stpool_task_getschattr(ptask, &attr);
			stpool_task_setschattr(nptask, &attr);
		}
		TASK_CAST_DOWN(nptask)->gid = TASK_CAST_DOWN(ptask)->gid;
	}

	return nptask;
}

EXPORT int 
stpool_task_clone_and_queue(struct sttask *ptask, int clone_schattr, struct sttask **ptask2, void *task_arg)
{
	int e;
	ctask_t *nptask;
	cpool_t *pool = TASK_CAST_DOWN(ptask)->pool;
	struct schattr *attr = NULL, attr0;
	
	if (!pool) 
		return POOL_TASK_ERR_DESTINATION;
	
	if (clone_schattr) {
		attr = &attr0;
		stpool_task_getschattr(ptask, attr);
	}

	if (!task_arg)
		task_arg = ptask->task_arg;
	
	if (!ptask2) {
		/**
		 * We try to get a task object from the cache, 
	 	 * (@__stpool_cache_put(ptask) should be called later to recycle it) 
		 */
		nptask = __stpool_cache_get(pool);
		if (!nptask)
			return POOL_ERR_NOMEM;
	
		__stpool_task_INIT(nptask, ptask->task_name, ptask->task_run, ptask->task_err_handler, task_arg);
		assert (!nptask->ref);
	
	} else {
		if (!(*ptask2 = stpool_task_new(pool, ptask->task_name, ptask->task_run, ptask->task_err_handler, task_arg)))
			return POOL_ERR_NOMEM;
		nptask = (ctask_t *)*ptask2;
	}
	nptask->gid = TASK_CAST_DOWN(ptask)->gid;

	if (attr)
		stpool_task_setschattr(TASK_CAST_UP(nptask), attr);

	/**
	 * Deliver the task into the queue 
	 */
	if ((e = ME_CALL(pool, task_queue)(pool->ins, nptask))) {
		if (ptask2)
			stpool_task_delete(*ptask2);	
		else
			__stpool_cache_put(pool, nptask);
		
		/**
		 * Get the export error code according to the inner error code 
		 */
		return __stpool_liberror(e);
	}

	return 0;
}

EXPORT void 
stpool_task_delete(struct sttask *ptask) 
{	
	assert (ptask && ___smc);
	assert (!(eTASK_VM_F_CACHE & TASK_CAST_DOWN(ptask)->f_vmflags));
	
	if (TASK_CAST_DOWN(ptask)->f_stat || TASK_CAST_DOWN(ptask)->ref) {
		ctask_t *ptask0 = TASK_CAST_DOWN(ptask);
		cpool_t *pool = ptask0->pool;
		
		assert (pool);
		if (TASK_CAST_DOWN(ptask)->f_stat || 
			(!ME_EX_HAS(pool, task_wsync) || ME_EX_CALL(pool, task_wsync)(pool->ins, ptask0))) {
			MSG_log(M_POOL, LOG_ERR,
				"It is not safe to destroy the task now. task(%s/%p) ref(%hhd) code(%d) stat:%p\n",
				ptask0->task_desc, ptask0, ptask0->ref, ptask0->task_code, 
				pool ? ME_CALL(pool, task_stat)(pool->ins, ptask0, NULL) : ptask0->f_stat);
		}
		assert (!ptask0->ref);
	}
	__stpool_cache_put(NULL, TASK_CAST_DOWN(ptask));
}

EXPORT int
stpool_task_set_p(struct sttask *ptask, stpool_t *pool)
{
	int e = 0;
	ctask_t *ptask0 = TASK_CAST_DOWN(ptask);
	
	if (ptask0->pool != pool) {
		if (ptask0->ref || ptask0->f_stat) {
			cpool_t *pool = ptask0->pool;
				
			assert (pool);
			if (ptask0->f_stat) {
				MSG_log(M_POOL, LOG_WARN,
						"@%s:Task(%s/%p) is in progress. ref(%hhd) stat(%p)\n",
						__FUNCTION__, ptask0->task_desc, ptask0, ptask0->ref, 
						pool ? ME_CALL(pool, task_stat)(pool->ins, ptask0, NULL) : (long)NULL);
				
				return POOL_ERR_BUSY;
			} 
					
			if (!ME_EX_HAS(pool, task_wsync) || (e = ME_EX_CALL(pool, task_wsync)(pool->ins, ptask0))) {
				MSG_log(M_POOL, LOG_WARN,
					"It is not safe to change the task's pool since its reference "
					"is not zero. task(%s/%p) ref(%hhd) stat(%p)\n",
					ptask->task_name, ptask, ptask0->ref, 
					ME_CALL(pool, task_stat)(pool->ins, ptask0, NULL));

				return __stpool_liberror(e);
			}
			assert (!ptask0->ref && !ptask0->f_stat);
		}
		
		/**
		 * We initialize it if the task has not been initialized before 
		 */
		if (ptask0->pool && ME_HAS(ptask0->pool, task_deinit)) {
			ME_CALL(ptask0->pool, task_deinit)(ptask0->pool->ins, ptask0);
			ptask0->pool = NULL;
		}

		return __stpool_task_set_p(ptask0, pool);
	}

	return 0;
}

EXPORT stpool_t *
stpool_task_p(struct sttask *ptask)
{	
	return TASK_CAST_DOWN(ptask)->pool;
}

EXPORT const char *
stpool_task_pname(struct sttask *ptask)
{
	stpool_t *p = TASK_CAST_DOWN(ptask)->pool;
	
	return p ? p->desc : NULL;
}

EXPORT void
stpool_task_set_userflags(struct sttask *ptask, unsigned short uflags) 
{	
	TASK_CAST_DOWN(ptask)->user_flags = uflags;
}

EXPORT unsigned short
stpool_task_get_userflags(struct sttask *ptask) 
{
	return TASK_CAST_DOWN(ptask)->user_flags;
}

EXPORT void 
stpool_task_setschattr(struct sttask *ptask, struct schattr *attr) 
{
	/**
	 * Correct the priority parameters
	 */
	if (attr->sche_pri < 0)
		attr->sche_pri = 0;
	if (attr->sche_pri > 99)
		attr->sche_pri = 99;
	
	/**
	 * If the task's scheduling attribute is not permanent,
	 * It'll be reset at the next scheduling time 
	 */
	if (!attr->permanent) 
		TASK_CAST_DOWN(ptask)->f_vmflags |= eTASK_VM_F_PRI_ONCE;	
	else
		TASK_CAST_DOWN(ptask)->f_vmflags &= ~eTASK_VM_F_PRI_ONCE;
	
	/**
	 * If the task has a zero priority, We just push the task
	 * into the lowest priority queue 
	 */
	if (!attr->sche_pri_policy || (!attr->sche_pri && ep_SCHE_BACK == attr->sche_pri_policy)) {
		TASK_CAST_DOWN(ptask)->f_vmflags &= ~(eTASK_VM_F_PRI|eTASK_VM_F_ADJPRI);
		TASK_CAST_DOWN(ptask)->f_vmflags |= eTASK_VM_F_PUSH;
		TASK_CAST_DOWN(ptask)->pri = 0;
		TASK_CAST_DOWN(ptask)->priq = 0;
	
	} else {
		/**
		 * We set the task with eTASK_VM_F_ADJPRI, The pool will 
		 * choose a propriate priority queue to queue the task
		 * when the user calls @stpool_task_queue 
		 */
		TASK_CAST_DOWN(ptask)->f_vmflags |= (eTASK_VM_F_PRI|eTASK_VM_F_ADJPRI);
		TASK_CAST_DOWN(ptask)->f_vmflags &= ~eTASK_VM_F_PUSH;
		TASK_CAST_DOWN(ptask)->pri = attr->sche_pri;	
	}
	TASK_CAST_DOWN(ptask)->pri_policy = attr->sche_pri_policy;	
}

EXPORT void 
stpool_task_getschattr(struct sttask *ptask, struct schattr *attr) 
{	
	attr->sche_pri = TASK_CAST_DOWN(ptask)->pri;
	attr->sche_pri_policy = TASK_CAST_DOWN(ptask)->pri_policy;
	
	if (TASK_CAST_DOWN(ptask)->f_vmflags & eTASK_VM_F_PRI_ONCE)
		attr->permanent = 0;
	else
		attr->permanent = 1;
}

EXPORT int
stpool_task_queue(struct sttask *ptask) 
{
	int e;
	stpool_t *pool = TASK_CAST_DOWN(ptask)->pool;
	
	if (!pool)
		return POOL_TASK_ERR_DESTINATION;
	
	if ((e=ME_CALL(pool, task_queue)(pool->ins, TASK_CAST_DOWN(ptask))))
		return __stpool_liberror(e);
	
	return 0;
}

EXPORT int
stpool_task_remove(struct sttask *ptask, int dispatched_by_pool)
{
	cpool_t *pool = TASK_CAST_DOWN(ptask)->pool;

	if (!pool || !TASK_CAST_DOWN(ptask)->f_stat)
		return 0;
	
	if (ME_HAS(pool, task_remove))
		return ME_CALL(pool, task_remove)(pool->ins, TASK_CAST_DOWN(ptask), dispatched_by_pool);

	if (ME_HAS(pool, task_mark))
		ME_CALL(pool, task_mark)(pool->ins, TASK_CAST_DOWN(ptask), dispatched_by_pool ? 
					TASK_VMARK_REMOVE_BYPOOL : TASK_VMARK_REMOVE);
	
	return TASK_CAST_DOWN(ptask)->f_stat ? 0 : 1;
}

/* NOTE:
 * 		@stpool_task_detach is only allowed being called in the
 * task's working routine or in the task's completion routine.
 */
EXPORT void
stpool_task_detach(struct sttask *ptask) 
{
	cpool_t *pool = TASK_CAST_DOWN(ptask)->pool;

	if (ME_HAS(pool, task_detach))
		ME_CALL(pool, task_detach)(pool->ins, TASK_CAST_DOWN(ptask));
}

EXPORT void
stpool_task_mark(struct sttask *ptask, long lflags) 
{	
	cpool_t *pool = TASK_CAST_DOWN(ptask)->pool;

	if (pool && ME_HAS(pool, task_mark))
		ME_CALL(pool, task_mark)(pool->ins, TASK_CAST_DOWN(ptask), lflags);
}

EXPORT int  
stpool_task_pthrottle_wait(struct sttask *ptask, long ms)
{
	stpool_t *pool = TASK_CAST_DOWN(ptask)->pool;

	if (!pool) {
		MSG_log(M_POOL, LOG_WARN,
				"tsk(%s/%p): Firstly, you should call @stpool_task_set_p to specify its destination\n",
				ptask->task_name, ptask);
		return POOL_TASK_ERR_DESTINATION;
	}

	return stpool_throttle_wait(pool, ms);
}

EXPORT int
stpool_task_wait(struct sttask *ptask, long ms)
{
	int e;
	cpool_t *pool = TASK_CAST_DOWN(ptask)->pool;

	if (!pool || !TASK_CAST_DOWN(ptask)->f_stat)
		return 0;

	if (!ME_EX_HAS(pool, task_wait))
		return POOL_ERR_NSUPPORT;
	
	if ((e=ME_EX_CALL(pool, task_wait)(pool->ins, TASK_CAST_DOWN(ptask), ms)))
		return __stpool_liberror(e);
	
	return 0;
}

EXPORT int  
stpool_task_wait_all(struct sttask *entry[], int n, long ms) 
{
	int idx, e = 0;
	uint64_t sclock = (ms > 0) ? us_startr() : 0;
	
	do {
		/**
		 * Scan the whole entry to find a task who is not
		 * free now, and then we wait for it in @ms 
		 */
		for (idx=0; idx<n; idx++) 
			if (entry[idx] && !stpool_task_is_free(entry[idx]))
				break;
		
		/**
		 * If there are none busy tasks existing in the entry,
		 * we return 0 imediately 
		 */
		if (idx == n)
			return 0;

		e = stpool_task_wait(entry[idx], ms);

		/**
		 * Caculate the left waiting time if it is necessary 
		 */
		if (ms > 0 && 
			0  > (ms -= us_endr(sclock) / 1000))
			ms = 0;
	} while (!e);

	return e;
}

EXPORT int  
stpool_task_wait_any(struct sttask *entry[], int n, long ms) 
{
	int idx, e;
	stpool_t *pool = NULL;
	
	/**
	 * Scan the whole entry to find a task who is free now. 
	 */
	for (idx=0; idx<n; idx++) {
		if (!entry[idx])
			continue;

		/**
		 * If there are any tasks who is free now, we return 0 imediately 
		 */
		if (stpool_task_is_free(entry[idx]) || !TASK_CAST_DOWN(entry[idx])->pool)
			return 0;
		
		/**
		 * Get the destination pool 
		 */
		if (!pool)
			pool = TASK_CAST_DOWN(entry[idx])->pool;

		else if (TASK_CAST_DOWN(entry[idx])->pool != pool)
			return POOL_TASK_ERR_DESTINATION;
	}
	
	if (!pool)
		return 0;

	if (!ME_EX_HAS(pool, wait_any2))
		return POOL_ERR_NSUPPORT;
	
	if ((e=ME_EX_CALL(pool, wait_any2)(pool->ins, (ctask_t **)entry, n, ms)))
		return __stpool_liberror(e);
	
	return 0;
}

EXPORT long
stpool_task_stat(struct sttask *ptask) 
{
	ctask_t *ptask0 = TASK_CAST_DOWN(ptask);
	cpool_t *pool = ptask0->pool;
	
	/**
	 * The task must have been intialized 
	 */
	assert (ptask0->task_run);
	
	if (!pool || !ptask0->f_stat)
		return 0;

	return ptask0->f_stat;
}

EXPORT long
stpool_task_vm(struct sttask *ptask) 
{
	return TASK_CAST_DOWN(ptask)->f_vmflags & (eTASK_VM_F_REMOVE_FLAGS|eTASK_VM_F_DISABLE_QUEUE|eTASK_VM_F_ENABLE_QUEUE);
}

EXPORT long
stpool_task_stat2(struct sttask *ptask, long *vm) 
{
	long stat;
	ctask_t *ptask0 = TASK_CAST_DOWN(ptask);
	cpool_t *pool = ptask0->pool;
	
	/**
	 * The task must have been intialized 
	 */
	assert (ptask0->task_run);
	
	if (!pool) {
		if (vm) 
			*vm = 0;
		
		return 0;
	}
		
	stat = ME_CALL(pool, task_stat)(pool->ins, ptask0, vm);
	if (vm)
		*vm &= (eTASK_VM_F_REMOVE_FLAGS|eTASK_VM_F_DISABLE_QUEUE|eTASK_VM_F_ENABLE_QUEUE);
	
	return stat;
}


EXPORT int
stpool_task_is_free(struct sttask *ptask)
{
	return !TASK_CAST_DOWN(ptask)->f_stat;
}

/************************************************************************************/
/*****************************Interfaces about the pool *****************************/
/************************************************************************************/
static void
__stpool_atexit(cpool_com_t *ins, void *opaque) 
{
	cpool_t *pool = opaque;
	
	pool->destroy(pool);
}

EXPORT const char *
stpool_version() 
{
	return "2015/10/12-3.2.0-libstpool-eCAPs";
}

EXPORT stpool_t * 
stpool_create(const char *desc, long eCAPs, int maxthreads, int minthreads, int suspend, int pri_q_num) 
{
	cpool_t *pool = NULL;
	long elibCAPs;
	int  nfuncs;
	const char *fac_desc;
	const cpool_factory_t *fac;
	char eCAPs_buffer[400];
	
	struct fac_candidate {
		const char *fac_desc;
		int nfuncs;
		long eCAPs;
		const cpool_factory_t *fac;
	} fac_sel[20];
	int idx, sel_idx = 0;

	
	/**
	 * It does not need to load the ospx library since 
     * we do not call any APIs who must use the TLS datas.
	 */
	MSG_log(M_POOL, LOG_INFO,
			"Request creating a pool(\"%s\") efuncs(%s) ...\n",
			desc, __eCAPs_desc(eCAPs, eCAPs_buffer));

	/**
	 * Select the best templates to create the pool 
	 */
	for (fac=first_factory(&fac_desc); fac; fac=next_factory(&fac_desc)) {
		elibCAPs = __enum_CAPs(fac, &nfuncs);
		if ((elibCAPs & eCAPs) == eCAPs) {
			MSG_log(M_POOL, LOG_DEBUG,
					"Find a Factory(\"%s\" scores(%d), nfuns(%d)): %s\n\n",
					fac_desc, fac->scores, nfuncs, __eCAPs_desc(elibCAPs, eCAPs_buffer));
			
			/**
			 * We skip it if the entry is full
			 */
			if (sel_idx == sizeof(fac_sel)/sizeof(*fac_sel))
				continue;
			
			/**
			 * Add the factory into our candidate entries
			 */
			for (idx=0; idx<sel_idx; idx++) {
				if (fac->scores > fac_sel[idx].fac->scores ||
					(fac->scores == fac_sel[idx].fac->scores && nfuncs > fac_sel[idx].nfuncs)) {
					memmove(fac_sel + idx + 1, fac_sel + idx, (sel_idx - idx) * sizeof(struct fac_candidate));
					break;
				}
			}
			fac_sel[idx].fac_desc = fac_desc;
			fac_sel[idx].nfuncs = nfuncs;
			fac_sel[idx].eCAPs = elibCAPs;
			fac_sel[idx].fac = fac;
			++ sel_idx;
		}
	}
	
	if (!sel_idx) {
		MSG_log(M_POOL, LOG_ERR,
				"Can not find any pool template to satify user. eCAPs(%p) (%s)\n",
				eCAPs, stpool_version());
		return NULL;
	}
	
	for (idx=0; idx<sel_idx; idx++) {
		MSG_log(M_POOL, LOG_INFO,
				"Factory(\"%s\" scores(%d) nfuns(%d)) try to service us. lib_eCAPs(%p) user_eCAPs(%p)\n",
				fac_sel[idx].fac_desc, fac_sel[idx].fac->scores, fac_sel[idx].nfuncs, fac_sel[idx].eCAPs, eCAPs);

		if ((pool = fac_sel[idx].fac->create(desc, maxthreads, minthreads, pri_q_num, suspend))) 
			break;

		MSG_log2(M_POOL, LOG_ERR,
			"Failed to create the pool: Factory(\"%s\"/%p).",
			fac_sel[idx].fac_desc, fac_sel[idx].fac);
	}

	if (idx != sel_idx && ME_HAS(pool, atexit))
		ME_CALL(pool, atexit)(pool->ins, __stpool_atexit, pool);

	return pool;
}

EXPORT long 
stpool_caps(stpool_t *pool)
{
	return __enum_CAPs2(pool->efuncs, pool->me, NULL);
}

EXPORT const char *
stpool_desc(stpool_t *pool)
{
	return pool->desc;
}

EXPORT void  
stpool_thread_setscheattr(stpool_t *pool, struct stpool_thattr *attr) 
{
	struct thread_attr attr0 = {0};

	if (ME_HAS(pool, setattr)) {
		/**
	 	 * Convert the libray attributs into the inner attributes
	 	 */
		assert (sizeof(attr0) == sizeof(*attr));
		
		memcpy(attr, &attr0, sizeof(*attr));

		ME_CALL(pool, setattr)(pool->ins, &attr0);
	}
}

EXPORT struct stpool_thattr *
stpool_thread_getscheattr(stpool_t *pool, struct stpool_thattr *attr) 
{	
	struct thread_attr attr0 = {0};

	if (ME_HAS(pool, getattr))
		ME_CALL(pool, getattr)(pool->ins, &attr0);

	/**
	 * Convert the attributs into the library attributes
	 */
	assert (sizeof(attr0) == sizeof(*attr));
	memcpy(attr, &attr0, sizeof(*attr));

	return attr;
}

EXPORT long 
stpool_addref(stpool_t *pool) 
{	
	assert (ME_HAS(pool, addref));
	
	return ME_CALL(pool, addref)(pool->ins);
}

EXPORT long 
stpool_release(stpool_t *pool) 
{
	long ref;
	
	assert (ME_HAS(pool, release));

	if (!(ref = ME_CALL(pool, release)(pool->ins)) && ___smc) 
		smcache_flush(___smc, 1);

	return ref;
}

EXPORT void 
stpool_set_activetimeo(stpool_t *pool, long acttimeo, long randtimeo) 
{
	if (ME_HAS(pool, set_activetimeo))
		ME_CALL(pool, set_activetimeo)(pool->ins, acttimeo, randtimeo);
}

EXPORT void 
stpool_adjust_abs(stpool_t *pool, int maxthreads, int minthreads) 
{
	assert (ME_HAS(pool, adjust_abs));
	ME_CALL(pool, adjust_abs)(pool->ins, maxthreads, minthreads);
}

EXPORT void 
stpool_adjust(stpool_t *pool, int maxthreads, int minthreads) 
{
	assert (ME_HAS(pool, adjust));
	ME_CALL(pool, adjust)(pool->ins, maxthreads, minthreads);
}

EXPORT int
stpool_flush(stpool_t *pool) 
{
	if (!ME_HAS(pool, flush))	
		return 0;

	return ME_CALL(pool, flush)(pool->ins);
}

EXPORT struct pool_stat *
stpool_stat(stpool_t *pool, struct pool_stat *stat) 
{
	static struct pool_stat __stat;
	struct cpool_stat stat0 = {0};

	assert (ME_HAS(pool, stat));
	ME_CALL(pool, stat)(pool->ins, &stat0);
	if (!stat)
		stat = &__stat;
	
	/**
	 * Convert the inner status into the library status
	 */
	assert (sizeof(*stat) == sizeof(stat0));
	memcpy(stat, &stat0, sizeof(*stat));

	return stat;
}

EXPORT const char *
stpool_stat_print(stpool_t *pool)
{
	struct pool_stat stat;

	stpool_stat(pool, &stat);
	
	return stpool_stat_print2(&stat, NULL, 0);
}

EXPORT const char *
stpool_stat_print2(struct pool_stat *stat, char *buffer, size_t bufferlen) 
{
	static char __buffer[1024] = {0};
	
	struct tm *p_tm;	
	char buffer0[200] = {0}, buffer1[50];
	char *pos = buffer0;
	
	if (!buffer) {
		buffer = __buffer;
		bufferlen = sizeof(__buffer);
	}

#define __libSTR(v, buff) (((v) == (unsigned int)-1) ? "*" : (sprintf(buff, "%d", v), buff))
	pos += sprintf(pos, "   tasks_peak  : %s\n", __libSTR(stat->tasks_peak, buffer1)); 
	pos += sprintf(pos, "   tasks_added : %s\n", __libSTR(stat->tasks_added, buffer1));
	pos += sprintf(pos, "tasks_processed: %s\n", __libSTR(stat->tasks_processed, buffer1));
	pos += sprintf(pos, " tasks_removed : %s\n", __libSTR(stat->tasks_removed, buffer1));
	
	p_tm = localtime(&stat->created);
	buffer[bufferlen -1] = '\0';
	
	/**
	 * Format the pool status to make it readable for us
	 */
	snprintf(buffer, bufferlen - 1, 
			"    desc  : \"%s\"\n"
			"   created: %04d-%02d-%02d %02d:%02d:%02d\n"
			"  user_ref: %ld\n"
			" pri_q_num: %d\n"
			" suspended: %s\n"
			"  throttle: %s\n"
			"maxthreads: %d\n"
			"minthreads: %d\n"
			" threads_actto : %.2f/%.2f (s)\n"
			"threads_current: %d\n"
			" threads_active: %d\n"
			" threads_dying : %d\n"
			" threads_peak  : %d\n"
			"curtasks_pending: %d\n"
			"curtasks_scheduling: %d\n"
			"curtasks_removing: %d\n"
			"%s\n",
			stat->desc, 
			p_tm->tm_year + 1900, p_tm->tm_mon + 1, p_tm->tm_mday,
			p_tm->tm_hour, p_tm->tm_min, p_tm->tm_sec,
			stat->ref,
			stat->priq_num,
			stat->suspended ? "yes" : "no",
			stat->throttle_enabled ? "on" : "off",
			stat->maxthreads,
			stat->minthreads,
			(double)stat->acttimeo / 1000,
			(double)stat->randtimeo / 1000,
			stat->curthreads,
			stat->curthreads_active,
			stat->curthreads_dying,
			stat->threads_peak,
			stat->curtasks_pending,
			stat->curtasks_scheduling,
			stat->curtasks_removing,
			buffer0
		);
			
	return buffer;
}

EXPORT char *
stpool_scheduler_map_dump2(stpool_t *pool, char *buffer, int len)
{
	if (!ME_HAS(pool, scheduler_map_dump))
		return NULL;
	
	return ME_CALL(pool, scheduler_map_dump)(pool->ins, buffer, len);
}

EXPORT int  
stpool_add_routine(stpool_t *pool, 
				const char *name, void (*task_run)(struct sttask *ptask),
				void (*task_err_handler)(struct sttask *ptask, long reasons),
				void *task_arg, struct schattr *attr)
{	
	int e;
	ctask_t *ptask;
	
	/**
	 * We try to get a task object from the cache, 
	 * (@__stpool_cache_put should be called later to recycle it) 
	 */
	ptask = __stpool_cache_get(pool);
	if (!ptask)
		return POOL_ERR_NOMEM;
	
	__stpool_task_INIT(ptask, name, task_run, task_err_handler, task_arg);
	if (attr)
		stpool_task_setschattr(TASK_CAST_UP(ptask), attr);
	
	/**
	 * Pay the task object back to cache if we fail 
	 * to add it into the pool's pending queue 
	 */
	if ((e = __stpool_task_set_p(ptask, pool)) || 
		(e = stpool_task_queue(TASK_CAST_UP(ptask)))) {
		__stpool_cache_put(pool, ptask);
		e = __stpool_liberror(e);
	}

	return e;
}

EXPORT long
stpool_mark_all(stpool_t *pool, long lflags) 
{
	MSG_log(M_POOL, LOG_INFO,
			"{\"%s\"/%p} Marking all tasks with %p ...\n",
			pool->desc, pool, lflags);
	
	if (!ME_HAS(pool, mark_all)) {
		if (ME_HAS(pool, remove_all) && TASK_VMARK_REMOVE & lflags) 
			return ME_CALL(pool, remove_all)(pool->ins, lflags & TASK_VMARK_REMOVE_BYPOOL);
		return 0;
	}
	
	return ME_CALL(pool, mark_all)(pool->ins, lflags);
}

EXPORT int  
stpool_mark_cb(stpool_t *pool, Walk_cb wcb, void *wcb_arg)
{
	if (!ME_HAS(pool, mark_cb))
		return 0;
	
	return ME_CALL(pool, mark_cb)(pool->ins, (Visit_cb)wcb, wcb_arg);
}
	
EXPORT int
stpool_throttle_enable(stpool_t *pool, int enable) 
{
	MSG_log(M_POOL, LOG_INFO,
			"{\"%s\"/%p} %s the throttle ...\n",
			pool->desc, pool, enable ? "ENABLING" : "DISABLING");
	
	if (!ME_EX_HAS(pool, throttle_enable)) 
		return POOL_ERR_NSUPPORT;
	
	ME_EX_CALL(pool, throttle_enable)(pool->ins, enable);
	return 0;
}

EXPORT int  
stpool_throttle_wait(stpool_t *pool, long ms) 
{
	int e;

	if (!ME_EX_HAS(pool, throttle_wait))
		return 0;
	
	if ((e = ME_EX_CALL(pool, throttle_wait)(pool->ins, ms)))
		e = __stpool_liberror(e);
	
	return e;
}

EXPORT int
stpool_suspend(stpool_t *pool, long ms) 
{
	int e;

	MSG_log(M_POOL, LOG_INFO,
			"{\"%s\"/%p} suspend ... (%ld ms)\n",
			pool->desc, pool, ms);

	if (!ME_HAS(pool, suspend))
		return POOL_ERR_NSUPPORT;

	if ((e=ME_CALL(pool, suspend)(pool->ins, ms)))
		return __stpool_liberror(e);
	
	return 0;
}

EXPORT void 
stpool_resume(stpool_t *pool) 
{
	MSG_log(M_POOL, LOG_INFO,
			"{\"%s\"/%p} resume ... \n",
			pool->desc, pool);

	assert (ME_HAS(pool, resume));
	ME_CALL(pool, resume)(pool->ins);
}

EXPORT int  
stpool_remove_all(stpool_t *pool, int dispatched_by_pool) 
{
	MSG_log(M_POOL, LOG_INFO,
			"{\"%s\"/%p} remove all tasks ... (%d)\n",
			pool->desc, pool, dispatched_by_pool);

	if (!ME_HAS(pool, remove_all))
		return 0;
	
	return ME_CALL(pool, remove_all)(pool->ins, dispatched_by_pool);
}

EXPORT long 
stpool_wakeid() 
{
	return WWAKE_id();
}

EXPORT int  
stpool_wait_all(stpool_t * pool, long ms)
{
	int e;
	MSG_log(M_POOL, LOG_INFO,
			"{\"%s\"/%p} start waiting for all tasks's being done ... (%ld ms)\n",
			pool->desc, pool, ms);

	if (!ME_HAS(pool, wait_all))
		return POOL_ERR_NSUPPORT;
	
	if ((e = ME_CALL(pool, wait_all)(pool->ins, ms)))
		e = __stpool_liberror(e);
	
	return e;
}

EXPORT int  
stpool_wait_cb(stpool_t *pool, Walk_cb wcb, void *wcb_arg, long ms)
{
	int e;

	if (!ME_EX_HAS(pool, wait_cb))
		return POOL_ERR_NSUPPORT;
	
	if ((e = ME_EX_CALL(pool, wait_cb)(pool->ins, (Visit_cb)wcb, wcb_arg, ms)))
		e = __stpool_liberror(e);
	
	return e;
}

EXPORT int  
stpool_wait_any(stpool_t *pool, long ms)
{
	int e;

	if (!ME_EX_HAS(pool, wait_any))
		return POOL_ERR_NSUPPORT;
	
	if ((e = ME_EX_CALL(pool, wait_any)(pool->ins, ms)))
		e = __stpool_liberror(e);
	
	return e;
}


EXPORT void 
stpool_wakeup(long wakeup_id) 
{
	WWAKE_invoke(wakeup_id);
}


