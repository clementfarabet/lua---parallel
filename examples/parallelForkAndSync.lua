
-- required libs
require 'parallel'
require 'torch'

-- calibration code:
function calib()
   require 'torch'
   require 'sys'
   s = torch.Tensor(100000):fill(1)
   d = torch.Tensor(100000):fill(0)
   parallel.yield()
   sys.tic()
   for i = 1,10000 do
      d:add(13,s)
   end
   time = sys.toc()
   parallel.parent:send(time)
end

-- parent code:
function parent()
   for i = 1,2 do
      -- for 4 processes
      parallel.sfork(4)

      -- verify creation
      parallel.print('currently have ' .. parallel.nchildren .. ' children')

      -- exec code
      parallel.children:exec(calib)

      -- trigger computations
      parallel.children:join()

      -- receive results
      times = parallel.children:receive()
      for i,time in pairs(times) do
         print('time taken by process ' .. i .. ': ' .. time)
      end

      -- sync processes (wait for them to die)
      parallel.children:sync()

      -- end status
      parallel.print('currently have ' .. parallel.nchildren .. ' children')
   end
end

-- protected execution:
ok,err = pcall(parent)
if not ok then print(err) end
parallel.close()
