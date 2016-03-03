/* 
 *	  Stpool is a portable and efficient tasks pool library, it can work on diferent 
 * platforms such as Windows, linux, unix and ARM.  
 *
 *    If you have any troubles or questions on using the library, contact me.
 *
 * 	  (Email: piggy_xrh@163.com  QQ: 1169732280)
 */

#include "timer.h"
#include "cpool_factory.h"
#include "cpool_com_internal.h"
#include "cpool_gp_method.h"
#include "cpool_gp_wait_internal.h"

void  
cpool_gp_throttle_ctl(cpool_ctx_t ctx, int on)
{
	cpool_gp_t *gpool = ctx;
	
	OSPX_pthread_mutex_lock(&gpool->core->mut);
	if (gpool->throttle_on && !on)
		__cpool_gp_w_wakeup_pool_throttlel(gpool);	
	gpool->throttle_on = on;
	OSPX_pthread_mutex_unlock(&gpool->core->mut);
}

int   
cpool_gp_throttle_wait(cpool_ctx_t ctx, long ms)
{
	int e;
	cpool_gp_t *gpool = ctx;

	OSPX_pthread_mutex_lock(&gpool->core->mut);
	__cpool_gp_w_waitl_utils(gpool, WAIT_CLASS_POOL|WAIT_TYPE_THROTTLE,
		-1,	NULL, ms, e, us_startr(),
		/**
		 * If the pool is being destroyed, we return @eERR_DESTROYING
		 */
		if (CORE_F_destroying & cpool_core_statusl(gpool->core)) {
			e = eERR_DESTROYING;
			break;
		}

		/**
		 * Check the throttle status
		 */
		if (!gpool->throttle_on) {
			e = 0;
			break;
		}
	);
	OSPX_pthread_mutex_unlock(&gpool->core->mut);
			
	return e;
}

int   
cpool_gp_wait_any(cpool_ctx_t ctx, long ms)
{
	int e;
	cpool_gp_t *gpool = ctx;

	OSPX_pthread_mutex_lock(&gpool->core->mut);
	e = __cpool_gp_w_wait_cbl(ctx, -1, WAIT_CLASS_POOL|WAIT_TYPE_TASK_ANY, NULL, NULL, ms);
	OSPX_pthread_mutex_unlock(&gpool->core->mut);
	
	return e;
}

int   
cpool_gp_task_wsync(cpool_ctx_t ctx, ctask_t *ptask)
{
	cpool_gp_t *gpool = ctx;
	
	if (ptask->ref) {
		OSPX_pthread_mutex_lock(&gpool->core->mut);
		for (;ptask->ref;)
			OSPX_pthread_cond_wait(&gpool->cond_sync, &gpool->core->mut);
		OSPX_pthread_mutex_unlock(&gpool->core->mut);
	}

	return 0;
}

int   
cpool_gp_task_wait(cpool_ctx_t ctx, ctask_t *ptask, long ms)
{
	int e;
	cpool_gp_t *gpool = ctx;
	
	if (!ptask->f_stat)
		return 0;

	assert (ctx == ptask->pool->ins);
	if (!ms)
		return eERR_TIMEDOUT;

	OSPX_pthread_mutex_lock(&gpool->core->mut);
	e = __cpool_gp_w_wait_cbl(ctx, ptask->gid, WAIT_CLASS_ENTRY|WAIT_TYPE_TASK, NULL, (void *)ptask, ms);
	OSPX_pthread_mutex_unlock(&gpool->core->mut);
	
	return e;
}

int
cpool_gp_task_wait_any(cpool_ctx_t ctx, ctask_t *entry[], int n, long ms) 
{
	int idx, e, m = 0;
	cpool_gp_t *gpool = ctx;
	
	/**
	 * Scan the whole entry to find a task who is free now. 
	 */
	for (idx=0; idx<n; idx++) {
		if (!entry[idx])
			continue;
		
		/**
		 * The destionation pool of the task must be equal to the Core
		 */
		if (!entry[idx]->pool || entry[idx]->pool->ins != ctx)
			return eTASK_ERR_DESTINATION;
		/**
		 * If there are any tasks who is free now, we return 0 imediately 
		 */
		if (!entry[idx]->f_stat)
			return 0;
		
		++ m;
	}
	
	/**
	 * If the entry does not contain any valid tasks, we just return 0 
	 */
	if (!m)
		return 0;
	
	OSPX_pthread_mutex_lock(&gpool->core->mut);
	e = __cpool_gp_w_wait_cbl(gpool, n, WAIT_CLASS_POOL|WAIT_TYPE_TASK_ANY2, NULL, (ctask_t *)entry, ms);
	OSPX_pthread_mutex_unlock(&gpool->core->mut);

	return e;
}

