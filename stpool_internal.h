#ifndef __STPOOL_INTERNAL_H__
#define __STPOOL_INTERNAL_H__

/* 
 *	  Stpool is a portable and efficient tasks pool library, it can work on diferent 
 * platforms such as Windows, linux, unix and ARM.  
 *
 *    If you have any troubles or questions on using the library, contact me.
 *
 * 	  (Email: piggy_xrh@163.com  QQ: 1169732280)
 */

#include "objpool.h"
#include "stpool.h"
#include "cpool_factory.h"

#define M_POOL "pool"

#define _INVOKABLE0(func, p)  ((p)->me->me.func != NULL)
#define _INVOKABLE1(func, p)  ((p)->me->extme.func != NULL)
#define _INVOKABLE2(func, p)  ((p)->me->advme.func != NULL)

#define _INVOKE0(func, p, ...) (p)->me->me.func((p)->ctx, ##__VA_ARGS__)  
#define _INVOKE1(func, p, ...) (p)->me->extme.func((p)->ctx, ##__VA_ARGS__) 
#define _INVOKE2(func, p, ...) (p)->me->advme.func((p)->ctx, ##__VA_ARGS__) 

#define TASK_CAST_UP(ptsk)    ((struct sttask *)ptsk)
#define TASK_CAST_DOWN(ptsk)  ((ctask_t *)ptsk)
#define TASK_CAST_CB_RUN(run) ((void (*)(ctask_t *))run)
#define TASK_CAST_CB_ERR(err) ((void (*)(ctask_t *, long))err)

static struct eCAPs_conv_table {
	const char *desc;
	long libeCAP, fac_efunc;
} __conv[] = {
   {"eCAP_F_DYNAMIC",         eCAP_F_DYNAMIC,         eFUNC_F_DYNAMIC_THREADS},
   {"eCAP_F_FIXED",           eCAP_F_FIXED,           0},
   {"eCAP_F_PRIORITY",        eCAP_F_PRIORITY,        eFUNC_F_PRIORITY},
   {"eCAP_F_THROTTLE",        eCAP_F_THROTTLE,        0},
   {"eCAP_F_SUSPEND",         eCAP_F_SUSPEND,         0},
   {"eCAP_F_TRACE",       	  eCAP_F_TRACE,           eFUNC_F_TRACEABLE},
   {"eCAP_F_WAIT_ANY",        eCAP_F_WAIT_ANY,        0},
   {"eCAP_F_WAIT_ALL",        eCAP_F_WAIT_ALL,        0},
   {"eCAP_F_DISABLEQ",        eCAP_F_DISABLEQ,        eFUNC_F_DISABLEQ},
   {"eCAP_F_REMOVE_BYPOOL",   eCAP_F_REMOVE_BYPOOL,   0},
   {"eCAP_F_ROUTINE",         eCAP_F_ROUTINE,         0},
   {"eCAP_F_CUSTOM_TASK",     eCAP_F_CUSTOM_TASK,     eFUNC_F_TASK_EX},
   {"eCAP_F_TASK_WAIT",       eCAP_F_TASK_WAIT,   	  eFUNC_F_TASK_WAITABLE},
   {"eCAP_F_TASK_WAIT_ALL",   eCAP_F_TASK_WAIT_ALL,   0},
   {"eCAP_F_TASK_WAIT_ANY",   eCAP_F_TASK_WAIT_ANY,   0},
   {"eCAP_F_GROUP",           eCAP_F_GROUP,           eFUNC_F_ADVANCE},
   {"eCAP_F_GROUP_THROTTLE",  eCAP_F_GROUP_THROTTLE,  0},
   {"eCAP_F_GROUP_WAIT_ANY",  eCAP_F_GROUP_WAIT_ANY,  0},
   {"eCAP_F_GROUP_WAIT_ALL",  eCAP_F_GROUP_WAIT_ALL,  0},
   {"eCAP_F_GROUP_SUSPEND",   eCAP_F_GROUP_SUSPEND,   0},
};

static inline int
__count_funcs(int *entry, int len)
{
	int n = 0;

	for (;--len>=0;) 
		if (entry[len])
			++ n;

	return n;
}

