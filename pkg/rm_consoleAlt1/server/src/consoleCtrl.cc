#include "./consoleCtrl.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread-l4.h>
#include <unistd.h>

#include <cstring>
#include <list>

#include <l4/sys/capability>
#include <l4/re/dataspace>
#include <l4/re/util/cap_alloc>
#include <l4/re/env>
#include <l4/re/rm>
#include <l4/cxx/ipc_stream>

#include <l4/rm_keyboard/shared.h>

#define DBG 0

static void * fun_refresh(void* ptr)
{
#if DBG
	printf("consoleCtrl: ENTER fun_refresh.\n");
#endif
	ConsoleCtrl *cc = (ConsoleCtrl*) ptr;
	sleep(5);
	cc->refresh_loop();
	return NULL;
}

void ConsoleCtrl::refresh_loop()
{
#if DBG
	printf("consoleCtrl: ENTER refresh_loop.\n");
#endif
	unsigned id;
	while(true)
	{
		id = 0;
		if( !consoles[id].active )
			id = 1;

#if DBG
		printf("consoleCtrl: refresh id %i.\n",id);
#endif
		memcpy(_fb_addr, consoles[id].addr, _fb_size);

		usleep(20);
	}
}

static void * fun_switch(void* ptr)
{
#if DBG
	printf("consoleCtrl: ENTER fun_switch.\n");
#endif
	ConsoleCtrl *cc = (ConsoleCtrl*) ptr;
	sleep(5);
	cc->switch_loop();
	return NULL;
}

void ConsoleCtrl::switch_loop()
{
#if	DBG
	printf("consoleCtrl: ENTER switch_loop.\n");
#endif

	// GET Keyboard CAPABILITY
  L4::Cap<void> kbd = L4Re::Env::env()->get_cap<void>("kbd");
  if( !kbd.is_valid() )
  {
    printf("Could not get server capability!\n");
      exit(1);
  }

	int code=0, size=0;
	bool last_tab=false;
	std::list<unsigned> codes;
	L4::Ipc_iostream stream(l4_utcb());
	l4_msgtag_t res;
	std::list<unsigned>::iterator it;

	while( true )
	{
    res = stream.call(kbd.cap(),Protocol::Keycode);
    if( l4_ipc_error(res,l4_utcb()) )
    {
      printf("consoleCtrl: ERROR call ipc stream.\n");
      exit(1);
    }

		// get Array Size
		stream >> size;
#if	DBG
		//printf("consoleCtrl: size %d.\n",size);
#endif

		codes.clear();

		for( int i=0; i<size; ++i )
		{
			stream >> code;
			codes.push_front(code);
		}

		if( !size ) last_tab = false;
		else
		{
			for( it=codes.begin(); it != codes.end(); ++it)
			{
				if( (*it) == Key::TAB && !last_tab )
				{
					printf("consoleCtrl: TAB pressed.\n");
					if( consoles[0].active ) {
						consoles[0].active = false;
						consoles[1].active = true;
					}

					else if( consoles[1].active ) {
						consoles[0].active = true;
						consoles[1].active = false;
					}
					break;
				}
			}
		
			// check if one Key is TAB
			for( it=codes.begin(); it != codes.end(); ++it)
			{
				if( (*it) == Key::TAB )
				{
					last_tab = true;
					break;
				}
				else
					last_tab = false;
			}
		}

		usleep(500);
	}
}

ConsoleCtrl::ConsoleCtrl()
{
#if DBG
	printf("consoleCtrl: ENTER constructor.\n");
#endif

	pthread_create(&thread_switch, NULL, fun_switch, this);
	pthread_create(&thread_refresh, NULL, fun_refresh, this);

#if DBG
	printf("consoleCtrl: LEAVE constructor.\n");
#endif
}

void ConsoleCtrl::set_fb(L4Re::Util::Video::Goos_fb *_fb) 
{	
#if DBG
	printf("consoleCtrl: set fb\n");
#endif

	this->_fb = _fb; 
	_fb_addr = _fb->attach_buffer();
	_fb_size = _fb->buffer()->size();
}

L4::Cap<L4Re::Dataspace> ConsoleCtrl::get_ds(unsigned id)
{
#if DBG
	printf("consoleCtrl: ENTER get dataspace, id %i\n", id);
#endif

	unsigned size = _fb->buffer()->size();

	L4::Cap<L4Re::Dataspace> ds
    = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();

  if(!ds.is_valid())
  {
    printf("consoleCtrl: Couldn't get capability for a new dataspace\n");
		exit(1);
	}

  long err = L4Re::Env::env()->mem_alloc()->alloc(size, ds);
  if(err)
  {
    printf("consoleCtrl: Couldn't allocate memory.\n");
		exit(1);
  }

  void *addr = 0;
  err = L4Re::Env::env()->rm()->attach(&addr, size,
      L4Re::Rm::Search_addr, ds, 0);
  if(err)
  {
    printf("consoleCtrl: Couldn't attach memory.\n");
		exit(1);
  }

	consoles[id].addr = addr;
	consoles[id].ds = ds;
	if( id == 0 ) consoles[id].active = true;
	else consoles[id].active = false;

#if DBG
	printf("consoleCtrl: fb size %i, addr %p, active %i\n",size,addr,consoles[id].active?1:0);
	printf("consoleCtrl: LEAVE get dataspace.\n");
#endif

  return ds;
}
