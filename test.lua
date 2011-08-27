
require 'parallel'

worker = [[
      require 'torch'
      parallel.print('Im a worker, my ID is: ' .. parallel.id)
      t = torch.Storage()
      for i = 1,5 do
         parallel.receive(parallel.parent, t)
         parallel.print('from process ' .. parallel.id)
         sys.sleep(1)
      end
]]

parallel.print('Im the parent, my ID is: ' .. parallel.id)

w = {}
for i = 1,3 do
   w[i] = parallel.run(worker)
end

data = torch.Storage(100)
parallel.send(w[1], data)

parallel.join(w)
parallel.print('all processes terminated')
