
-- libs
require 'parallel'
require 'torch'
require 'image'

-- forked process
function worker()
   require 'torch'
   while true do
      parallel.yield()
      t = parallel.parent:receive()
      io.write('.') io.flush()
      collectgarbage()
   end
end

-- parent
function parent()
   process = parallel.fork()
   process:exec(worker)
   N = 100
   t = image.scale(image.lena():float(), 640,480)
   timer = torch.Timer()
   for i = 1,N do
      process:join()
      process:send(t)
   end
   print('')
   print('average time to transfer one image: ' .. timer:time().real/N .. ' sec')
end

-- protected env
ok,err = pcall(parent)
if not ok then print(err) end
parallel.close()
