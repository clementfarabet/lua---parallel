
-- required libs
require 'parallel'
require 'lab'

-- print from top process
parallel.print('Im the parent, my ID is: ' .. parallel.id)

-- fork N processes
parallel.print('forking N processes on remote machine(s)')
parallel.nfork({4, ip='texier.cs', lua='~/lua-local/bin/lua'})

-- define code for workers:
worker = [[
      print('crap')

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
]]

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
