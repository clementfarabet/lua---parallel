
-- required libs
require 'parallel'
require 'torch'

print = parallel.print

-- parent code:
function parent()
   -- declare machines to use
   parallel.addremote({ip='localhost', cores=4, lua=paths.install_bin..'/torch-lua'},
                      {ip='localhost', cores=4, lua=paths.install_bin..'/torch-lua'})

   -- run calibration
   parallel.calibrate()

   -- print coefs obtained
   for _,remote in ipairs(parallel.remotes) do
      print(remote.ip .. ' has a speed coef of ' .. remote.speed)
   end

   -- creating 8 children
   print('free cores: ' .. parallel.remotes.cores)
   forked = parallel.sfork(parallel.remotes.cores)
   print('created ' .. #forked .. ' children, with speed coefs:')
   for _,forked in ipairs(forked) do
      print('id: ' .. forked.id .. ', speed = ' .. forked.speed)
   end
end

-- protected execution:
ok,err = pcall(parent)
if not ok then print(err) end
parallel.close()
