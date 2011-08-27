# parallel: a (simple) parallel computing framework for Lua

This package provides a simple mechanism to dispatch and run Lua code
as independant processes and communicate via unix point-to-point 
shared memory buffers.

## Install dependencies 

1/ third-party libraries:

On Linux (Ubuntu > 9.04):

``` sh
$ apt-get install gcc g++ git libreadline5-dev cmake
```

On Mac OS (Leopard, or more), using [Homebrew](http://mxcl.github.com/homebrew/):

``` sh
$ brew install git readline cmake wget
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
$ luarocks install parallel
```

(for info: this will first install Torch7, which is used to exchange/serialize
data between processes)

## Use the library

A simple example:

``` lua
require 'torch'
require 'lab'
require 'parallel'

-- set shared buffer size
parallel.setSharedSize(256*1024)

-- define code for workers:
worker = [[
      -- a worker starts with a blank stack, we need to reload
      -- our libraries
      require 'torch'

      -- print from worker:
      parallel.print('Im a worker, my ID is: ' .. parallel.id)

      -- define a storage to receive data from top process
      for i = 1,5 do
         -- receive data
         local t = parallel.parent:receive()
         parallel.print('received object with norm: ', t.data:norm())
      end
]]

-- print from top process
parallel.print('Im the parent, my ID is: ' .. parallel.id)

-- nb of workers
nprocesses = 4

-- dispatch/run each worker in a separate process
for i = 1,nprocesses do
   parallel.run(worker)
end

-- create a complex object to send to workers
t = {name='my variable', data=lab.randn(100,100)}

-- transmit object to each worker
parallel.print('transmitting object with norm: ', t.data:norm())
for i = 1,5 do
   for i = 1,nprocesses do
      parallel.children[i]:send(t)
   end
end

-- sync/terminate when all workers are done
parallel.children:join()
parallel.print('all processes terminated')
```