static long
__enum_CAPs2(long efuncs, const cpool_method_t *const method, int *nfuncs)
{
	int idx;
	long libeCAPs = 0;

	for (idx=0; idx<sizeof(__conv)/sizeof(*__conv); idx++) {
		if (efuncs & __conv[idx].fac_efunc)
			libeCAPs |= __conv[idx].libeCAP;
	}
	if (nfuncs)
		*nfuncs = __count_funcs((int *)&method->me, sizeof(method->me)/sizeof(void *)) - 1;
	
	/**
	 * Now all of the factories support both eCAP_F_SUSPEND and eCAP_F_REMOVE_BYPOOL
	 */
	libeCAPs |= eCAP_F_SUSPEND|eCAP_F_REMOVE_BYPOOL;

	if (!(libeCAPs & eCAP_F_DYNAMIC))
		libeCAPs |= eCAP_F_FIXED;
	
	if (method->me.cache_get)
		libeCAPs |= eCAP_F_ROUTINE;
	
	if (method->me.wait_all)
		libeCAPs |= eCAP_F_WAIT_ALL;
		
	if (eFUNC_F_EXTEND & efuncs) {
		if (method->extme.throttle_enable)
			libeCAPs |= eCAP_F_THROTTLE;

		if (method->extme.wait_any)
			libeCAPs |= eCAP_F_WAIT_ANY;
		
		if (method->extme.wait_any2)
			libeCAPs |= eCAP_F_TASK_WAIT_ANY;
		
		if (method->extme.task_wait)
			libeCAPs |= eCAP_F_TASK_WAIT|eCAP_F_WAIT_ALL;
		
		if (nfuncs)
			*nfuncs += __count_funcs((int *)&method->extme, sizeof(method->extme)/sizeof(void *));
	}

	if (eFUNC_F_ADVANCE & efuncs) {
		if (method->advme.group_wait_any)
			libeCAPs |= eCAP_F_GROUP_WAIT_ANY;
		
		if (method->advme.group_wait_all)
			libeCAPs |= eCAP_F_GROUP_WAIT_ALL;
		
		if (method->advme.group_throttle_enable)
			libeCAPs |= eCAP_F_GROUP_THROTTLE;
		
		if (method->advme.group_suspend)
			libeCAPs |= eCAP_F_GROUP_SUSPEND;

		if (nfuncs)
			*nfuncs += __count_funcs((int *)&method->advme, sizeof(method->advme)/sizeof(void *));
	}

	return libeCAPs;
}

static inline long
__enum_CAPs(const cpool_factory_t *const fac, int *nfuncs)
{
	return __enum_CAPs2(fac->efuncs, fac->method, nfuncs);
}

static inline char *
__eCAPs_desc(long libeCAPs, char *eCAPs_buffer)
{
	int idx, n=0;
	
	/**
	 * We always assume that the buffer is big enough to
	 * store the datas
	 */
	eCAPs_buffer[0] = '\0';
	for (idx=0; idx<sizeof(__conv)/sizeof(*__conv); idx++) {
		if (libeCAPs & __conv[idx].libeCAP) {
			if (1 != ++ n)
				strcat(eCAPs_buffer, "|");
			strcat(eCAPs_buffer, __conv[idx].desc);
		}
	}

	return eCAPs_buffer;
}


static inline void 
__stpool_task_INIT(ctask_t *ptsk, const char *name, 
	void (*run)(struct sttask *), void (*err_handler)(struct sttask *, long), void *arg) 
{
	ptsk->task_desc = name;
	ptsk->task_run  = TASK_CAST_CB_RUN(run);
	ptsk->task_err_handler = TASK_CAST_CB_ERR(err_handler);
	ptsk->task_arg = arg;
	ptsk->task_code = 0;
	ptsk->ref = 0; 
	ptsk->pri = 0; 
	ptsk->f_stat = 0; 
	ptsk->gid = 0;
	ptsk->user_flags = 0;
	ptsk->pri_policy = ep_SCHE_BACK; 
}

static inline int 
__stpool_task_set_p(ctask_t *ptask, cpool_t *pool)
{
	int e;
	assert (!ptask->ref);
	
	if (pool && _INVOKABLE0(task_init, pool) &&
		(e = _INVOKE0(task_init, pool, ptask)))
		return e;

	ptask->f_vmflags &= ~eTASK_VM_F_DISABLE_QUEUE;
	ptask->f_vmflags |= eTASK_VM_F_ENABLE_QUEUE;
	ptask->pool = pool;	
	return 0;
}

extern smcache_t *___smc;

static void 
__stpool_init_global_task_cache()
{
	static objpool_t ___dummy_objp;
	
	if (objpool_ctor(&___dummy_objp, "FObjp-C-Global-task", stpool_task_size(), 0)) {
		MSG_log(M_POOL, LOG_ERR,
				"Failed to create the global task cache.\n");
		abort();
	}
	___smc = objpool_get_cache(&___dummy_objp);	
}

static inline ctask_t *
__stpool_cache_get(stpool_t *pool)
{
	ctask_t *ptask = NULL;
	
	if (pool && _INVOKABLE0(cache_get, pool)) {
		if ((ptask = _INVOKE0(cache_get, pool)))
			ptask->pool = pool;
	
	} else {
		static OSPX_pthread_once_t __octl = OSPX_PTHREAD_ONCE_INIT;
	
		OSPX_pthread_once(&__octl, __stpool_init_global_task_cache);
		assert (!pool || pool->efuncs & eFUNC_F_TASK_EX); 
		/**
		 * Create a task object and initialzie it 
		 */
		if (___smc && (ptask = smcache_get(___smc, 1))) {
			if (pool && __stpool_task_set_p(ptask, pool)) {
				smcache_add_dir(___smc, ptask);
				return NULL;
			}
		}
	}
	
	return ptask;
}

static inline void 
__stpool_cache_put(stpool_t *pool, ctask_t *ptask)
{
	if (pool && _INVOKABLE0(cache_get, pool))
		_INVOKE0(cache_put, pool, ptask);
	else
		smcache_add_dir(___smc, ptask);
}

static inline int
__stpool_liberror(int efac)
{
	return efac;
}

#endif
