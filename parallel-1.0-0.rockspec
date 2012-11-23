package = "parallel"
version = "1.0-0"

source = {
   url = "git://github.com/clementfarabet/lua---parallel",
   tag = "1.0-0"
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
   type = "cmake",
   variables = {
      LUAROCKS_PREFIX = "$(PREFIX)"
   }
}
