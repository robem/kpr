#include <stdio.h>

#include <l4/re/util/object_registry>
#include <l4/re/util/meta>
#include <l4/re/util/video/goos_fb>
#include <l4/re/util/video/goos_svr>

#include "./consoleCtrl.h"

#define DBG 0

static L4Re::Util::Registry_server<> server;
static ConsoleCtrl consoleCtrl;

class FactoryServer : public L4::Server_object
{
	private:
  	L4Re::Util::Video::Goos_fb *_fb;
		unsigned id_ctrl;
	public:
		FactoryServer();
 	 	int dispatch(l4_umword_t, L4::Ipc_iostream& ios);
};

class ConsoleAlt1 : public L4Re::Util::Video::Goos_svr, 
                    public L4::Server_object
{
  private:
		unsigned _id;

  public:
    ConsoleAlt1(unsigned id, L4Re::Util::Video::Goos_fb *_fb);
    int dispatch(l4_umword_t obj, L4::Ipc_iostream &ios);
};

/* === BEGIN FACTORY === */

FactoryServer::FactoryServer()
{
#if DBG
	printf("consoleAlt1-Factory: ENTER constructor\n");
#endif

	_fb = new L4Re::Util::Video::Goos_fb("fb");
	consoleCtrl.set_fb(_fb);

	id_ctrl = 0;

#if DBG
	printf("consoleAlt1-Factory: LEAVE constructor\n");
#endif
}

int FactoryServer::dispatch(l4_umword_t, L4::Ipc_iostream& ios)
{
#if DBG
  printf("consoleAlt1-Factory: ENTER dispatch\n");
#endif
  l4_msgtag_t tag;
  ios >> tag;

  switch(tag.label())
  {
    case L4::Meta::Protocol:
#if DBG
  		printf("consoleAlt1-Factory: LEAVE dispatch, Meta-Protocol\n");
#endif
      return L4Re::Util::handle_meta_request<L4::Factory>(ios);
    case L4::Factory::Protocol:
		{
			unsigned op;
			ios >> op;
			if(op != 0){
  			printf("consoleAlt1-Factory: LEAVE dispatch, Factory-Protocol Opcode != 0\n");
				return -L4_EINVAL;
			}

			// create ConsoleAlt1
			ConsoleAlt1* consoleAlt1 = new ConsoleAlt1(id_ctrl++,_fb);
			server.registry()->register_obj(consoleAlt1);
			ios << consoleAlt1->obj_cap();
#if DBG
 	printf("consoleAlt1-Factory: LEAVE dispatch, Factory-Protocol OK\n");
#endif
			return L4_EOK;
		}
    default:
      printf("consoleAlt1-Factory: strange TagLabel!\n");
      break;
  }

#if DBG
  printf("consoleAlt1-Factory: LEAVE dispatch\n");
#endif
  return 0;
}
 
/* === END FACTORY === */

/* === BEGIN CONSOLEALT1 === */

ConsoleAlt1::ConsoleAlt1(unsigned id, L4Re::Util::Video::Goos_fb *_fb)
{
#if DBG
  printf("consoleAlt1: ENTER constructor, id %i\n",id);
#endif

	_id = id;

  //L4Re::Util::Video::Goos_fb _fb("fb");
	
  _fb->view_info(&_view_info);

  _fb->goos()->info(&_screen_info);

	//_fb_ds = _fb->buffer();
	_fb_ds = consoleCtrl.get_ds(_id);


#if DBG
  printf("consoleAlt1: LEAVE constructor, id %i\n",_id);
#endif
}

int ConsoleAlt1::dispatch(l4_umword_t, L4::Ipc_iostream &ios)
{
#if DBG
  printf("consoleAlt1: ENTER dispatch, id %i\n",_id);
#endif
  //Goos_svr::dispatch(obj,ios);
  l4_msgtag_t tag;
  ios >> tag;

  if (tag.label() != L4Re::Protocol::Goos)
	{
  	printf("consoleAlt1: LEAVE dispatch, id %i, Bad Protocol.\n",_id);
    return -L4_EBADPROTO;
	}

  L4::Opcode op;
  ios >> op;
  switch (op)
  {
  	case L4Re::Video::Goos_::View_info:
		{
			unsigned idx;
			ios >> idx;
			if (idx != 0)
			{
				printf("consoleAlt1: LEAVE dispatch, id %i, Range Error(View Info).\n",_id);
				return -L4_ERANGE;
			}
		}
			ios.put(_view_info);
			printf("consoleAlt1: LEAVE dispatch, id %i, OK(View Info).\n",_id);
			return L4_EOK;
		case L4Re::Video::Goos_::Info:
			ios.put(_screen_info);
			printf("consoleAlt1: LEAVE dispatch, id %i, OK(Info).\n",_id);
			return L4_EOK;
		case L4Re::Video::Goos_::Get_buffer:
		{
			unsigned idx;
			ios >> idx;
			if (idx != 0)
			{
				printf("consoleAlt1: LEAVE dispatch, id %i, Range Error(Buffer).\n",_id);
				return -L4_ERANGE;
			}
		}
			ios << L4::Ipc::Snd_fpage(_fb_ds, L4_CAP_FPAGE_RW);
			printf("consoleAlt1: LEAVE dispatch, id %i, OK(Buffer).\n",_id);
			return L4_EOK;
		case L4Re::Video::Goos_::View_refresh:
		{
			unsigned idx;
			int x,y,w,h;
			ios >> idx >> x >> y >> w >> h;
			if (idx != 0)
			{
				printf("consoleAlt1: LEAVE dispatch, id %i, Range Error(Refresh).\n",_id);
				return -L4_ERANGE;
			}

			printf("consoleAlt1: LEAVE dispatch, id %i, refresh.\n",_id);
			return refresh(x, y, w, h);
		}
		default:
			printf("consoleAlt1: LEAVE dispatch, id %i, No sys.\n",_id);
			return -L4_ENOSYS;
  }

#if DBG
  printf("consoleAlt1: LEAVE dispatch, id %i, OK.\n",_id);
#endif
  return L4_EOK;
}

/* === END CONSOLEALT1 === */

int main(void)
{
  static FactoryServer consoleAlt1_Factory;

  // register consoleAlt1 server
  if (!server.registry()->register_obj(&consoleAlt1_Factory, "consoleAlt1").is_valid())
  {
    printf("consoleAlt1: Could not register consoleAlt1-Factory service, readonly namespace?");
    return 1;
  }

  printf("consoleAlt1: starting consoleAlt1-Factory server loop.\n");

  // wait for client request
  server.loop();

  return 0;
}

