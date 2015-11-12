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
cpool_gp_throttle_ctl(cpool_core_t *core, int on)
{
	cpool_gp_t *gpool = core->priv;
	
	OSPX_pthread_mutex_lock(&core->mut);
	if (gpool->throttle_on && !on)
		__cpool_gp_w_wakeup_pool_throttlel(gpool);	
	gpool->throttle_on = on;
	OSPX_pthread_mutex_unlock(&core->mut);
}

int   
cpool_gp_throttle_wait(cpool_core_t *core, long ms)
{
	int e;
	cpool_gp_t *gpool = core->priv;

	OSPX_pthread_mutex_lock(&core->mut);
	__cpool_gp_w_waitl_utils(gpool, WAIT_CLASS_POOL|WAIT_TYPE_THROTTLE,
		-1,	NULL, ms, e, us_startr(),
		/**
		 * If the pool is being destroyed, we return @eERR_DESTROYING
		 */
		if (CORE_F_destroying & cpool_core_statusl(core)) {
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
	OSPX_pthread_mutex_unlock(&core->mut);
			
	return e;
}

int   
cpool_gp_wait_any(cpool_core_t *core, long ms)
{
	int e;

	OSPX_pthread_mutex_lock(&core->mut);
	e = __cpool_gp_w_wait_cbl(core->priv, -1, WAIT_CLASS_POOL|WAIT_TYPE_TASK_ANY, NULL, NULL, ms);
	OSPX_pthread_mutex_unlock(&core->mut);
	
	return e;
}

int   
cpool_gp_task_wsync(cpool_core_t *core, ctask_t *ptask)
{
	cpool_gp_t *gpool = core->priv;
	
	if (ptask->ref) {
		OSPX_pthread_mutex_lock(&core->mut);
		for (;ptask->ref;)
			OSPX_pthread_cond_wait(&gpool->cond_sync, &core->mut);
		OSPX_pthread_mutex_unlock(&core->mut);
	}

	return 0;
}

int   
cpool_gp_task_wait(cpool_core_t *core, ctask_t *ptask, long ms)
{
	int e;
	
	if (!ptask->f_stat)
		return 0;

	assert (core == ptask->pool->ins);
	if (!ms)
		return eERR_TIMEDOUT;

	OSPX_pthread_mutex_lock(&core->mut);
	e = __cpool_gp_w_wait_cbl(core->priv, ptask->gid, WAIT_CLASS_ENTRY|WAIT_TYPE_TASK, NULL, (void *)ptask, ms);
	OSPX_pthread_mutex_unlock(&core->mut);
	
	return e;
}

int
cpool_gp_task_wait_any(cpool_core_t *core, ctask_t *entry[], int n, long ms) 
{
	int idx, e, m = 0;
	cpool_gp_t *gpool = core->priv;
	
	/**
	 * Scan the whole entry to find a task who is free now. 
	 */
	for (idx=0; idx<n; idx++) {
		if (!entry[idx])
			continue;
		
		/**
		 * The destionation pool of the task must be equal to the Core
		 */
		if (!entry[idx]->pool || entry[idx]->pool->ins != core)
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
	
	OSPX_pthread_mutex_lock(&core->mut);
	e = __cpool_gp_w_wait_cbl(gpool, n, WAIT_CLASS_POOL|WAIT_TYPE_TASK_ANY2, NULL, (ctask_t *)entry, ms);
	OSPX_pthread_mutex_unlock(&core->mut);

	return e;
}

