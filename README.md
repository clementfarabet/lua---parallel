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

### API, in very short:

Load/start up package:

``` lua
require 'parallel'
```

Dispatch new process:

``` lua
-- define process' code:
code = [[
     -- arbitrary code contained here
     require 'torch'
     t = torch.Tensor(10)
     print(t)

     -- any process can access its id, its parent's id [and children's id]
     print(parallel.id)
     print(parallel.parent.id)
     if parallel.children[1] then print(parallel.children[1].id)

     -- if arguments were passed, they're found in the regular ... table        
     args = {...}    
     print(args[1])
]]

-- execute process, with optional arguments:
parallel.run(code [[[, arg1], arg2], ...])
```

Join running processes: this is a simple blocking call that waits for the 
given processes to terminate:

``` lua
-- create processes:
for i = 1,4 do
    parallel.run('print "Im in a process"')
end

-- sync processes, the following two calls are equivalent:
-- (1)
parallel.children:join() -- join all children
-- (2)
for i = 1,4 do
    parallel.children[i]:join() -- join individual child
end
```

When creating a child (parallel.run), two shared memory segments are automatically
created to transfer data between the two processes. Two functions send() and receive()
can be used to *efficiently* transfer data between these processes. Any Lua type, 
and all Torch7 type (tensor, storage, ...) can be transferred this way. The transmission
is efficient for numeric data, as serialization merely involves a binary copy and
some extra headers for book-keeping (see serialization in Torch7's manual).

``` lua
-- define some code for children
somecode = [[
    while true do
        -- in an infinite loop, receive objects from parent:
        local obj = parallel.parent:receive()
        -- print
        parallel.print('received object:', obj)
    end
]]

-- dispatch two processes:
parallel.run(somecode)
parallel.run(somecode)

-- and send them some data:
t = {'a table', entry2='with arbitrary entries', tensor=torch.Tensor(100,100)}
while true do
    parallel.children[1]:send(t)        -- send the whole table to child 1
    parallel.children[2]:send(t.entry2) -- just send an entry to child 2
end
```

A convenient print function that prepends the process ID issuing the print:

``` lua
> parallel.print('something')

<parallel#014>  something
```

### A simple example:

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
