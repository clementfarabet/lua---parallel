package = "parallel"
version = "1.1-0"

source = {
   url = "git://github.com/clementfarabet/lua---parallel"
}

description = {
   summary = "A package to easily fork processes, for Torch",
   detailed = [[
A package to fork and serialize data between multiple processes.
   ]],
   homepage = "https://github.com/clementfarabet/lua---parallel",
   license = "BSD"
}

dependencies = {
   "torch >= 7.0",
   "sys >= 1.0",
}

build = {
   type = "command",
   build_command = [[
cmake -E make_directory build;
cd build;
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(LUA_BINDIR)/.." -DCMAKE_INSTALL_PREFIX="$(PREFIX)"; 
$(MAKE)
   ]],
   install_command = "cd build && $(MAKE) install"
}
