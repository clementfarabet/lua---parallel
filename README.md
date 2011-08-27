# parallel: a (simple) parallel computing framework for Lua

This package provides a simple mechanism to dispatch Lua scripts 
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

(for info: this will first install Torch7, which is used to exchange numeric
data between processes)

## Use the library

A simple example:

``` lua
require 'torch'
require 'parallel'

-- define code for workers:
worker = [[
      -- a worker starts with a blank stack, we need to reload
      -- our libraries
      require 'torch'

      -- print from worker:
      parallel.print('Im a worker, my ID is: ' .. parallel.id)

      -- define a storage to receive data from top process
      t = torch.Storage()
      for i = 1,5 do
         -- receive data
         parallel.receive(parallel.parent, t)
         parallel.print('received storage of size ' .. t:size())
         parallel.print('first elets: ', t[1], t[2], t[3])
         sys.sleep(1)
      end
]]

-- print from top process
parallel.print('Im the parent, my ID is: ' .. parallel.id)

-- nb of workers
nprocesses = 4

-- dispatch/run each worker in a separate process
w = {}
for i = 1,nprocesses do
   w[i] = parallel.run(worker)
end

-- transmit data to each worker
data = torch.Storage(100)
for i = 1,100 do
   data[i] = i
end

-- receive data from each worker
for i = 1,5 do
   for i = 1,nprocesses do
      parallel.send(w[i], data)
   end
end

-- sync/terminate when all workers are done
parallel.join(w)
parallel.print('all processes terminated')
```
