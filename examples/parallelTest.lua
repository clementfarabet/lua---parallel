
require 'torch'
require 'lab'
require 'parallel'

-- define code for workers:
worker = [[
      -- a worker starts with a blank stack, we need to reload
      -- our libraries
      require 'torch'

      -- print from worker:
      parallel.print('Im a worker, my ID is: ' .. parallel.id .. ' and my IP: ' .. parallel.ip)

      -- define a storage to receive data from top process
      while true do
         -- yield = allow parent to terminate me
         parallel.yield()

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
   parallel.children:join()
   parallel.children:send(t)
end
parallel.print('transmitted data to all children')

-- sync/terminate when all workers are done
parallel.children:kill()
parallel.print('all processes terminated')
