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

#define DBG 0

const unsigned EXT_MEM = 32768; // 32KByte, 2^15
//const unsigned EXT_MEM = 33554432; // 32MByte, 2^25
const unsigned SIZE_INT = sizeof(int);
const unsigned ALIGN = 0x8; //8Byte

class Dmem_elem
{
// 12Byte
public:
  unsigned size;
  Dmem_elem *prev;
  Dmem_elem *next;
};

// Head for the Processes List
Dmem_elem *processes_head = NULL;
// Head for the Holes List
Dmem_elem *holes_head = NULL;

void *extend_mem();
void split(Dmem_elem *elem, unsigned size);
Dmem_elem *align(Dmem_elem *elem);
void insert_into_processes(Dmem_elem *elem);
void insert_into_holes(Dmem_elem *elem);
void remove_from_processes(Dmem_elem *elem);
void remove_from_holes(Dmem_elem *elem);
#if DBG
void show_lists();
#endif

void *malloc(unsigned size) throw()
{
	//printf("*** ENTER malloc!\n");
#if DBG
  enter_kdebug("enter malloc");
  printf("### malloc: size=%d, sizeof(Dmem_elem)=%d\n",size,sizeof(Dmem_elem));
#endif
  
  // ensure that size is aligned
  if(size <= SIZE_INT)
    size = SIZE_INT;
  else if( (size%SIZE_INT) != 0)
    size += SIZE_INT - (size%SIZE_INT);

#if DBG
  printf("### malloc: aligned size = %d Byte\n",size);
#endif

  if( holes_head == NULL )
  {
#if DBG
    printf("### malloc: create holes_head\n");
#endif
    // initialze Holes-Head Element
    holes_head = align((Dmem_elem*) extend_mem());
    //holes_head = (Dmem_elem*) extend_mem();
    holes_head->size = EXT_MEM;
    holes_head->next = holes_head;
    holes_head->prev = holes_head;
  }

  Dmem_elem *runner = holes_head;

  do 
  {
    if( runner->size >= size )
    {
#if DBG
      printf("### malloc: elem found %8p with %d Byte\n",runner,runner->size);
#endif
      // Element found in Hole-List
      if( (runner->size - size) > sizeof(Dmem_elem) )
      {
        // split is possible 
        split(runner, size);
#if DBG
        printf("### malloc: splitted in %d and %d Bytes\n", 
            runner->size, runner->next->size);
#endif
      }

      remove_from_holes(runner);

      // add runner to Processes-List
      if(processes_head == NULL)
      {
#if DBG
        printf("### malloc: create processes_head\n");
#endif
        processes_head        = runner;
        processes_head->next  = runner;
        processes_head->prev  = runner;
      }
      else
      {
        insert_into_processes(runner);
      }

#if DBG
      printf("### malloc: return addr %8p\n", (runner+1));
      show_lists();
#endif
      return (void*)(runner +1);
    }
    runner = runner->next;
  } while( runner != holes_head );

  // extend memory
  Dmem_elem *ext_mem = align((Dmem_elem*) extend_mem());
  //Dmem_elem *ext_mem = (Dmem_elem*) extend_mem();
  ext_mem->size = EXT_MEM;
  ext_mem->next = ext_mem;
  ext_mem->prev = ext_mem;

  if( (ext_mem->size - size) > sizeof(Dmem_elem) )
  {
    // split is possible 
    split(ext_mem, size);
    insert_into_holes(ext_mem->next);
  }
	//else if 1 extend is not enough, add new and merge
  //TODO: merge chunks, which are in series

  insert_into_processes(ext_mem);

#if DBG
  printf("### malloc: extend memory, return addr=%8p\n",(ext_mem+1));
  show_lists();
#endif
  return (void*)(ext_mem +1);
}

void free(void *addr) throw()
{
	//printf("*** ENTER free!\n");
#if DBG
  enter_kdebug("enter free");
  printf("### free: %8p\n",addr);
#endif

  Dmem_elem *elem = ((Dmem_elem*) addr) -1;

  remove_from_processes(elem);

  // add elem into Holes-List
  if( holes_head == NULL )
  {
    holes_head = elem;
    holes_head->next = holes_head;
    holes_head->prev = holes_head;
  }
  else
  {
    insert_into_holes(elem);
  }
#if DBG
  show_lists();
#endif 
  
  //TODO: merge chunks, which are in series
}

