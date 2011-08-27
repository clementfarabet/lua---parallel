
require 'parallel'

parallel.sharedSize = 1024*1024

nprocesses = 4

worker = [[
      require 'torch'
      parallel.print('Im a worker, my ID is: ' .. parallel.id)
      t = torch.Storage()
      for i = 1,5 do
         parallel.receive(parallel.parent, t)
         parallel.print('received storage of size ' .. t:size())
         parallel.print('first elets: ', t[1], t[2], t[3])
         sys.sleep(1)
      end
]]

parallel.print('Im the parent, my ID is: ' .. parallel.id)

w = {}
for i = 1,nprocesses do
   w[i] = parallel.run(worker)
end

data = torch.Storage(100)
for i = 1,100 do
   data[i] = i
end

for i = 1,5 do
   for i = 1,nprocesses do
      parallel.send(w[i], data)
   end
end

parallel.join(w)
parallel.print('all processes terminated')
