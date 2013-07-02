-- vim:set ft=lua:

-- Include L4 functionality
require("L4");

-- Some shortcut for less typing
local ld = L4.default_loader;
local fbdrv_io = ld:new_channel(); -- for IO
local fbdrv_fb = ld:new_channel(); -- for FB-DRV
local console_chan = ld:new_channel();
local log_chan = ld:new_channel();
local pong_chan = ld:new_channel();
local key_chan = ld:new_channel();
local the_keyboard = ld:new_channel();

ld:start({ caps = { keyboard = the_keyboard:svr(),
										fbdrv = fbdrv_io:svr(),
                    icu   = L4.Env.icu,
                    sigma0= L4.cast( L4.Proto.Factory, L4.Env.sigma0 ):create( L4.Proto.Sigma0 )},
         },
        "rom/io rom/x86-legacy.devs rom/x86-fb.io");

ld:start({ caps = { vbus = fbdrv_io, 
                    fb   = fbdrv_fb:svr() },
         },
        "rom/fb-drv -m 0x117");

-- KEYBOARD
ld:start({ caps = { vbus = the_keyboard,
                    kbd = key_chan:svr() 
                  },
           log = { "keyboard", "green"}
         },
         "rom/rm_keyboard");

-- CONSOLE
ld:start({ caps = { fb = fbdrv_fb,
										kbd = key_chan,
                    consoleAlt1 = console_chan:svr() },
           log = { "CONSOLE", "red" } 
         },
         "rom/rm_consoleAlt1");

-- LOG
ld:start({ caps = { fb = console_chan:create(0, ""),
                    logsrv = log_chan:svr() },
           log = { "LOG", "yellow" } 
         },
         "rom/rm_log");

--local env = L4.App_env.new();
--env.log = log_chan;

ld:startv({ caps = { vesa = console_chan:create(0, ""), 
										 PongServer = pong_chan:svr() },
					--log = log_chan 
					},
					"rom/pong-server");

ld:startv({ caps = { PongServer = pong_chan,
										 kbd = key_chan },
					log = log_chan 
					},
					"rom/rm_pong-client");

--ld:startv({ log = log_chan } , "rom/hello");
