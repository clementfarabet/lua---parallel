
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
         parallel.print('received object with first elts: ', t.data[1][1], t.data[1][2], t.data[1][3])
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
parallel.print('transmitting object with first elts: ', t.data[1][1], t.data[1][2], t.data[1][3])
for i = 1,5 do
   for i = 1,nprocesses do
      parallel.children[i]:send(t)
   end
end

-- sync/terminate when all workers are done
parallel.children:join()
parallel.print('all processes terminated')
