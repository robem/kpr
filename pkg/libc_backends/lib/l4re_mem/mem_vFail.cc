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
#include <l4/sys/kdebug.h>
#include <l4/sys/capability>
#include <l4/re/dataspace>
#include <l4/re/env>
#include <l4/re/rm>
#include <l4/re/util/cap_alloc>

#define BYTE unsigned char

#define DBG 1

const unsigned BLOCK_SIZE = 32768; // 32KB
const unsigned PAGE_SIZE = 4096;  // 4KB
const unsigned MAP_SIZE = 1; // 1Byte
//const unsigned MAP_SIZE = (BLOCK_SIZE/PAGE_SIZE)>>3;

class Dmem_elem
{
public:
  Dmem_elem *next;
  Dmem_elem *prev;
  BYTE bitmap[MAP_SIZE];

  bool is_zero(int idx);
  void set_bit(int idx);
  void clear_bit(int idx);
};

bool Dmem_elem::is_zero(int idx)
{
  return !( bitmap[0] & (BYTE)(1<<idx) );
}

void Dmem_elem::set_bit(int idx)
{
  bitmap[0] = bitmap[0] | (BYTE)(1<<idx);
}

void Dmem_elem::clear_bit(int idx)
{
  bitmap[0] = bitmap[0] & (BYTE)~(1<<idx);
}

Dmem_elem head = {&head, &head, {(BYTE)0}};

void *extend_mem(unsigned size);
unsigned dec_idx(unsigned idx, unsigned dec, Dmem_elem* runner);
unsigned inc_idx(unsigned idx, unsigned inc, Dmem_elem* runner);

void *malloc(unsigned size) throw()
{
#if DBG
  enter_kdebug("enter malloc");
  printf("### size = %i\n",size);
#endif
  void *addr;
  
  // ensure that size is aligned
  if(size <= PAGE_SIZE)
    size = PAGE_SIZE;
  else
    size += PAGE_SIZE - (size%PAGE_SIZE);

#if DBG
  printf("### aligned size = %i\n",size);
#endif

  Dmem_elem *runner = head.next;
  unsigned zeros=0, idx=0;
  unsigned required_pages = (2+(size/PAGE_SIZE));

  while( runner != &head )
  {
#if DBG
    printf("### old bitmap : ");
    unsigned i=0;
    for(; i<8; ++i)
      printf("%i",(runner->bitmap[0] & 1<<i)?1:0);
    printf("\n");
#endif
    // search for memory
    // exmpl.: 011110 -> 4*PAGE_SIZE
    for(idx=0; idx < (MAP_SIZE*8); ++idx)
    {
      if( runner->is_zero(idx) )
        ++zeros;
      else
        zeros=0;

      if(zeros >= required_pages)
      {
        // MEM found!
   
#if DBG
        printf("### memory found, zeros=%i, idx=%i\n",zeros,idx);
#endif
          
        // go backwards and set bits
        for(zeros-=2; zeros > 0; --zeros)
        {
          idx = dec_idx(idx,1u,runner);
          runner->set_bit(idx);
        }

        addr = (void*)((runner + 1) + (idx*PAGE_SIZE/8));
#if DBG
        printf("### return addr=%8p, runner=%8p, zeros=%i, idx=%i\n",addr,runner,zeros,idx);
        printf("### new bitmap : ");
        unsigned i=0;
        for(; i<8; ++i)
          printf("%i",(runner->bitmap[0] & 1<<i)?1:0);
        printf("\n");
#endif
        return addr;
      }
    }
    runner = runner->next;
  }

#if DBG
  printf("### not enough space -> extend memory\n");
#endif

  idx = dec_idx(idx,zeros,runner);

  do
  {
    runner = (Dmem_elem*) extend_mem(sizeof(Dmem_elem)+BLOCK_SIZE);
    zeros += 8;
  }while( zeros < required_pages );

  // -> enough space is available
  // set correct idx
  idx = inc_idx(idx,required_pages-1,runner);

  // go backwards and set bits
  for(required_pages-=2; required_pages > 0; --required_pages)
  {
    idx = dec_idx(idx,1u,runner);
    runner->set_bit(idx);
  }

  addr = (void*)((runner +1) + (idx*PAGE_SIZE/8));
#if DBG
  printf("### return addr=%8p, runner=%8p, zeros=%i, idx=%i\n",addr,runner,zeros,idx);
  printf("### new bitmap : ");
  unsigned i=0;
  for(; i<8; ++i)
    printf("%i",(runner->bitmap[0] & 1<<i)?1:0);
  printf("\n");
#endif
  return addr;
}

