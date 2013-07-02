
require("L4");
local ld = L4.default_loader;
local the_keyboard = ld:new_channel();
local my_kbd = ld:new_channel();

ld:start({ caps = { keyboard = the_keyboard:svr(),
                    icu   = L4.Env.icu,
                    sigma0= L4.cast( L4.Proto.Factory, L4.Env.sigma0 ):create( L4.Proto.Sigma0 )},
           log = { "io", "g" }
         },
         "rom/io rom/x86-legacy.devs rom/x86-fb.io");

ld:start({ caps = { vbus = the_keyboard,
                    kbd = my_kbd:svr() 
                  },
           log = { "keyboard", "y"}
         },
         "rom/rm_keyboard");

ld:start({ caps = { kb_server = my_kbd },
           log = { "keyboard-client", "r" }
         },
         "rom/rm_keyboard-client");
