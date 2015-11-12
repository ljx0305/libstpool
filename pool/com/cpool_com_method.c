/* 
 *	  Stpool is a portable and efficient tasks pool library, it can work on diferent 
 * platforms such as Windows, linux, unix and ARM.  
 *
 *    If you have any troubles or questions on using the library, contact me.
 *
 * 	  (Email: piggy_xrh@163.com  QQ: 1169732280)
 */

#include "cpool_com_method.h"
#include "cpool_core.h"
#include "cpool_core_gc.h"
#include "cpool_factory.h"

void  
cpool_com_atexit(cpool_com_t *core, void (*__atexit)(cpool_com_t *ins, void *), void *opaque)
{
	cpool_core_atexit(core, __atexit, opaque);
}

void
cpool_com_adjust(cpool_com_t *core, int max, int min)
{
	OSPX_pthread_mutex_lock(&core->mut);
	max += core->maxthreads;
	min += core->minthreads;
	
	/**
	 * Correct the param 
	 */	
	if (max <= 0)
		max = 1;

	if (min <= 0)
		min = 0;
	min = min(max, min);	
	cpool_core_adjust_abs_l(core, max, min);
	OSPX_pthread_mutex_unlock(&core->mut);
}

void
cpool_com_adjust_abs(cpool_com_t *core, int max, int min)
{
	if (min < 0 || max < min) 
		return;

	OSPX_pthread_mutex_lock(&core->mut);	
	cpool_core_adjust_abs_l(core, max, min);
	OSPX_pthread_mutex_unlock(&core->mut);
}

int   
cpool_com_flush(cpool_com_t *core)
{
	int ndecs;
	
	OSPX_pthread_mutex_lock(&core->mut);
	ndecs = core->nthreads_real_pool - core->minthreads;
	ndecs = cpool_core_dec_threadsl(core, ndecs);
	OSPX_pthread_mutex_unlock(&core->mut);
	
	return ndecs;
}

void  
cpool_com_resume(cpool_com_t *core)
{
	cpool_core_resume(core);
}

long  
cpool_com_addref(cpool_com_t *core)
{
	long ref = 0;
	
	OSPX_pthread_mutex_lock(&core->mut);
	if (CORE_F_created & core->status)
		ref = cpool_core_addrefl(core, 1);
	OSPX_pthread_mutex_unlock(&core->mut);
	
	return ref;

}

long  
cpool_com_release(cpool_com_t *core)
{
	/**
	 * We do not waste our time on waiting for 
	 * the pool's being destroyed completely 
	 */
	return cpool_core_release_ex(core, 0, 0);
}

void  
cpool_com_setattr(cpool_com_t *core, struct thread_attr *attr)
{
	core->thattr.stack_size = attr->stack_size;
	core->thattr.sche_policy = (enum ep_POLICY)attr->ep_schep;
	core->thattr.sche_priority = attr->sche_priority;
}

void  
cpool_com_getattr(cpool_com_t *core, struct thread_attr *attr)
{
	attr->stack_size = core->thattr.stack_size;
	attr->ep_schep = (enum ep_TH)core->thattr.sche_policy;
	attr->sche_priority = core->thattr.sche_priority;
}

void  
cpool_com_set_activetimeo(cpool_com_t *core, long acttimeo, long randtimeo)
{
	core->acttimeo = acttimeo * 1000;
	core->randtimeo = randtimeo * 1000;
}

ctask_t *
cpool_com_cache_get(cpool_com_t *core)
{
	ctask_t *ptask ;
	
	cpool_core_slow_GC(core);
	ptask = smcache_get(core->cache_task, 0);

	if (!ptask) {
		cpool_core_objs_local_flush_all(core);
		ptask = smcache_get(core->cache_task, 1);
	}
	
	if (ptask) {
		ptask->f_reserved = 0;
		ptask->f_stat = 0;
		ptask->ref = 0;
		ptask->f_sys_flags = 0;
		ptask->user_flags  = 0;
		ptask->gid = 0;
		ptask->pri = 0;
		ptask->pri_policy  = ep_BACK;
		ptask->f_vmflags   = (eTASK_VM_F_PUSH|eTASK_VM_F_ENABLE_QUEUE|eTASK_VM_F_CACHE);
	}

	return ptask;
}

void  
cpool_com_cache_put(cpool_com_t *core, ctask_t *ptask)
{
	assert (ptask && ptask->f_vmflags & eTASK_VM_F_CACHE);

	smcache_add(core->cache_task, ptask);
}

