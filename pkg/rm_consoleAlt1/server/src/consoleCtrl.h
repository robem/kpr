#ifndef CONSOLECTRL_H
#define CONSOLECTRL_H

#include <l4/re/util/video/goos_fb>
#include <pthread-l4.h>

#define NUM_CONSOLES 2

struct console
{
 void *addr;	
 L4::Cap<L4Re::Dataspace> ds;
 bool active;
};

class ConsoleCtrl
{
	private:
		L4Re::Util::Video::Goos_fb *_fb;
		void *_fb_addr;
		unsigned _fb_size;
		struct console consoles[NUM_CONSOLES];
		pthread_t thread_refresh, thread_switch;
	public:
		ConsoleCtrl();
		void set_fb(L4Re::Util::Video::Goos_fb *_fb);
		L4::Cap<L4Re::Dataspace> get_ds(unsigned id);

		void refresh_loop();
		void switch_loop();
};

#endif
