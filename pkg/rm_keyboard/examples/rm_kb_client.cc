#include <stdio.h>
#include <l4/rm_keyboard/shared.h>

#include <l4/re/env>
#include <l4/cxx/ipc_stream>
#include <list>
#include <unistd.h>

int main(void)
{
  L4::Cap<void> kb_cap = L4Re::Env::env()->get_cap<void>("kb_server");
	int size=0, code=0;
	std::list<unsigned> codes;

  L4::Ipc_iostream stream(l4_utcb());

  while(1)
  {
    stream << l4_umword_t(Opcode::get_code);
		//printf("keyborad-client: BEFORE stream call.\n");
    l4_msgtag_t res = stream.call(kb_cap.cap(),Protocol::Keycode);
		//printf("keyborad-client: AFTER stream call.\n");
    if(l4_ipc_error(res,l4_utcb()))
    {
      printf("Error: call ipc stream.\n");
      return 1;
    }

    stream >> size;
		printf("keyboard-client: size %d.\n",size);

		codes.clear();

		for( int i=0; i<size; ++i)
		{
			stream >> code;
			codes.push_front(code);
		}

		//printf("Keys: ");
		std::list<unsigned>::iterator it;
		for( it=codes.begin(); it != codes.end(); it++)
		{
			if( (*it) == Key::TAB ||
					(*it) == Key::UP_1 ||
					(*it) == Key::DOWN_1 ||
					(*it) == Key::UP_2 ||
					(*it) == Key::DOWN_2 
				)
				//printf("key-client: GOT A NICE KEY!\n");
				printf(".");
		}

		codes.clear();
		//printf("\n");
		usleep(100);
  }

  return 0;
}
