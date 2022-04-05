/*   
 *   File: ssmem.h
 *   Author: Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *   Description: ssmem interface and structures
 *   ssmem.h is part of ASCYLIB
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Vasileios Trigonakis <vasileios.trigonakis@epfl.ch>
 *                Distributed Programming Lab (LPD), EPFL
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

/* December 10, 2013 */
#ifndef _SSMEM_H_
#define _SSMEM_H_

#include <stdio.h>
#include <stdint.h>

/* **************************************************************************************** */
/* parameters */
/* **************************************************************************************** */

#define SSMEM_TRANSPARENT_HUGE_PAGES 0 /* Use or not Linux transparent huge pages */
#define SSMEM_ZERO_MEMORY            0 /* Initialize allocated memory to 0 or not */
#define SSMEM_GC_FREE_SET_SIZE 507 /* mem objects to free before doing a GC pass */
#define SSMEM_GC_RLSE_SET_SIZE 3   /* num of released object before doing a GC pass */
#define SSMEM_DEFAULT_MEM_SIZE (32 * 1024 * 1024L) /* memory-chunk size that each threads
                gives to the allocators */
#define SSMEM_MEM_SIZE_DOUBLE  0 /* if the allocator is out of memory, should it allocate
          a 2x larger chunk than before? (in order to stop asking
         for memory again and again */
#define SSMEM_MEM_SIZE_MAX     (4 * 1024 * 1024 * 1024LL) /* absolute max chunk size 
                 (e.g., if doubling is 1) */

/* increase the thread-local timestamp of activity on each ssmem_alloc() and/or ssmem_free() 
   call. If enabled (>0), after some memory is alloced and/or freed, the thread should not 
   access ANY ssmem-protected memory that was read (the reference were taken) before the
   current alloc or free invocation. If disabled (0), the program should employ manual 
   SSMEM_SAFE_TO_RECLAIM() calls to indicate when the thread does not hold any ssmem-allocated
   memory references. */

#define SSMEM_TS_INCR_ON_NONE   0
#define SSMEM_TS_INCR_ON_BOTH   1
#define SSMEM_TS_INCR_ON_ALLOC  2
#define SSMEM_TS_INCR_ON_FREE   3

#define SSMEM_TS_INCR_ON        SSMEM_TS_INCR_ON_FREE
/* **************************************************************************************** */
/* help definitions */
/* **************************************************************************************** */
#define ALIGNED(N) __attribute__ ((aligned (N)))
#define CACHE_LINE_SIZE 64

/* **************************************************************************************** */
/* data structures used by ssmem */
/* **************************************************************************************** */

/* an ssmem allocator */
typedef struct ALIGNED(CACHE_LINE_SIZE) ssmem_allocator
{
  union
  {
    struct
    {
      void* mem;    /* the actual memory the allocator uses */
      size_t mem_curr;    /* pointer to the next addrr to be allocated */
      size_t mem_size;    /* size of mem chunk */
      size_t tot_size;    /* total memory that the allocator uses */
      size_t fs_size;   /* size (in objects) of free_sets */
      struct ssmem_list* mem_chunks; /* list of mem chunks (used to free the mem) */

      struct ssmem_ts* ts;  /* timestamp object associated with the allocator */

      struct ssmem_free_set* free_set_list; /* list of free_set. A free set holds freed mem 
               that has not yet been reclaimed */
      size_t free_set_num;  /* number of sets in the free_set_list */
      struct ssmem_free_set* collected_set_list; /* list of collected_set. A collected set
              contains mem that has been reclaimed */
      size_t collected_set_num; /* number of sets in the collected_set_list */
      struct ssmem_free_set* available_set_list; /* list of set structs that are not used
              and can be used as free sets */
      size_t released_num;  /* number of released memory objects */
      struct ssmem_released* released_mem_list; /* list of release memory objects */
    };
    uint8_t padding[2 * CACHE_LINE_SIZE];
  };
} ssmem_allocator_t;

/* a timestamp used by a thread */
typedef struct ALIGNED(CACHE_LINE_SIZE) ssmem_ts
{
  union
  {
    struct
    {
      size_t version;
      size_t id;
      struct ssmem_ts* next;
    };
  };
  uint8_t padding[CACHE_LINE_SIZE];
} ssmem_ts_t;

/* 
 * a timestamped free_set. It holds:  
 *  1. the collection of timestamps at the point when the free_set gets full
 *  2. the array of freed pointers to be used by ssmem_free()
 *  3. a set_next pointer in order to be able to create linked lists of
 *   free_sets
 */
typedef struct ALIGNED(CACHE_LINE_SIZE) ssmem_free_set
{
  size_t* ts_set;   /* set of timestamps for GC */
  size_t size;
  long int curr;    
  struct ssmem_free_set* set_next;
  uintptr_t* set;
} ssmem_free_set_t;


/* 
 * a timestamped node of released memory. The memory will be returned to the OS
 * (free(node->mem)) when the current timestamp is greater than the one of the node
 */
typedef struct ssmem_released
{
  size_t* ts_set;
  void* mem;
  struct ssmem_released* next;
} ssmem_released_t;

/*
 * a generic list that keeps track of actual memory that has been allocated
 * (using malloc / memalign) and the different allocators that the list is using
 */
typedef struct ssmem_list
{
  void* obj;
  struct ssmem_list* next;
} ssmem_list_t;

/* **************************************************************************************** */
/* ssmem interface */
/* **************************************************************************************** */

/* initialize an allocator with the default number of objects */
void ssmem_alloc_init(ssmem_allocator_t* a, size_t size, int id);
/* initialize an allocator and give the number of objects in free_sets */
void ssmem_alloc_init_fs_size(ssmem_allocator_t* a, size_t size, size_t free_set_size, int id);
/* explicitely subscribe to the list of threads in order to used timestamps for GC */
void ssmem_gc_thread_init(ssmem_allocator_t* a, int id);
/* terminate the system (all allocators) and free all memory */
void ssmem_term();
/* terminate the allocator a and free all its memory
 * This function should NOT be used if the memory allocated by this allocator
 * might have been freed (and is still in use) by other allocators */
void ssmem_alloc_term(ssmem_allocator_t* a);

/* allocate some memory using allocator a */
void* ssmem_alloc(ssmem_allocator_t* a, size_t size, bool flush);
/* free some memory using allocator a */
void ssmem_free(ssmem_allocator_t* a, void* obj, bool flush);

/* release some memory to the OS using allocator a */
void ssmem_release(ssmem_allocator_t* a, void* obj);

/* increment the thread-local activity counter. Invoking this function suggests that
 no memory references to ssmem-allocated memory are held by the current thread beyond
this point. */
void ssmem_ts_next();
#define SSMEM_SAFE_TO_RECLAIM() ssmem_ts_next()


/* debug/help functions */
void ssmem_ts_list_print();
size_t* ssmem_ts_set_collect();
void ssmem_ts_set_print(size_t* set);

void ssmem_free_list_print(ssmem_allocator_t* a);
void ssmem_collected_list_print(ssmem_allocator_t* a);
void ssmem_available_list_print(ssmem_allocator_t* a);
void ssmem_all_list_print(ssmem_allocator_t* a, int id);


/* **************************************************************************************** */
/* platform-specific definitions */
/* **************************************************************************************** */

#if defined(__x86_64__)
#  define CAS_U64(a,b,c) __sync_val_compare_and_swap(a,b,c)
#  define FAI_U32(a) __sync_fetch_and_add(a,1)
#endif

#if defined(__sparc__)
#  include <atomic.h>
#  define CAS_U64(a,b,c) atomic_cas_64(a,b,c)
#  define FAI_U32(a) (atomic_inc_32_nv(a)-1)
#endif

#if defined(__tile__)
#  include <arch/atomic.h>
#  include <arch/cycle.h>
#  define CAS_U64(a,b,c) arch_atomic_val_compare_and_exchange(a,b,c)
#  define FAI_U32(a) arch_atomic_increment(a)
#endif

/* begin ssmem.c */

#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <assert.h>
#include <string.h>
#include <iostream>

#include <persist/pmem_utils.hpp>
#include <persist/utils.hpp>

ssmem_ts_t *ssmem_ts_list = nullptr;
volatile uint32_t ssmem_ts_list_len = 0;
__thread volatile ssmem_ts_t *ssmem_ts_local = nullptr;
__thread size_t ssmem_num_allocators = 0;
__thread ssmem_list_t *ssmem_allocator_list = nullptr;

inline int
ssmem_get_id()
{
  if (ssmem_ts_local != nullptr)
  {
    return ssmem_ts_local->id;
  }
  return -1;
}

static ssmem_list_t *ssmem_list_node_new(void *mem, ssmem_list_t *next, bool flush);

/* 
 * explicitely subscribe to the list of threads in order to used timestamps for GC
 */
void ssmem_gc_thread_init(ssmem_allocator_t *a, int id)
{
  a->ts = (ssmem_ts_t *)ssmem_ts_local;
  if (a->ts == nullptr)
  {
    a->ts = (ssmem_ts_t *)aligned_alloc(CACHE_LINE_SIZE, sizeof(ssmem_ts_t));
    assert(a->ts != nullptr);
    ssmem_ts_local = a->ts;

    a->ts->id = id;
    a->ts->version = 0;

    do
    {
      a->ts->next = ssmem_ts_list;
    } while (CAS_U64((volatile uint64_t *)&ssmem_ts_list,
             (uint64_t)a->ts->next, (uint64_t)a->ts) != (uint64_t)a->ts->next);

    __attribute__((unused)) uint32_t null = FAI_U32(&ssmem_ts_list_len);
  }
}

ssmem_free_set_t *ssmem_free_set_new(size_t size, ssmem_free_set_t *next);

/* 
 * initialize allocator a with a custom free_set_size
 * If the thread is not subscribed to the list of timestamps (used for GC),
 * additionally subscribe the thread to the list
 */
void ssmem_alloc_init_fs_size(ssmem_allocator_t *a, size_t size, size_t free_set_size, int id)
{
  ssmem_num_allocators++;
  ssmem_allocator_list = ssmem_list_node_new((void *)a, ssmem_allocator_list, true);

#if SSMEM_TRANSPARENT_HUGE_PAGES == 1
  int ret = posix_memalign(&a->mem, CACHE_LINE_SIZE, size);
  assert(ret == 0);
#else
  a->mem = (void *)aligned_alloc(CACHE_LINE_SIZE, size);
#endif
  assert(a->mem != nullptr);
#if SSMEM_ZERO_MEMORY == 1
  memset(a->mem, 0, size);
#endif

  a->mem_curr = 0;
  a->mem_size = size;
  a->tot_size = size;
  a->fs_size = free_set_size;

  a->mem_chunks = ssmem_list_node_new(a->mem, nullptr, true);
  FLUSH(&a->mem_chunks);
  FENCE();    

  ssmem_gc_thread_init(a, id);

  a->free_set_list = ssmem_free_set_new(a->fs_size, nullptr);
  a->free_set_num = 1;

  a->collected_set_list = nullptr;
  a->collected_set_num = 0;

  a->available_set_list = nullptr;

  a->released_mem_list = nullptr;
  a->released_num = 0;
}

/* 
 * initialize allocator a with the default SSMEM_GC_FREE_SET_SIZE
 * If the thread is not subscribed to the list of timestamps (used for GC),
 * additionally subscribe the thread to the list
 */
void ssmem_alloc_init(ssmem_allocator_t *a, size_t size, int id)
{
  return ssmem_alloc_init_fs_size(a, size, SSMEM_GC_FREE_SET_SIZE, id);
}

/* 
 * 
 */
static ssmem_list_t *
ssmem_list_node_new(void *mem, ssmem_list_t *next, bool flush = true)
{
  ssmem_list_t *mc;
  mc = (ssmem_list_t *)malloc(sizeof(ssmem_list_t));
  assert(mc != nullptr);
  mc->obj = mem;
  mc->next = next;
  if(flush) {
    FLUSH(mc);
    FENCE();
  }
  return mc;
}

/* 
 *
 */
inline ssmem_released_t *
ssmem_released_node_new(void *mem, ssmem_released_t *next)
{
  ssmem_released_t *rel;
  rel = (ssmem_released_t *)malloc(sizeof(ssmem_released_t) + (ssmem_ts_list_len * sizeof(size_t)));
  assert(rel != nullptr);
  rel->mem = mem;
  rel->next = next;
  rel->ts_set = (size_t *)(rel + 1);

  return rel;
}

/* 
 * 
 */
ssmem_free_set_t *
ssmem_free_set_new(size_t size, ssmem_free_set_t *next)
{
  /* allocate both the ssmem_free_set_t and the free_set with one call */
  ssmem_free_set_t *fs = (ssmem_free_set_t *)aligned_alloc(CACHE_LINE_SIZE, utils::round_up(sizeof(ssmem_free_set_t) + (size * sizeof(uintptr_t)), CACHE_LINE_SIZE));
  assert(fs != nullptr);

  fs->size = size;
  fs->curr = 0;

  fs->set = (uintptr_t *)(((uintptr_t)fs) + sizeof(ssmem_free_set_t));
  fs->ts_set = nullptr; /* will get a ts when it becomes full */
  fs->set_next = next;

  return fs;
}

/* 
 * 
 */
ssmem_free_set_t *
ssmem_free_set_get_avail(ssmem_allocator_t *a, size_t size, ssmem_free_set_t *next)
{
  ssmem_free_set_t *fs;
  if (a->available_set_list != nullptr)
  {
    fs = a->available_set_list;
    a->available_set_list = fs->set_next;

    fs->curr = 0;
    fs->set_next = next;

    /* printf("[ALLOC] got free_set from available_set : %p\n", fs); */
  }
  else
  {
    fs = ssmem_free_set_new(size, next);
  }

  return fs;
}

/* 
 * 
 */
static void
ssmem_free_set_free(ssmem_free_set_t *set)
{
  free(set->ts_set);
  free(set);
}

/* 
 * 
 */
static inline void
ssmem_free_set_make_avail(ssmem_allocator_t *a, ssmem_free_set_t *set)
{
  /* printf("[ALLOC] added to avail_set : %p\n", set); */
  set->curr = 0;
  set->set_next = a->available_set_list;
  a->available_set_list = set;
}

/* 
 * terminated allocator a and free its memory
 */
void ssmem_alloc_term(ssmem_allocator_t *a)
{
  /* printf("[ALLOC] term() : ~ total mem used: %zu bytes = %zu KB = %zu MB\n", */
  /*   a->tot_size, a->tot_size / 1024, a->tot_size / (1024 * 1024)); */
  ssmem_list_t *mcur = a->mem_chunks;
  do
  {
    ssmem_list_t *mnxt = mcur->next;
    free(mcur->obj);
    free(mcur);
    mcur = mnxt;
  } while (mcur != nullptr);

  ssmem_list_t *prv = ssmem_allocator_list;
  ssmem_list_t *cur = ssmem_allocator_list;
  while (cur != nullptr && (uintptr_t)cur->obj != (uintptr_t)a)
  {
    prv = cur;
    cur = cur->next;
  }

  if (cur == nullptr)
  {
    printf("[ALLOC] ssmem_alloc_term: could not find %p in the ssmem_allocator_list\n", a);
  }
  else if (cur == prv)
  {
    ssmem_allocator_list = cur->next;
    free(cur);
  }
  else
  {
    prv->next = cur->next;
    free(cur);
  }

  if (--ssmem_num_allocators == 0)
  {
    free(a->ts);
  }

  /* printf("[ALLOC] free(free_set)\n"); fflush(stdout); */
  /* freeing free sets */
  ssmem_free_set_t *fs = a->free_set_list;
  while (fs != nullptr)
  {
    ssmem_free_set_t *nxt = fs->set_next;
    ssmem_free_set_free(fs);
    fs = nxt;
  }

  /* printf("[ALLOC] free(collected_set)\n"); fflush(stdout); */
  /* freeing collected sets */
  fs = a->collected_set_list;
  while (fs != nullptr)
  {
    ssmem_free_set_t *nxt = fs->set_next;
    ssmem_free_set_free(fs);
    fs = nxt;
  }

  /* printf("[ALLOC] free(available_set)\n"); fflush(stdout); */
  /* freeing available sets */
  fs = a->available_set_list;
  while (fs != nullptr)
  {
    ssmem_free_set_t *nxt = fs->set_next;
    ssmem_free_set_free(fs);
    fs = nxt;
  }

  /* freeing the relased memory */
  ssmem_released_t *rel = a->released_mem_list;
  while (rel != nullptr)
  {
    ssmem_released_t *next = rel->next;
    free(rel->mem);
    free(rel);
    rel = next;
  }
}

/* 
 * terminate all allocators
 */
void ssmem_term()
{
  while (ssmem_allocator_list != nullptr)
  {
    ssmem_alloc_term((ssmem_allocator_t *)ssmem_allocator_list->obj);
  }
}

/* 
 * 
 */
void
ssmem_ts_next()
{
  ssmem_ts_local->version++;
}

/* 
 * 
 */
size_t *
ssmem_ts_set_collect(size_t *ts_set)
{
  if (ts_set == nullptr)
  {
    ts_set = (size_t *)malloc(ssmem_ts_list_len * sizeof(size_t));
    assert(ts_set != nullptr);
  }

  ssmem_ts_t *cur = ssmem_ts_list;
  while (cur != nullptr)
  {
    //std::cout << cur->id << " " << ssmem_ts_list_len << std::endl;
    ts_set[cur->id] = cur->version;
    cur = cur->next;
  }

  return ts_set;
}

/* 
 * 
 */
void ssmem_ts_set_print(size_t *set)
{
  printf("[ALLOC] set: [");
  for (unsigned int i = 0; i < ssmem_ts_list_len; i++)
  {
    printf("%zu | ", set[i]);
  }
  printf("]\n");
}

#if !defined(PREFETCHW)
#if defined(__x86_64__) | defined(__i386__)
#define PREFETCHW(x) asm volatile("prefetchw %0" ::"m"(*(unsigned long *)(x))) /* write */
#elif defined(__sparc__)
#define PREFETCHW(x) __builtin_prefetch((const void *)(x), 1, 3)
#elif defined(__tile__)
#include <tmc/alloc.h>
#include <tmc/udn.h>
#include <tmc/sync.h>
#define PREFETCHW(x) tmc_mem_prefetch((x), 64)
#else
#warning "You need to define PREFETCHW(x) for your architecture"
#endif
#endif

/* 
 * 
 */
void *
ssmem_alloc(ssmem_allocator_t *a, size_t size, bool flush = true)
{
  void *m = nullptr;

  /* 1st try to use from the collected memory */
  ssmem_free_set_t *cs = a->collected_set_list;
  if (cs != nullptr)
  {
    m = (void *)cs->set[--cs->curr];
    PREFETCHW(m);

    if (cs->curr <= 0)
    {
      a->collected_set_list = cs->set_next;
      a->collected_set_num--;

      ssmem_free_set_make_avail(a, cs);
    }
  }
  else
  {
    if ((a->mem_curr + size) >= a->mem_size)
    {
#if SSMEM_MEM_SIZE_DOUBLE == 1
      a->mem_size <<= 1;
      if (a->mem_size > SSMEM_MEM_SIZE_MAX)
      {
        a->mem_size = SSMEM_MEM_SIZE_MAX;
      }
#endif
      /* printf("[ALLOC] out of mem, need to allocate (chunk = %llu MB)\n", */
      /*   a->mem_size / (1LL<<20)); */
      if (size > a->mem_size)
      {
        /* printf("[ALLOC] asking for large mem. chunk\n"); */
        while (a->mem_size < size)
        {
          if (a->mem_size > SSMEM_MEM_SIZE_MAX)
          {
            fprintf(stderr, "[ALLOC] asking for memory chunk larger than max (%llu MB) \n",
                SSMEM_MEM_SIZE_MAX / (1024 * 1024LL));
            assert(a->mem_size <= SSMEM_MEM_SIZE_MAX);
          }
          a->mem_size <<= 1;
        }
        /* printf("[ALLOC] new mem size chunk is %llu MB\n", a->mem_size / (1024 * 1024LL)); */
      }
#if SSMEM_TRANSPARENT_HUGE_PAGES == 1
      int ret = posix_memalign(&a->mem, CACHE_LINE_SIZE, a->mem_size);
      assert(ret == 0);
#else
      a->mem = (void *)aligned_alloc(CACHE_LINE_SIZE, a->mem_size);
#endif
      assert(a->mem != nullptr);
#if SSMEM_ZERO_MEMORY == 1
      memset(a->mem, 0, a->mem_size);
#endif

      a->mem_curr = 0;

      a->tot_size += a->mem_size;

      a->mem_chunks = ssmem_list_node_new(a->mem, a->mem_chunks, flush);
      if(flush) {
        FLUSH(&a->mem_chunks);
        FENCE();        
      }
    }

    m = (void *)((char *)(a->mem) + a->mem_curr);
    a->mem_curr += size;
  }

#if SSMEM_TS_INCR_ON == SSMEM_TS_INCR_ON_ALLOC || SSMEM_TS_INCR_ON == SSMEM_TS_INCR_ON_BOTH
  ssmem_ts_next();
#endif
  return m;
}

/* return > 0 iff snew is > sold for each entry */
static int
ssmem_ts_compare(size_t *s_new, size_t *s_old)
{
  int is_newer = 1;
  for (unsigned int i = 0; i < ssmem_ts_list_len; i++)
  {
    // std::cout << i << " " << ssmem_ts_list_len << std::endl;
    if (s_new[i] <= s_old[i])
    {
      is_newer = 0;
      break;
    }
  }
  return is_newer;
}

/* return > 0 iff s_1 is > s_2 > s_3 for each entry */
static int __attribute__((unused))
ssmem_ts_compare_3(size_t *s_1, size_t *s_2, size_t *s_3)
{
  int is_newer = 1;
  for (unsigned int i = 0; i < ssmem_ts_list_len; i++)
  {
    if (s_1[i] <= s_2[i] || s_2[i] <= s_3[i])
    {
      is_newer = 0;
      break;
    }
  }
  return is_newer;
}

static void ssmem_ts_set_print_no_newline(size_t *set);

/* 
 *
 */
int ssmem_mem_reclaim(ssmem_allocator_t *a)
{
  if (__builtin_expect(a->released_num > 0, 0))
  {
    ssmem_released_t *rel_cur = a->released_mem_list;
    ssmem_released_t *rel_nxt = rel_cur->next;

    if (rel_nxt != nullptr && ssmem_ts_compare(rel_cur->ts_set, rel_nxt->ts_set))
    {
      rel_cur->next = nullptr;
      a->released_num = 1;
      /* find and collect the memory */
      do
      {
        rel_cur = rel_nxt;
        free(rel_cur->mem);
        free(rel_cur);
        rel_nxt = rel_nxt->next;
      } while (rel_nxt != nullptr);
    }
  }

  ssmem_free_set_t *fs_cur = a->free_set_list;
  if (fs_cur->ts_set == nullptr)
  {
    return 0;
  }
  ssmem_free_set_t *fs_nxt = fs_cur->set_next;
  int gced_num = 0;

  if (fs_nxt == nullptr || fs_nxt->ts_set == nullptr) /* need at least 2 sets to compare */
  {
    return 0;
  }

  if (ssmem_ts_compare(fs_cur->ts_set, fs_nxt->ts_set))
  {
    gced_num = a->free_set_num - 1;
    /* take the the suffix of the list (all collected free_sets) away from the
   free_set list of a and set the correct num of free_sets*/
    fs_cur->set_next = nullptr;
    a->free_set_num = 1;

    /* find the tail for the collected_set list in order to append the new 
   free_sets that were just collected */
    ssmem_free_set_t *collected_set_cur = a->collected_set_list;
    if (collected_set_cur != nullptr)
    {
      while (collected_set_cur->set_next != nullptr)
      {
        collected_set_cur = collected_set_cur->set_next;
      }

      collected_set_cur->set_next = fs_nxt;
    }
    else
    {
      a->collected_set_list = fs_nxt;
    }
    a->collected_set_num += gced_num;
  }

  /* if (gced_num) */
  /*   { */
  /*     printf("//collected %d sets\n", gced_num); */
  /*   } */
  return gced_num;
}

/* 
 *
 */
void ssmem_free(ssmem_allocator_t *a, void *obj, bool flush = true)
{
  ssmem_free_set_t *fs = a->free_set_list;
  if (fs->curr == (long) fs->size)
  {
    fs->ts_set = ssmem_ts_set_collect(fs->ts_set);
    ssmem_mem_reclaim(a);

    /* printf("[ALLOC] free_set is full, doing GC / size of garbage pointers: %10zu = %zu KB\n", garbagep, garbagep / 1024); */
    ssmem_free_set_t *fs_new = ssmem_free_set_get_avail(a, a->fs_size, a->free_set_list);
    a->free_set_list = fs_new;
    a->free_set_num++;
    fs = fs_new;
  }

  fs->set[fs->curr++] = (uintptr_t)obj;
#if SSMEM_TS_INCR_ON == SSMEM_TS_INCR_ON_FREE || SSMEM_TS_INCR_ON == SSMEM_TS_INCR_ON_BOTH
  ssmem_ts_next();
#endif
}

/* 
 *
 */
inline void
ssmem_release(ssmem_allocator_t *a, void *obj)
{
  ssmem_released_t *rel_list = a->released_mem_list;
  ssmem_released_t *rel = ssmem_released_node_new(obj, rel_list);
  rel->ts_set = ssmem_ts_set_collect(rel->ts_set);
  int rn = ++a->released_num;
  a->released_mem_list = rel;
  if (rn >= SSMEM_GC_RLSE_SET_SIZE)
  {
    ssmem_mem_reclaim(a);
  }
}

/* 
 *
 */
static void
ssmem_ts_set_print_no_newline(size_t *set)
{
  printf("[");
  if (set != nullptr)
  {
    for (unsigned int i = 0; i < ssmem_ts_list_len; i++)
    {
      printf("%zu|", set[i]);
    }
  }
  else
  {
    printf(" no timestamp yet ");
  }
  printf("]");
}

/* 
 *
 */
void ssmem_free_list_print(ssmem_allocator_t *a)
{
  printf("[ALLOC] free_set list (%zu sets): \n", a->free_set_num);

  int n = 0;
  ssmem_free_set_t *cur = a->free_set_list;
  while (cur != nullptr)
  {
    printf("(%-3d | %p::", n++, cur);
    ssmem_ts_set_print_no_newline(cur->ts_set);
    printf(") -> \n");
    cur = cur->set_next;
  }
  printf("nullptr\n");
}

/* 
 *
 */
void ssmem_collected_list_print(ssmem_allocator_t *a)
{
  printf("[ALLOC] collected_set list (%zu sets): \n", a->collected_set_num);

  int n = 0;
  ssmem_free_set_t *cur = a->collected_set_list;
  while (cur != nullptr)
  {
    printf("(%-3d | %p::", n++, cur);
    ssmem_ts_set_print_no_newline(cur->ts_set);
    printf(") -> \n");
    cur = cur->set_next;
  }
  printf("nullptr\n");
}

/* 
 *
 */
void ssmem_available_list_print(ssmem_allocator_t *a)
{
  printf("[ALLOC] avail_set list: \n");

  int n = 0;
  ssmem_free_set_t *cur = a->available_set_list;
  while (cur != nullptr)
  {
    printf("(%-3d | %p::", n++, cur);
    ssmem_ts_set_print_no_newline(cur->ts_set);
    printf(") -> \n");
    cur = cur->set_next;
  }
  printf("nullptr\n");
}

/* 
 *
 */
void ssmem_all_list_print(ssmem_allocator_t *a, int id)
{
  printf("[ALLOC] [%-2d] free_set list: %-4zu / collected_set list: %-4zu\n",
       id, a->free_set_num, a->collected_set_num);
}

/* 
 *
 */
void ssmem_ts_list_print()
{
  printf("[ALLOC] ts list (%u elems): ", ssmem_ts_list_len);
  ssmem_ts_t *cur = ssmem_ts_list;
  while (cur != nullptr)
  {
    printf("(id: %-2zu / version: %zu) -> ", cur->id, cur->version);
    cur = cur->next;
  }

  printf("nullptr\n");
}


#endif /* _SSMEM_H_ */

