
-- libs
require 'parallel'
require 'torch'
require 'image'
require 'camera'

-- do everything with floats
torch.setdefaulttensortype('torch.FloatTensor')

-- a queue handler is very simple: it waits for code to execute,
-- and executes it as soon as it receives it
function queue_handler()
   require 'torch'
   torch.setdefaulttensortype('torch.FloatTensor')
   while true do
      local code = parallel.parent:receive()
      code()
      collectgarbage()
   end
end

-- the main process grabs video frames,
-- and pushes them to the queue, to get asynchronously written to disk
function main()
   -- start queue
   queue = parallel.fork()
   queue:exec(queue_handler)

   -- make output dir
   os.execute('mkdir -p scratch')

   -- frame grabber
   getframe = image.Camera(0)
   frame = getframe:forward()

   -- grab frames async
   N = 100
   timer = torch.Timer()
   for i = 1,N do
      -- saver
      queue:send(function()
                    require 'image'
                    i = i or 1
                    nextimg = parallel.parent:receive()
                    image.save(string.format('scratch/img_%05d.jpg',i), nextimg)
                    io.write('.') io.flush()
                    i = i + 1
                 end)

      -- grab and send image
      frame = getframe:forward()
      queue:send(frame)

      -- display
      win = image.display{image=frame, win=win}
   end

   -- timing...
   print('')
   print('average time to write one image to disk, asynchronously: ' .. timer:time().real/N .. ' sec')
end

-- protected env
ok,err = pcall(main)
if not ok then print(err) end
parallel.close()