void free(void *addr) throw()
{
#if DBG
  enter_kdebug("free");
#endif

  if( addr < (head.next+1) )
  {
    printf("free 1: %8p isn't a valid address to free\n", addr);
    return;
  }

  // search for the block
  Dmem_elem *runner = head.next;

  do
  {
#if DBG
    printf("### runner=%8p, addr=%8p\n",runner,addr);
#endif
    if( addr < runner
        || ((runner->next == &head) && (addr < (runner+(8*PAGE_SIZE)))) )
    {
#if DBG
      printf("### block found!\n");
#endif

      unsigned idx=0;
      for(; idx < 8; ++idx)
      {
        if( addr == ((runner+1)+(idx*PAGE_SIZE/8)) )
          break;
      }

      if( idx >= 8 )
      {
        printf("free 2: %8p isn't a valid address to free\n", addr);
        return;
      }

#if DBG
        printf("### old bitmap : ");
        unsigned i=0;
        for(; i<8; ++i)
          printf("%i",(runner->bitmap[0] & 1<<i)?1:0);
        printf("\n");
#endif
 
      // CLEAR BITS
      while(!runner->is_zero(idx))
      {
        runner->clear_bit(idx);
        idx=inc_idx(idx,1u,runner);
      }

      // -- and free
      L4::Cap<L4Re::Dataspace> ds
        = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
      ds = (L4::Cap<L4Re::Dataspace>)L4Re::Env::env()->rm()->detach(l4_addr_t(addr), &ds);
      L4Re::Env::env()->mem_alloc()->free(ds);

#if DBG
        printf("### new bitmap : ");
        i=0;
        for(; i<8; ++i)
          printf("%i",(runner->bitmap[0] & 1<<i)?1:0);
        printf("\n");
        printf("successfull freed.\n");
#endif

      return;
    }

    runner = runner->next;

  }while( runner->next != &head );
  
  printf("free 3: %8p isn't a valid address to free\n", addr);
}

void *extend_mem(unsigned size)
{
   L4::Cap<L4Re::Dataspace> ds
    = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();

  if(!ds.is_valid())
  {
    printf("Couldn't get capability for a new dataspace\n");
    return NULL;
  }

  long err = L4Re::Env::env()->mem_alloc()->alloc(size, ds);
  if(err)
  {
    printf("Couldn't allocate extend memory.\n");
    return NULL;
  }

  void *addr = 0;
  err = L4Re::Env::env()->rm()->attach(&addr, size,
      L4Re::Rm::Search_addr, ds, 0);
  if(err) return NULL;

  Dmem_elem *new_elem = (Dmem_elem*) addr;
  new_elem->prev = head.prev;
  new_elem->next = &head;
  new_elem->bitmap[0] = (BYTE)0;
  head.prev->next = new_elem;
  head.prev = new_elem;

  return new_elem;
}

unsigned inc_idx(unsigned idx, unsigned inc, Dmem_elem *runner)
{
  while(inc != 0)
  {
    if(idx == 7)
      runner = runner->next;
    idx = (idx+1)%8;
    --inc;
  }
  return idx;
}

unsigned dec_idx(unsigned idx, unsigned dec, Dmem_elem *runner)
{
  while(dec != 0)
  {
    if(idx == 0)
      runner = runner->prev;
    idx = (idx-1)%8;
    --dec;
  }
  return idx;
}

