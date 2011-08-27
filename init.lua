----------------------------------------------------------------------
--
-- Copyright (c) 2011 Clement Farabet
--
-- Permission is hereby granted, free of charge, to any person obtaining
-- a copy of this software and associated documentation files (the
-- "Software"), to deal in the Software without restriction, including
-- without limitation the rights to use, copy, modify, merge, publish,
-- distribute, sublicense, and/or sell copies of the Software, and to
-- permit persons to whom the Software is furnished to do so, subject to
-- the following conditions:
--
-- The above copyright notice and this permission notice shall be
-- included in all copies or substantial portions of the Software.
--
-- THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
-- EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
-- MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
-- NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
-- LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
-- OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
-- WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
--
----------------------------------------------------------------------
-- description:
--     parallel - a package that provides a simple mechanism to
--                dispatch Lua scripts as independant processes
--                and communicate via a super raw shared memory
--                buffer
--
-- history:
--     August 26, 2011, 6:03PM - creation - Clement Farabet
----------------------------------------------------------------------

require 'os'
require 'io'
require 'sys'
require 'torch'
require 'libparallel'

local glob = _G
local assignedid
if parallel then 
   assignedid = parallel.id 
   parent = parallel.parent
end
local sys = sys
local tostring = tostring
local torch = torch
local error = error
local require = require
local os = os
local io = io
local pairs = pairs
local ipairs = ipairs

module 'parallel'
_lib = glob.libparallel
glob.libparallel = nil

--------------------------------------------------------------------------------
-- internal variables
--------------------------------------------------------------------------------
id = assignedid or 0
parent = parent or {id = -1}
processid = 1
processes = {}
sharedSize = 8*1024
TMPFILE = '/tmp/lua.parallel.process.'

--------------------------------------------------------------------------------
-- start and run new process
--------------------------------------------------------------------------------
run = function(code,...)
         -- (0) generate dummy files for shared buffers
         local fileWR = TMPFILE..id..'-'..processid
         local fileRD = TMPFILE..processid..'-'..id
         os.execute('touch ' .. fileRD .. ' ' .. fileWR)

         -- (1) generate code for child
         --     this involve setting its id, parent id, and making sure it connects
         --     to the share buffer
         local tmpfile = TMPFILE .. tostring(sys.clock()) .. '.' .. processid
         local file = io.open(tmpfile,'w')
         file:write('parallel = {}\n')
         file:write('parallel.id = ' .. processid .. '\n')
         file:write('parallel.parent = {id = ' .. id .. '}\n')
         file:write('require "parallel"\n')
         file:write('torch.Storage().parallel.connect(' 
                    ..sharedSize..', '..id..', "'..fileRD..'", "'..fileWR..'")\n')
         file:write('\n')
         file:write(code)
         file:write('\nos.execute("rm ' .. tmpfile .. '")')
         file:close()

         -- (2) fork a lua process, running the code dumped above
         local args = {...}
         local strargs = ''
         for i = 1,glob.select('#',...) do
            strargs = strargs .. tostring(args[i]) .. ' '
         end
         os.execute('lua ' .. tmpfile .. ' ' .. strargs .. ' &')

         -- (3) register child process for future reference
         processes[processid] = {file=tmpfile}
         torch.Storage().parallel.create(sharedSize, processid, fileWR, fileRD)
         processid = processid + 1
         return {id=processid-1, join=join}
      end

--------------------------------------------------------------------------------
-- join = wait for a process to conclude
--------------------------------------------------------------------------------
join = function(process)
          if process[1] then -- a list of processes to join
             for _,proc in ipairs(process) do
                join(proc)
             end
          else -- a single process to join
             local file = processes[process.id].file
             while sys.filep(file) do
                sys.sleep(0.01)
             end
          end
       end

--------------------------------------------------------------------------------
-- transmit data
--------------------------------------------------------------------------------
send = function(process, object)
          if torch.typename(object):find('torch.*Storage') then
             object.parallel.sendStorage(object,process.id)
          else
             print('<parallel.send> unsupported type')
          end
       end

--------------------------------------------------------------------------------
-- receive data
--------------------------------------------------------------------------------
receive = function(process, object)
             if torch.typename(object):find('torch.*Storage') then
                object.parallel.receiveStorage(object,process.id)
             else
                print('<parallel.receive> unsupported type')
             end
          end

--------------------------------------------------------------------------------
-- all processes should use this print method
--------------------------------------------------------------------------------
print = function(...)
           glob.print('<parallel#' .. glob.string.format('%03d',id) .. '> ', ...)
        end