void *extend_mem()
{
  L4::Cap<L4Re::Dataspace> ds
    = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();

  if(!ds.is_valid())
  {
    printf("Couldn't get capability for a new dataspace\n");
    return NULL;
  }

  long err = L4Re::Env::env()->mem_alloc()->alloc(EXT_MEM+sizeof(Dmem_elem), ds);
  if(err)
  {
    printf("Couldn't allocate extend memory.\n");
    return NULL;
  }

  void *addr = 0;
  err = L4Re::Env::env()->rm()->attach(&addr, EXT_MEM+sizeof(Dmem_elem),
      L4Re::Rm::Search_addr, ds, 0);
  if(err) return NULL;

  return addr;
}

void split(Dmem_elem *elem, unsigned size)
{
  Dmem_elem *new_elem = align((Dmem_elem*) (((char*)(elem+1)) + size));
  //Dmem_elem *new_elem = (Dmem_elem*) (((char*)(elem+1)) + size);

  new_elem->size = elem->size - size - sizeof(Dmem_elem);
  new_elem->prev = elem;
  new_elem->next = elem->next;

  elem->next->prev = new_elem;

  elem->size = size;
  elem->next = new_elem;
}

Dmem_elem *align(Dmem_elem *elem)
{
  // the return addr is aligned 
  // the header addr is NOT
  void *ret = (void*) (elem+1);

  if( ((unsigned)ret) % ALIGN )
  {
    ret += ALIGN - ((unsigned)ret % ALIGN);
  }

  return (((Dmem_elem*) ret) -1);
}

void insert_into_processes(Dmem_elem* elem)
{
  // Processes-List is sort by address
  Dmem_elem *runner = processes_head;
  while( runner->next != processes_head && runner < elem )
  {
    runner = runner->next;
  }

  // insert elem before runner
  elem->next          = runner;
  elem->prev          = runner->prev;
  runner->prev->next  = elem;
  runner->prev        = elem;

  if( runner == processes_head && runner > elem )
  {
    processes_head = elem;
  }
  else if( runner->next == processes_head && runner < elem )
  {
    // end of list
    // exchange elem and runner
    elem->next          = runner->next;
    runner->prev        = elem->prev;
    runner->next->prev  = elem;
    runner->next        = elem;
    elem->prev->next    = runner;
    elem->prev          = runner;
  }
}

void insert_into_holes(Dmem_elem* elem)
{
  Dmem_elem *runner = holes_head;
  while( runner->next != holes_head && runner->size <= elem->size )
  {
    runner = runner->next;
  }

  // insert elem before runner
  elem->next          = runner;
  elem->prev          = runner->prev;
  runner->prev->next  = elem;
  runner->prev        = elem;

  if( runner == holes_head && runner->size > elem->size )
  {
    holes_head = elem;
  }
  else if( runner->next == holes_head && runner->size <= elem->size )
  {
    // end of list
    // exchange elem and runner
    elem->next          = runner->next;
    runner->prev        = elem->prev;
    runner->next->prev  = elem;
    runner->next        = elem;
    elem->prev->next    = runner;
    elem->prev          = runner;
  }
}

void remove_from_processes(Dmem_elem *elem)
{
  if( elem == processes_head && elem->next == processes_head )
  {
    processes_head = NULL;
  }
  else
  {
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
    if( elem == processes_head )
    {
      processes_head = elem->next;
    }
  }
  elem->next = NULL;
  elem->prev = NULL;
}

void remove_from_holes(Dmem_elem *elem)
{
  if( elem == holes_head && elem->next == holes_head )
  {
    holes_head = NULL;
  }
  else
  {
    elem->prev->next = elem->next;
    elem->next->prev = elem->prev;
    if( elem == holes_head )
    {
      holes_head = elem->next;
    }
  }
  elem->next = NULL;
  elem->prev = NULL;
}

void show_lists()
{
  if( processes_head == NULL )
    printf("### phead: NULL");
  else
  {
    printf("### phead: %8p: ", processes_head);
    printf("%p->", processes_head);
    printf("%p->", processes_head->next);
    printf("%p->", processes_head->next->next);
  }

  if( holes_head == NULL )
    printf("\n### hhead: NULL: ");
  else
  {
    printf("\n### hhead: %8p: ", holes_head);
    printf("%p->", holes_head);
    printf("%p->", holes_head->next);
    printf("%p->", holes_head->next->next);
  }

  printf("\n");
}
