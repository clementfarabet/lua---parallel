# para||el: a (simple) parallel computing framework for Lua

This package provides a simple mechanism to dispatch and run Lua code
as independant processes and communicate via ZeroMQ sockets. Processes
can be forked locally or on remote machines.

## License

Copyright (c) 2011 Clement Farabet, Marco Scoffier

Permission is hereby granted, free of charge, to any person obtaining
a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

## Install dependencies 

1/ third-party libraries:

On Linux (Ubuntu > 9.04):

``` sh
$ apt-get install gcc g++ git libreadline5-dev cmake libzmq-dev libzmq0
```

On Mac OS (Leopard, or more), using [Homebrew](http://mxcl.github.com/homebrew/):

``` sh
$ brew install git readline cmake wget zmq
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

Fork a new process, or N new processes, locally:

``` lua
parallel.fork()
parallel.nfork(4)
```

Fork remote processes. In that following code, we fork 4 processes on myserver.org,
and 6 processes on myserver2.org.

``` lua
parallel.nfork( {4, ip='myserver.org', protocol='ssh', lua='/path/to/remote/lua'},
                {6, ip='myserver2.org', protocol='ssh', lua='/path/to/remote/lua'} )
```

Once processes have been forked, they all exist in a table: parallel.children, and
all methods (exec,send,receive,join) work either on individual processes, or on
groups of processes.

The first thing to do is to load these new processes with code:

``` lua
-- define process' code:
code = [=[
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
]=]

-- execute code in given process(es), with optional arguments:
parallel.children:exec(code)

-- this is equivalent to:
for _,child in ipairs(parallel.child) do
    child:exec(code)
end
```

parallel implements a simple yield/join mechanism to allow a parent to sync
and affect the behavior of its children.

``` lua
-- child code:
code = [=[
     while true do
        print('something')
        parallel.yield()
     end
]=]
c = parallel.fork()
c:exec(code)

-- parent code
for i = 1,10 do
    c:join()
end

-- each time join() is called, it waits for the child to yield, and vice-versa.
-- in that example, 'something' only gets printed when the parent joins its child
```

Slightly more complex things can be implemented with yield/join: join() can take
a string as an argument, which is returned by the corresponding yield(). This
is useful to control branching in your children:

``` lua
-- child code:
code = [=[
     while true do
        print('something')
        m = parallel.yield()
        if m == 'break' then break end
     end
]=]
c = parallel.fork()
c:exec(code)

-- parent code
c:join('break')
```

When creating a child (parallel.fork), a connection is established
to transfer data between the two processes. Two functions send() and receive()
can be used to *efficiently* transfer data between these processes. Any Lua type, 
and all Torch7 type (tensor, storage, ...) can be transferred this way. The transmission
is efficient for numeric data, as serialization merely involves a binary copy and
some extra headers for book-keeping (see serialization in Torch7's manual).

``` lua
-- define some code for children
somecode = [=[
    while true do
        -- in an infinite loop, receive objects from parent:
        local obj = parallel.parent:receive()
        -- print
        parallel.print('received object:', obj)
    end
]=]

-- dispatch two processes:
parallel.nfork(2)
parallel.children:exec(somecode)

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

### A simple complete example:

``` lua
-- required libs
require 'parallel'
require 'lab'

-- print from top process
parallel.print('Im the parent, my ID is: ' .. parallel.id)

-- fork N processes (replace this by parallel.nfork{4, ip='server.org'}) to
-- run this on a remote machine
parallel.nfork(4)

-- define code for workers:
worker = [=[
      -- a worker starts with a blank stack, we need to reload
      -- our libraries
      require 'sys'
      require 'torch'

      -- print from worker:
      parallel.print('Im a worker, my ID is: ' .. parallel.id .. ' and my IP: ' .. parallel.ip)

      -- define a storage to receive data from top process
      while true do
         -- yield = allow parent to terminate me
         m = parallel.yield()
         if m == 'break' then sys.sleep(1) break end

         -- receive data
         local t = parallel.parent:receive()
         parallel.print('received object with norm: ', t.data:norm())

         -- send some data back
         parallel.parent:send('this is my response')
      end
]=]

-- exec worker code in each process
parallel.children:exec(worker)

-- create a complex object to send to workers
t = {name='my variable', data=lab.randn(100,100)}

-- transmit object to each worker
parallel.print('transmitting object with norm: ', t.data:norm())
for i = 1,5 do
   parallel.children:join()
   parallel.children:send(t)
   replies = parallel.children:receive()
end
parallel.print('transmitted data to all children')

-- sync/terminate when all workers are done
parallel.children:join('break')
parallel.print('all processes terminated')
```
