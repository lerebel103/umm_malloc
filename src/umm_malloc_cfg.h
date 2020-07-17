/*
 * Configuration for umm_malloc - DO NOT EDIT THIS FILE BY HAND!
 *
 * Refer to the notes below for how to configure the build at compile time
 * using -D to define non-default values
 */

#ifndef _UMM_MALLOC_CFG_H
#define _UMM_MALLOC_CFG_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*
 * There are a number of defines you can set at compile time that affect how
 * the memory allocator will operate.
 *
 * Unless otherwise noted, the default state of these values is #undef-ined!
 *
 * If you set them via the -D option on the command line (preferred method)
 * then this file handles all the configuration automagically and warns if
 * there is an incompatible configuration.
 *
 * UMM_TEST_BUILD
 *
 * Set this if you want to compile in the test suite
 *
 * UMM_BEST_FIT (default)
 *
 * Set this if you want to use a best-fit algorithm for allocating new blocks.
 * On by default, turned off by UMM_FIRST_FIT
 *
 * UMM_FIRST_FIT
 *
 * Set this if you want to use a first-fit algorithm for allocating new blocks.
 * Faster than UMM_BEST_FIT but can result in higher fragmentation.
 *
 * UMM_INFO
 *
 * Set if you want the ability to calculate metrics on demand
 *
 * UMM_INLINE_METRICS
 *
 * Set this if you want to have access to a minimal set of heap metrics that
 * can be used to gauge heap health.
 * Setting this at compile time will automatically set UMM_INFO.
 * Note that enabling this define will add a slight runtime penalty.
 *
 * UMM_INTEGRITY_CHECK
 *
 * Set if you want to be able to verify that the heap is semantically correct
 * before or after any heap operation - all of the block indexes in the heap
 * make sense.
 * Slows execution dramatically but catches errors really quickly.
 *
 * UMM_POISON_CHECK
 *
 * Set if you want to be able to leave a poison buffer around each allocation.
 * Note this uses an extra 8 bytes per allocation, but you get the benefit of
 * being able to detect if your program is writing past an allocated buffer.
 *
 * UMM_DBG_LOG_LEVEL=n
 *
 * Set n to a value from 0 to 6 depending on how verbose you want the debug
 * log to be
 *
 * ----------------------------------------------------------------------------
 *
 * Support for this library in a multitasking environment is provided when
 * you add bodies to the UMM_CRITICAL_ENTRY and UMM_CRITICAL_EXIT macros
 * (see below)
 *
 * ----------------------------------------------------------------------------
 */

/* A couple of macros to make packing structures less compiler dependent */

#define UMM_H_ATTPACKPRE
#define UMM_H_ATTPACKSUF __attribute__((__packed__))

/* -------------------------------------------------------------------------- */

#ifdef UMM_BEST_FIT
  #ifdef  UMM_FIRST_FIT
    #error Both UMM_BEST_FIT and UMM_FIRST_FIT are defined - pick one!
  #endif
#else /* UMM_BEST_FIT is not defined */
  #ifndef UMM_FIRST_FIT
    #define UMM_BEST_FIT
  #endif
#endif

/* -------------------------------------------------------------------------- */

#ifdef UMM_INLINE_METRICS
  #define UMM_FRAGMENTATION_METRIC_INIT() umm_fragmentation_metric_init()
  #define UMM_FRAGMENTATION_METRIC_ADD(c) umm_fragmentation_metric_add(c)
  #define UMM_FRAGMENTATION_METRIC_REMOVE(c) umm_fragmentation_metric_remove(c)
#else
  #define UMM_FRAGMENTATION_METRIC_INIT()
  #define UMM_FRAGMENTATION_METRIC_ADD(c)
  #define UMM_FRAGMENTATION_METRIC_REMOVE(c)
#endif // UMM_INLINE_METRICS

/* -------------------------------------------------------------------------- */

