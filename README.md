# parallel: a (simple) parallel computing framework for Lua

This package provides a simple mechanism to dispatch Lua scripts 
as independant processes and communicate via a unix shared memory
buffers.

## Install dependencies 

1/ third-party libraries:

On Linux (Ubuntu > 9.04):

``` sh
$ apt-get install gcc g++ git libreadline5-dev cmake wget libqt4-core libqt4-gui libqt4-dev
```

On Mac OS (Leopard, or more), using [Homebrew](http://mxcl.github.com/homebrew/):

``` sh
$ brew install git readline cmake wget qt
```

2/ Lua 5.1 + Luarocks + xLua:

``` sh
$ git clone https://github.com/clementfarabet/lua4torch
$ cd lua4torch
$ make install PREFIX=/usr/local
```

3/ parallel:

clone this repo and then:

``` sh
$ luarocks make
```

## Use the library

A simple example:

``` lua
require 'parallel'

parallel.sharedSize = 1024*1024

nprocesses = 4

worker = [[
      require 'torch'
      parallel.print('Im a worker, my ID is: ' .. parallel.id)
      t = torch.Storage()
      for i = 1,5 do
         parallel.receive(parallel.parent, t)
         parallel.print('received storage of size ' .. t:size())
         parallel.print('first elets: ', t[1], t[2], t[3])
         sys.sleep(1)
      end
]]

parallel.print('Im the parent, my ID is: ' .. parallel.id)

w = {}
for i = 1,nprocesses do
   w[i] = parallel.run(worker)
end

data = torch.Storage(100)
for i = 1,100 do
   data[i] = i
end

for i = 1,5 do
   for i = 1,nprocesses do
      parallel.send(w[i], data)
   end
end

parallel.join(w)
parallel.print('all processes terminated')
```