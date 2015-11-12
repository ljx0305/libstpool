#ifndef __OSPX_COMPATIBLE_H__
#define __OSPX_COMPATIBLE_H__
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

#if defined(_WIN32) || defined(WIN32) || defined(_WIN64)
#undef  EXPORT
#ifdef _USRDLL
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif

#define PRI64 "%I64"
#undef  inline
#define inline _inline

#define snprintf _snprintf
#define srandom srand
#define random  rand
#define msleep(rest)   Sleep(rest)
#define sleep(rest)    Sleep(rest * 1000)
#define bzero(ptr, n)  memset(ptr, 0, n)

#else
#define EXPORT
#define PRI64 "%ll"

#define srandom srandom
#define random  random
#define msleep(rest) usleep(rest * 1000)
#define sleep(rest)  sleep(rest)
#define min(a, b) ((a) < (b) ? (a) : (b))
#define max(a, b) ((a) < (b) ? (b) : (a))

#endif

/* i <1~x> */
#define BIT_set(address, i) (((uint8_t *)address)[(i + 7)/8 -1] |= ((uint8_t)1 << (i-1)%8))
#define BIT_get(address, i) (((uint8_t *)address)[(i + 7)/8 -1] & ((uint8_t)1 << (i-1)%8))
#define BIT_clr(address, i) (((uint8_t *)address)[(i + 7)/8 -1] &= ~((uint8_t)1 << (i-1)%8))

#define unlikely(exp) exp
#define likely(exp)   exp

#endif
