
-- required libs
require 'parallel'
require 'lab'

-- function to spawn an empty worker waiting for code (needed for PBS
-- b/c the job needs running to keep the node scheduled

function spawn(nworkers)
   for i = 1,nworkers do 
      parallel.fork('','qsub','lua')
   end
end

-- define code for workers:
worker = [[
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

-- parent code:
function parent()
   -- print from top process
   parallel.print('Im the parent, my ID is: ' .. parallel.id)

   -- configure remotes [modify this line to try other machines]
   -- parallel.addremote({ip='localhost', cores=8, lua='~/lua-local/bin/lua'})
   -- fork 20 processes
   -- parallel.print('forking 20 processes on remote machine(s)')
   -- parallel.sfork(20)
   spawn(20) -- create 20 workers


   -- exec worker code in each process
   parallel.children:exec(worker)

   -- create a complex object to send to workers
   t = {name='my variable', data=lab.randn(100,100)}

   -- transmit object to each worker
   parallel.print('transmitting object with norm: ', t.data:norm())
   for i = 1,1000 do
      parallel.children:join()
      parallel.children:send(t)
      replies = parallel.children:receive()
   end
   parallel.print('transmitted data to all children')

   -- sync/terminate when all workers are done
   parallel.children:join('break')
   parallel.print('all processes terminated')
end

-- protected execution:
ok,err = pcall(parent)
if not ok then print(err) end
parallel.close()
