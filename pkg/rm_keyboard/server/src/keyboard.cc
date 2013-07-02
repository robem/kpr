#include <stdio.h>
#include <assert.h>

#include <l4/sys/types.h>
#include <l4/sys/irq>
//#include <l4/sys/thread>
#include <l4/re/util/object_registry>
#include <l4/re/util/cap_alloc>
#include <l4/re/env>
#include <l4/util/port_io.h>
#include <l4/io/io.h>
#include <pthread-l4.h>

#include <list>

#include <l4/rm_keyboard/shared.h>

#define DBG 0

static L4Re::Util::Registry_server<> server;

class Keyboard : public L4::Server_object
{
private:
  L4::Cap<L4::Irq> irq_cap;
  std::list<unsigned> currently_pressed;
	pthread_t thread;
public:
  Keyboard();
  int dispatch(l4_umword_t obj, L4::Ipc_iostream &ios);
  std::list<unsigned> get_key();
	void loop();
};

static void * fun(void* ptr)
{
	Keyboard *kb = (Keyboard*) ptr;
	kb->loop();
	return NULL;
}

Keyboard::Keyboard()
{
#if DBG
  printf("keyboard: ENTER constructor\n");
#endif
  
  irq_cap = L4Re::Util::cap_alloc.alloc<L4::Irq>();
  assert(irq_cap.is_valid());

  long res = l4io_request_irq(0x1, irq_cap.cap());
  assert( res == 0 );

	// start thread
	pthread_create(&thread, NULL, fun, this);
  //L4::Cap<L4::Thread> thread_cap(pthread_getl4cap(pthread_self()));
  L4::Cap<L4::Thread> thread_cap(pthread_getl4cap(thread));
  l4_msgtag_t t = irq_cap->attach((l4_umword_t)0x1, thread_cap);
  assert(! l4_ipc_error(t,l4_utcb()) );

#if DBG
  printf("keyboard: LEAVE constructor\n");
#endif
}

int Keyboard::dispatch(l4_umword_t, L4::Ipc_iostream &ios)
{
#if DBG
  printf("keyboard: ENTER dispatch.\n");
#endif

  l4_msgtag_t t;
  ios >> t;

  if( t.label() != Protocol::Keycode )
	{
		printf("keyboard: LEAVE dispatch, Bad Protocol.\n");
    return -L4_EBADPROTO;
	}

	ios << currently_pressed.size();

	std::list<unsigned>::iterator it;
	for( it=currently_pressed.begin(); it != currently_pressed.end(); it++)
	{
		ios << (*it);
	}

#if DBG
  printf("keyboard: LEAVE dispatch.\n");
#endif
  return L4_EOK;
}

std::list<unsigned> Keyboard::get_key()
{
#if DBG
	printf("keyboard: ENTER get key.\n");
#endif

  return currently_pressed;
}

void Keyboard::loop()
{
	int err;
	unsigned buf;
	std::list<unsigned>::iterator it;

	while(true)
	{
		err=0;
		buf=0;

		l4_msgtag_t t = irq_cap->receive();
		assert(! l4_ipc_error(t,l4_utcb()));

		if((err=l4io_request_ioport(0x60,1u)) < 0)
		{
			printf("keyboard: request ioprt 0x20 error: %i\n",err);
		}
		buf = (unsigned)l4util_in8(0x60);

		//printf("keyboard: received code %d\n",buf);

		switch(buf)
		{ 
			case 15: // tab
#if DBG
				printf("keyboard: LEAVE get key, TAB\n");
#endif
				currently_pressed.push_front(Key::TAB);
				break;
			case 30: // a
#if DBG
				printf("keyboard: LEAVE get key, DOWN_1\n");
#endif
				currently_pressed.push_front(Key::DOWN_1);
				break;
			case 31: // s
#if DBG
				printf("keyboard: LEAVE get key, UP_1\n");
#endif
				currently_pressed.push_front(Key::UP_1);
				break;
			case 38: // l
#if DBG
				printf("keyboard: LEAVE get key, DOWN_2\n");
#endif
				currently_pressed.push_front(Key::DOWN_2);
				break;
			case 39: // oe
#if DBG
				printf("keyboard: LEAVE get key, UP_2\n");
#endif
				currently_pressed.push_front(Key::UP_2);
				break;
			default:
#if DBG
				printf("keyboard: LEAVE get key, Default\n");
#endif
				break;
		}

		// erase Key's if they're released
		for( it=currently_pressed.begin(); it != currently_pressed.end(); it++)
		{
			if( buf == 143 && (*it) == Key::TAB )
				currently_pressed.erase(it);
			else if( buf == 158 && (*it) == Key::DOWN_1)
				currently_pressed.erase(it);
			else if( buf == 159 && (*it) == Key::UP_1)
				currently_pressed.erase(it);
			else if( buf == 166 && (*it) == Key::DOWN_2)
				currently_pressed.erase(it);
			else if( buf == 167 && (*it) == Key::UP_2)
				currently_pressed.erase(it);
		}

		// remove all doubles
		currently_pressed.unique();
	}
}

int main(void)
{
  printf("keyboard: enter main.\n");
  static Keyboard keyboard;

  if (!server.registry()->register_obj(&keyboard, "kbd").is_valid())
  {
    printf("keyboard: Could not register keyboard, readonly namespace?\n");
    return 1;
  }
  printf("keyboard: service registered, starting server loop.\n");
  
  // wait for client request
  server.loop();

  return 0;
}

