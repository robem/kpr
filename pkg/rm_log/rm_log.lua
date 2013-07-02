-- vim:set ft=lua:

-- Include L4 functionality
require("L4");

-- Some shortcut for less typing
local ld = L4.default_loader;
local fbdrv_io = ld:new_channel(); -- for IO
local fbdrv_fb = ld:new_channel(); -- for FB-DRV
local log = ld:new_channel();

ld:start({ caps = { fbdrv = fbdrv_io:svr(),
                    icu   = L4.Env.icu,
                    sigma0= L4.cast( L4.Proto.Factory, L4.Env.sigma0 ):create( L4.Proto.Sigma0 )},
           log = { "io", "y" }
         },
        "rom/io rom/x86-legacy.devs rom/x86-fb.io");

ld:start({ caps = { vbus = fbdrv_io, 
                    fb   = fbdrv_fb:svr() },
           log = { "fbdrv", "b" }
         },
        "rom/fb-drv -m 0x117");

ld:start({ caps = { fb = fbdrv_fb,
                    log = log:svr() },
           log = { "server", "red" } 
         },
         "rom/rm_log");

local env = L4.App_env.new();
env.log = log;

ld:startv(env, "rom/hello");
