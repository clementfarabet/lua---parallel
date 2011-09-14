require 'os'
require 'io'

local _G = _G
local require = require
local pairs = pairs
local print = print
local table = table

module 'cloud'
_p = require 'parallel'

--------------------------------------------------------------------------------
-- load all functions from parallel
--------------------------------------------------------------------------------
for k,v in pairs(_p) do
   _G.cloud[k] = v
end

-- used for cleanup
jobid=sys.execute('date +cl_%m-%d_%H:%M:%S')
user = sys.execute('echo $USER')
jobsubmissionip="login-0-1"

-- function to pass to fork which launches a remove job
function launch (str)
   local qsub_str = "qsub"..
      " -l nodes=1:ppn=1,walltime=00:59:00"..
      " -N "..jobid..
      " -q short"
   str = ' \\"' .. rlua .. " -e '" .. str .. "' " .. '\\"'
   if jobsubmissionip then
      sys.execute('ssh '..jobsubmissionip..' "echo '..str..' | '..qsub_str..'"')
   else
      sys.execute('echo '..str..' | '..qsub_str)
   end
end

--------------------------------------------------------------------------------
-- fork on HPC
--------------------------------------------------------------------------------
function spawn(nworkers)
   local forked = {}
   for i = 1,nworkers do 
      local child = fork(nil,@launch,nil)
      table.insert(forked, child)
   end
   _fill(forked)
   return forked
end

function get_running_worker_ids()
   return sys.execute("qstat | grep -e "..jobid.." -e ' R ' | wc -l")
end

function get_all_job_ids()
   return sys.execute("qstat | grep "..jobid.." | wc -l")
end

