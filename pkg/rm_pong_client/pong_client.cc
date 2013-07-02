#include <l4/cxx/iostream>
#include <l4/cxx/l4iostream>
#include <l4/util/util.h>
#include <l4/re/env>
#include <l4/re/namespace>

#include <l4/cxx/exceptions>
#include <l4/cxx/ipc_stream>

#include <l4/re/util/cap_alloc>

#include <stdio.h>
#include <pthread-l4.h>

#include <l4/rm_keyboard/shared.h>
#include <unistd.h>
#include <stdlib.h>
#include <list>

class Paddle
{
public:
  Paddle( int speed, unsigned long svr, unsigned id );
  void run();

  int connect();
  int lifes();
  void move( int pos );

	void up();
	void down();

private:
  unsigned long pad_cap;
  unsigned long svr;
  int speed;
	unsigned id;

	int _pos;
};

class Main
{
public:
  void run();
};

int
Paddle::connect()
{
  L4::Ipc_iostream s(l4_utcb());
  pad_cap = L4Re::Util::cap_alloc.alloc<void>().cap();
  while (1)
  {   
    L4::cout << "PC: connect to " << L4::hex << svr << "\n";
    s << 1UL;
    s << L4::Small_buf(pad_cap);
    l4_msgtag_t err = s.call(svr);
    l4_umword_t fp;
    s >> fp;

    L4::cout << "FP: " << fp <<  " err=" << err << "\n";

    if (!l4_msgtag_has_error(err) && fp != 0)
    {
      L4::cout << "Connected to paddle " << (unsigned)fp << '\n';
      return pad_cap;
    }
    else
    {
      switch (l4_utcb_tcr()->error)
      {
        case L4_IPC_ENOT_EXISTENT:
          L4::cout << "No paddle server found, retry\n";
          l4_sleep(1000);
          s.reset();
          break;
        default:
          L4::cout << "Connect to paddle failed err=0x"
             << L4::hex << l4_utcb_tcr()->error << '\n';
          return l4_utcb_tcr()->error;
      }
    }
  }
  return 0;
}

int Paddle::lifes()
{
  L4::Ipc_iostream s(l4_utcb());
  s << 3UL;
  if (!l4_msgtag_has_error((s.call(pad_cap))))
  {
    int l;
    s >> l;
    return l;
  }

  return -1;
}

void Paddle::move( int pos )
{
  L4::Ipc_iostream s(l4_utcb());
  s << 1UL << pos;
  s.call(pad_cap);
  l4_sleep(10);
}

Paddle::Paddle(int speed, unsigned long svr, unsigned id)
  : svr(svr), speed(speed), id(id)
{
	printf("pong-client %i: ENTER constructor.\n", id);
}

void Paddle::run()
{
  L4::cout << "Pong client running...\n";
  int paddle = connect();
  if (paddle == -1)
    return;

  _pos = 180;

  int c = 0;
  while(1)
  {
    if (c++ >= 500)
    {
			c = 0;
			L4::cout << '(' << pthread_self() << ") Lifes: " << lifes() << '\n';
   	}

    move(_pos);
    if (_pos<0)
    {
      _pos = 0;
    }
    if (_pos>1023)
    {
      _pos = 1023;
    }
	}
}

void Paddle::up() { _pos -= speed; }
void Paddle::down() { _pos += speed; }

static l4_cap_idx_t server()
{
  L4::Cap<void> s = L4Re::Env::env()->get_cap<void>("PongServer");
  if (!s)
    throw L4::Element_not_found();

  return s.cap();
}

Paddle p0(30, server(), 1);
Paddle p1(30, server(), 2);

void *thread_fn(void* ptr)
{
    Paddle *pd = (Paddle*)ptr;
    pd->run();
    return 0;
}

void *thread_kb(void*)
{
	// GET Keyboard CAPABILITY
  L4::Cap<void> kbd = L4Re::Env::env()->get_cap<void>("kbd");
  if (!kbd.is_valid())
  {
    printf("Could not get server capability!\n");
      exit(1);
  }

	int code=0, size=0;
	l4_msgtag_t res;
	L4::Ipc_iostream stream(l4_utcb());
	std::list<unsigned> codes;
	std::list<unsigned>::iterator it;

	while(true)
	{
    res = stream.call(kbd.cap(),Protocol::Keycode);
    if(l4_ipc_error(res,l4_utcb()))
    {
      printf("paddle: ERROR call ipc stream.\n");
      exit(1);
    }

		// get Array Size
		stream >> size;

		for( int i=0; i<size; ++i)
		{
			stream >> code;
			codes.push_front(code);
		}

		for( it=codes.begin(); it != codes.end(); it++)
		{
			switch(*it)
			{
				case Key::UP_1:
					printf("paddle: kbLoop, Key::UP_1 pressed!\n");
					p0.up();
					break;
				case Key::DOWN_1:
					printf("paddle: kbLoop, Key::DOWN_1 pressed!\n");
					p0.down();
					break;
				case Key::UP_2:
					printf("paddle: kbLoop, Key::UP_2 pressed!\n");
					p1.up();
					break;
				case Key::DOWN_2:
					printf("paddle: kbLoop, Key::DOWN_2 pressed!\n");
					p1.down();
					break;
				default:
					break;
			}
		}

		codes.clear();
		usleep(300);
	}
}

void Main::run()
{
  L4::cout << "Hello from pong client\n";

  pthread_t p, q, k;

  pthread_create(&k, NULL, thread_kb, NULL);
  pthread_create(&p, NULL, thread_fn, (void*)&p0);
  pthread_create(&q, NULL, thread_fn, (void*)&p1);

  L4::cout << "PC: main sleep......\n";
  l4_sleep_forever();
}

int main()
{
  Main().run();
  return 0;
};
