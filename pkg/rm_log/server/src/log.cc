#include <stdio.h>
#include <assert.h>

#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/re/util/dataspace_svr>
#include <l4/re/util/video/goos_fb>
#include <l4/re/video/colors>
#include <l4/libgfxbitmap/bitmap.h>
#include <l4/libgfxbitmap/font.h>

#include <cstring>
#include <string>
#include <list>

#define DBG 0

class Log : public L4::Server_object
{
  private:
    L4Re::Util::Video::Goos_fb _fb; 
    void* _fb_addr;
    L4Re::Video::View::Info _fb_info;

    gfxbitmap_color_pix_t _fg_color, _bg_color;
    unsigned _last_line, _font_height;

    //std::list<std::string> *history;

  public:
    Log();
    int dispatch(l4_umword_t obj, L4::Ipc_iostream &ios);
    void print_log();
};

Log::Log() : _fb("fb")
{
#if DBG
  printf("log: ENTER constructor.\n");
#endif

  //_fb("fb");

  if(_fb.view_info(&_fb_info))
  {
    printf("log: cant get view_info!\n");
  }

  // get fb address
  if(!(_fb_addr=_fb.attach_buffer()))
  {
    printf("log: cant get base address.\n");
  }

  // gfx
  gfxbitmap_font_init();

  _last_line   = _fb_info.height-gfxbitmap_font_height(GFXBITMAP_DEFAULT_FONT);
  _fg_color    = gfxbitmap_convert_color(reinterpret_cast<l4re_video_view_info_t*>(&_fb_info),0xff00b3);
  _bg_color    = gfxbitmap_convert_color(reinterpret_cast<l4re_video_view_info_t*>(&_fb_info),0x000000);
  _font_height = gfxbitmap_font_height(GFXBITMAP_DEFAULT_FONT);

  // initial clear
	memset(_fb_addr, 0x0, _fb.buffer()->size());

  //history = new std::list<std::string>();
	
#if DBG
	printf("log: fb addr %p, size of fb %li\n",_fb_addr,_fb.buffer()->size());
  printf("log: gfx font_init(), fill console.\n");
  printf("log: LEAVE constructor.\n");
#endif
}

int Log::dispatch(l4_umword_t, L4::Ipc_iostream &ios)
{
#if DBG
  printf("log: ENTER dispatch.\n");
#endif

  l4_msgtag_t t;
  ios >> t;

  // Log Msg?
  if (t.label() != L4_PROTO_LOG){
  	printf("log: LEAVE dispatch, Bad Protocol %li.\n", t.label());
    return -L4_EBADPROTO;
	}

  l4_umword_t op;
  ios >> op;

	//printf("log dispatch: size fb %i\n",_fb.buffer()->size());

  switch(op)
  {
    case L4_VCON_WRITE_OP:
			//printf("log dispatch: size fb %li\n",_fb.buffer()->size());
      print_log();
      break;
    case L4_VCON_SET_ATTR_OP:
    case L4_VCON_GET_ATTR_OP:
    default:
      printf("log: Unsupported.\n");
      break;
  }

#if DBG
  printf("log: LEAVE dispatch, OK.\n");
#endif
  return L4_EOK;
}

void Log::print_log()
{
#if DBG
  printf("log: ENTER print_log.\n");
#endif
  std::string str(reinterpret_cast<char*>(&l4_utcb_mr()->mr[2]));
  //history->push_front(str);
	
	// replace special characters
	for(unsigned i=0; i < str.length(); ++i)
	{
		if( !(str[i] < 127 && str[i] > 32) )
			str[i] = ' ';
	}
  
#if DBG
  printf("to log: ");
	printf("%s",str.c_str());
#endif

  // scroll downwards
  gfxbitmap_copy( (l4_uint8_t*)_fb_addr,
                  (l4_uint8_t*)_fb_addr,
                  reinterpret_cast<l4re_video_view_info_t*>(&_fb_info),
                  0u, _font_height,
                  _fb_info.width, _fb_info.height,
                  0u, 0u);

	// clear last line
	gfxbitmap_fill( (l4_uint8_t*)_fb_addr,
								reinterpret_cast<l4re_video_view_info_t*>(&_fb_info),
								0u, _last_line,
								_fb_info.width, _fb_info.height,
								0x000000);

  // render log msg to last line
  gfxbitmap_font_text(  (void*) _fb_addr,
                        reinterpret_cast<l4re_video_view_info_t*>(&_fb_info),
                        GFXBITMAP_DEFAULT_FONT,
                        str.c_str(), // text
                        GFXBITMAP_USE_STRLEN,
                        0u, _last_line,
                        _fg_color,
                        _bg_color);
  _fg_color=~_fg_color;

#if DBG
  printf("log: LEAVE print_log.\n");
#endif
}

int main(void)
{
  static L4Re::Util::Registry_server<> server;
  static Log log;

  // Register print server
  if (!server.registry()->register_obj(&log, "logsrv").is_valid())
  {
    printf("log: Could not register log service, readonly namespace?");
    return 1;
  }

  printf("log: starting server loop.\n");

  // wait for client request
  server.loop();

  return 0;
}

