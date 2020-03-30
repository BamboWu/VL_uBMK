#define _MULTI_THREADED
#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>
#include "check.h"
#include <string.h>
#include <errno.h>

/*
 * Bind a thread to the specified core.
*/
void setAffinity(const int desired_core) {
   /**
    * pin the current thread 
    */
   cpu_set_t   *cpuset      = (cpu_set_t*) NULL;
   int cpu_allocate_size    = -1;
#if   (__GLIBC_MINOR__ > 9 ) && (__GLIBC__ == 2 )
   const int processors_to_allocate     = 1;
   cpuset = CPU_ALLOC( processors_to_allocate );
   cpu_allocate_size = CPU_ALLOC_SIZE( processors_to_allocate );
   CPU_ZERO_S( cpu_allocate_size, cpuset );
#else
   cpu_allocate_size = sizeof( cpu_set_t );
   cpuset = (cpu_set_t*) malloc( cpu_allocate_size );
   CPU_ZERO( cpuset );
#endif
   CPU_SET( desired_core,
            cpuset );
   errno = 0;
   if( sched_setaffinity( 0 /* calling thread */,
                         cpu_allocate_size,
                         cpuset ) != 0 )
   {
#define BUFF_LENGTH 1000      
      char buffer[ BUFF_LENGTH ];
      memset( buffer, '\0', BUFF_LENGTH );
      const char *str = strerror_r( errno, buffer, BUFF_LENGTH );
      fprintf( stderr, "Failed to set core affinity, with error message( %s )\n", str );
      exit( EXIT_FAILURE );
   }
   /** wait till we know we're on the right processor **/
   if( sched_yield() != 0 )
   {
      perror( "Failed to yield to wait for core change!\n" );
   }
}

/*
 * Name a thread.
 */
void nameThread(const char *desired_name) {
   int rc;
   rc = pthread_setname_np(pthread_self(), desired_name);
   checkResults("pthread_setname_np()", rc);
}