#ifdef UMM_INFO
  typedef struct UMM_HEAP_INFO_t {
    unsigned int totalEntries;
    unsigned int usedEntries;
    unsigned int freeEntries;

    unsigned int totalBlocks;
    unsigned int usedBlocks;
    unsigned int freeBlocks;
    unsigned int freeBlocksSquared;

    unsigned int maxFreeContiguousBlocks;
  }
  UMM_HEAP_INFO;

  extern UMM_HEAP_INFO ummHeapInfo;

  extern void *umm_info( void *ptr, bool force );
  extern size_t umm_free_heap_size( void );
  extern size_t umm_max_free_block_size( void );
  extern int umm_usage_metric( void );
  extern int umm_fragmentation_metric( void );
#else
  #define umm_info(p,b)
  #define umm_free_heap_size() (0)
  #define umm_max_free_block_size() (0)
  #define umm_fragmentation_metric() (0)
  #define umm_in_use_metric() (0)
#endif

/*
 * A couple of macros to make it easier to protect the memory allocator
 * in a multitasking system. You should set these macros up to use whatever
 * your system uses for this purpose. You can disable interrupts entirely, or
 * just disable task switching - it's up to you
 *
 * NOTE WELL that these macros MUST be allowed to nest, because umm_free() is
 * called from within umm_malloc()
 */

#ifdef UMM_TEST_BUILD
    extern int umm_critical_depth;
    extern int umm_max_critical_depth;
    #define UMM_CRITICAL_ENTRY() {\
          ++umm_critical_depth; \
          if (umm_critical_depth > umm_max_critical_depth) { \
              umm_max_critical_depth = umm_critical_depth; \
          } \
    }
    #define UMM_CRITICAL_EXIT()  (umm_critical_depth--)
#else
    #define UMM_CRITICAL_ENTRY()
    #define UMM_CRITICAL_EXIT()
#endif

/*
 * Enables heap integrity check before any heap operation. It affects
 * performance, but does NOT consume extra memory.
 *
 * If integrity violation is detected, the message is printed and user-provided
 * callback is called: `UMM_HEAP_CORRUPTION_CB()`
 *
 * Note that not all buffer overruns are detected: each buffer is aligned by
 * 4 bytes, so there might be some trailing "extra" bytes which are not checked
 * for corruption.
 */

#ifdef UMM_INTEGRITY_CHECK
   extern bool umm_integrity_check( void );
#  define INTEGRITY_CHECK() umm_integrity_check()
   extern void umm_corruption(void);
#  define UMM_HEAP_CORRUPTION_CB() printf( "Heap Corruption!" )
#else
#  define INTEGRITY_CHECK() (1)
#endif

/*
 * Enables heap poisoning: add predefined value (poison) before and after each
 * allocation, and check before each heap operation that no poison is
 * corrupted.
 *
 * Other than the poison itself, we need to store exact user-requested length
 * for each buffer, so that overrun by just 1 byte will be always noticed.
 *
 * Customizations:
 *
 *    UMM_POISON_SIZE_BEFORE:
 *      Number of poison bytes before each block, e.g. 4
 *    UMM_POISON_SIZE_AFTER:
 *      Number of poison bytes after each block e.g. 4
 *    UMM_POISONED_BLOCK_LEN_TYPE
 *      Type of the exact buffer length, e.g. `uint16_t`
 *
 * NOTE: each allocated buffer is aligned by 4 bytes. But when poisoning is
 * enabled, actual pointer returned to user is shifted by
 * `(sizeof(UMM_POISONED_BLOCK_LEN_TYPE) + UMM_POISON_SIZE_BEFORE)`.
 * It's your responsibility to make resulting pointers aligned appropriately.
 *
 * If poison corruption is detected, the message is printed and user-provided
 * callback is called: `UMM_HEAP_CORRUPTION_CB()`
 */

#ifdef UMM_POISON_CHECK
   #define UMM_POISON_SIZE_BEFORE (6)
   #define UMM_POISON_SIZE_AFTER (6)
   #define UMM_POISONED_BLOCK_LEN_TYPE uint16_t

   extern void *umm_poison_malloc( size_t size );
   extern void *umm_poison_calloc( size_t num, size_t size );
   extern void *umm_poison_realloc( void *ptr, size_t size );
   extern void  umm_poison_free( void *ptr );
   extern bool  umm_poison_check( void );

   #define POISON_CHECK() umm_poison_check()
#else
  #define POISON_CHECK() (1)
#endif

#endif /* _UMM_MALLOC_CFG_H */
