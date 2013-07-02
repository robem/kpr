/**
 * \file   libc_backends/l4re_mem/mem.cc
 */
/*
 * (c) 2004-2009 Technische Universität Dresden
 * This file is part of TUD:OS and distributed under the terms of the
 * GNU Lesser General Public License 2.1.
 * Please see the COPYING-LGPL-2.1 file for details.
 */
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <l4/sys/kdebug.h>
#include <l4/sys/capability>
#include <l4/re/dataspace>
#include <l4/re/env>
#include <l4/re/rm>
#include <l4/re/util/cap_alloc>

#define DBG 0

const unsigned MEM_SIZE = 128000; // 128KB
const unsigned PAGE_SIZE= sizeof(unsigned);
const unsigned BITS     = MEM_SIZE/PAGE_SIZE;
const unsigned BMP_SIZE = (BITS/PAGE_SIZE); 
// 128KB = BITS*32Byte
// => 4096 in bitmap 

unsigned bitmap[BMP_SIZE];
unsigned *start_addr=NULL;

void *init();

void *malloc(unsigned size) throw()
{
#if DBG
  enter_kdebug("### enter malloc");
  printf("### malloc: size = %i\n",size);
#endif

  if(start_addr == NULL)
    if(init() == NULL)
      return NULL;
  
  if(size <= PAGE_SIZE)
    size = PAGE_SIZE;
  else
    size += (PAGE_SIZE - (size%PAGE_SIZE));

  assert((size%PAGE_SIZE) == 0);

#if DBG
  printf("### malloc: aligned size = %i\n",size);
#endif

  unsigned zeros=0;
  unsigned required_pages = (1+(size/PAGE_SIZE));

  // search for memoryspace
  for(unsigned bit=0; bit < BITS; ++bit)
  {
    if(bitmap[bit/32] & (1u<<(bit%32)))
      zeros = 0;
    else
      ++zeros;

    if(zeros >= required_pages)
    {
      // space found
#if DBG
      printf("### malloc: memory found, zeros=%i\n",zeros);
#endif
      // go backwards and set bits
      for(; zeros > 0; --zeros)
      {
        bitmap[bit/32] |= (1u<<(bit%32));
        --bit;
      }

      *(start_addr+(++bit)) = size;
#if DBG
      printf("### malloc: return addr=%8p, size %i written to %8p\n", \
          (start_addr+bit+1u),*(start_addr+bit),start_addr+bit);
#endif

      return (start_addr+bit+1u);
    }
  }

  printf("##! malloc: not enough space\n");
  return NULL;
}

void free(void *addr) throw()
{
#if DBG
  enter_kdebug("free");
#endif

  if(addr < start_addr || addr > &bitmap[BMP_SIZE-1])
  {
    printf("### free: %8p isn't a valid address to free\n", addr);
    return;
  }

  unsigned *iter = (unsigned*) addr;
  unsigned size = *(iter-1u);

  // which bit?
  unsigned bit = (start_addr-(iter-1u));

#if DBG
  printf("### free: found size=%i @%8p, bit=%i\n",size,(iter-1u),bit);
  printf("### free: bitmap[%i]=%i\n",bit/32,bitmap[bit/32]);
#endif

  for(unsigned i=0;i<=(size/32);++i)
  {
    bitmap[bit/32] &= ~(1u<<(bit%32));
    ++bit;
  }

#if DBG
  printf("### free: bitmap[%i]=%i\n",bit/32,bitmap[bit/32]);
#endif

  return;
}

void *init()
{
#if DBG
    printf("### init: no memory to manage\n");
#endif
    // init bitmap
    for(unsigned i=0;i<BMP_SIZE;++i)
      bitmap[i] = 0u;

    // allocate MEM
    L4::Cap<L4Re::Dataspace> ds
      = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();

    if(!ds.is_valid())
    {
      printf("##! init: Couldn't get capability for a new dataspace\n");
      return NULL;
    }

    long err = L4Re::Env::env()->mem_alloc()->alloc(MEM_SIZE, ds);
    if(err)
    {
      printf("##! init: Couldn't allocate extend memory.\n");
      return NULL;
    }

    void *addr = 0;
    err = L4Re::Env::env()->rm()->attach(&addr, MEM_SIZE,
        L4Re::Rm::Search_addr, ds, 0);
    if(err) return NULL;

    start_addr = (unsigned*) addr;
#if DBG
    printf("### init: start_addr is set to %8p\n",start_addr);
#endif
    return (void*)1;
}

