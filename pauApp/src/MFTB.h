#ifndef __MFTB_H
#define __MFTB_H

#ifdef __rtems__
#include <rtems.h>              /* timer routines              */
#include <rtems/timerdrv.h>     /* timer routines              */
#include <bsp.h>                /* BSP*                        */
#if (__RTEMS_MAJOR__ > 4) || (__RTEMS_MAJOR__ == 4 && __RTEMS_MINOR__ >= 9)
#define Timer_initialize benchmark_timer_initialize
#define Read_timer benchmark_timer_read
#endif
#endif


#ifdef __rtems__
/*
 * From Till Straumann:
 * Macro for "Move From Time Base" to get current time in ticks.
 * The PowerPC Time Base is a 64-bit register incrementing usually
 * at 1/4 of the PPC bus frequency (which is CPU/Board dependent.
 * Even the 1/4 divisor is not fixed by the architecture).
 *
 * 'MFTB' just reads the lower 32-bit of the time base.
 */
#ifdef __PPC__
#define MFTB(var) asm volatile("mftb %0":"=r"(var))
#else
#define MFTB(var) (var)=Read_timer()
#endif
#endif

#if defined(i386) || defined(__i386) || defined(__i386__) || defined(_M_IX86) || defined(_X86_)  /*  for 32bits */ \
     || defined(__x86_64) || defined(__x86_64__) || defined(_M_X64)                              /*  for 64bits */
__inline__ static unsigned long long int rdtsc(void)
{
        unsigned long long int x;
        __asm__ volatile (".byte 0x0f, 0x31": "=A" (x));
        return x;
}

#define MFTB(var)  ((var)=(unsigned) rdtsc())
#endif


#endif /* __MFTB_H */
