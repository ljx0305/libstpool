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
#include "cpool_wait.h"
#include "cpool_com_internal.h"
#include "cpool_rt_internal.h"
#include "cpool_rt_method.h"

static void
cpool_rt_wakeup_throttle_wait(struct WWAKE_requester *r)
{
	cpool_rt_t *rtp = ((cpool_core_t *)r->opaque)->priv;
	
	if (!r->b_interrupted) {
		r->b_interrupted = 1;

		rtp->ev_need_notify = 0;
		OSPX_pthread_cond_broadcast(rtp->cond_event);
	}
}

void  
cpool_rt_throttle_ctl(cpool_ctx_t ctx, int on)
{
	cpool_rt_t *rtp = ctx;
	
	OSPX_pthread_mutex_lock(&rtp->core->mut);
	rtp->throttle_on = on;
	if (!on && rtp->ev_need_notify) {
		OSPX_pthread_cond_broadcast(rtp->cond_event);
		rtp->ev_need_notify = 0;
	}
	OSPX_pthread_mutex_unlock(&rtp->core->mut);
}

int   
cpool_rt_throttle_wait(cpool_ctx_t ctx, long ms)
{
	int e = 0;
	cpool_rt_t *rtp = ctx;
	uint64_t us_clock;
	
	DECLARE_WWAKE_REQUEST(r, 
						WWAKE_id(), 
						cpool_rt_wakeup_throttle_wait, 
						rtp);
	
	if (!rtp->throttle_on)
		return 0;
	
	if (!ms)
		return eERR_TIMEDOUT;
	us_clock = us_startr();
	/**
	 * Regist us at the WAKE sub system
	 */
	WWAKE_add(&r);
	
	OSPX_pthread_mutex_lock(&rtp->core->mut);
	for (;;) {
		if (!rtp->throttle_on) {
			e = 0;
			break;
		}

		if (CORE_F_destroying & cpool_core_statusl(rtp->core)) {
			e = eERR_DESTROYING;
			break;
		}

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
				e = eERR_TIMEDOUT; printf("Timeout\n");
				break;
			}
		}

		rtp->ev_need_notify = 1;
		++ rtp->ev_wref; 
		OSPX_pthread_cond_timedwait(rtp->cond_event, &rtp->core->mut, ms);
		-- rtp->ev_wref; 
		
		/**
		 * Check the sync env 
		 */
		if (!rtp->ev_wref && rtp->ref_sync) {
			rtp->ref_sync = 0;
			OSPX_pthread_cond_broadcast(rtp->cond_sync);
		}
	}
	OSPX_pthread_mutex_unlock(&rtp->core->mut);
	WWAKE_erase_direct(&r);

	return e;
}


