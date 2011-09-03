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
--     September  2, 2011, 5:42PM - using 0MQ instead of IPC - Scoffier / Farabet
--     August 27, 2011, 6:31PM - beta release - Clement Farabet
--     August 26, 2011, 6:03PM - creation - Clement Farabet
----------------------------------------------------------------------

require 'os'
require 'io'
require 'sys'
require 'torch'
require 'zmq'

local glob = _G
local assignedid
if parallel then 
   assignedid = parallel.id
   assignedip = parallel.ip
   parent = parallel.parent
end
local sys = sys
local zmq = zmq
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
ip = assignedip or "127.0.0.1"
parent = parent or {id = -1}
children = {}
processid = 1

--------------------------------------------------------------------------------
-- 0MQ context and options
--------------------------------------------------------------------------------
zmqctx = zmq.init(1)
currentport = 5000

--------------------------------------------------------------------------------
-- configure local IP
--------------------------------------------------------------------------------
autoip = function(interface)
            local interfaces
            if glob.type(interface) == 'table' then
               interfaces = interface
            elseif interface then
               interfaces = {interface}
            end
            if sys.OS == 'linux' then
               interfaces = interfaces or {'eth0','eth1'}
               local ipfound
               for _,interface in ipairs(interfaces) do
                  ipfound = sys.execute("/sbin/ifconfig " .. interface
                                        .. " | grep 'inet addr:'| grep -v '127.0.0.1'"
                                        .. " | cut -d: -f2 | awk '{ print $1}'")
                  if ipfound:find('%d') then
                     ip = ipfound:gsub('%s','')
                     break
                  end
               end
            elseif sys.OS == 'macos' then
               interfaces = interfaces or {'en0','en1'}
               local ipfound
               for _,interface in ipairs(interfaces) do
                  ipfound = sys.execute("/sbin/ifconfig " .. interface
                                        .. " | grep -E 'inet.[0-9]' | grep -v '127.0.0.1'"
                                        .. " | awk '{ print $2}'")
                  if ipfound:find('%d') then
                     ip = ipfound:gsub('%s','')
                     break
                  end
               end
            else
               print('WARNING: unsupported OS')
               return
            end
         end

--------------------------------------------------------------------------------
-- run is a shortcut for fork/exec code on the local machine
--------------------------------------------------------------------------------
run = function(code,...)
         -- (1) fork process
         local child = fork(nil, nil, nil, ...)

         -- (2) exec code
         child:exec(code)
      end

--------------------------------------------------------------------------------
-- fork new idle process
--------------------------------------------------------------------------------
fork = function(rip, protocol, rlua, ...)
          -- (0) connect to remote machine
          if rip then
             protocol = protocol or 'ssh'
             rlua = rlua or 'lua'
             if ip == '127.0.0.1' then
                print('<parallel.fork> WARNING: local ip is set to localhost, forked'
                      .. ' remote processes will not be able to reach it,'
                      .. ' please set your local ip: parallel.ip = "XX.XX.XX.XX"')
             end
          end

          -- (1) create sockets to communicate with child
          local sockreq = zmqctx:socket(zmq.REQ)
          local sockrep = zmqctx:socket(zmq.REP)
          local sockmsg = zmqctx:socket(zmq.REQ)
          local portreq = currentport
          while not sockreq:bind("tcp://" .. ip .. ":" .. portreq) do
             currentport = currentport + 1
             portreq = currentport
          end
          local portrep = currentport
          while not sockrep:bind("tcp://" .. ip .. ":" .. portrep) do
             currentport = currentport + 1
             portrep = currentport
          end
          local portmsg = currentport
          while not sockmsg:bind("tcp://" .. ip .. ":" .. portmsg) do
             currentport = currentport + 1
             portmsg = currentport
          end

          -- (2) generate code for child
          --     this involve setting its id, parent id, and making sure it connects
          --     to its parent
          local str =  "parallel = {} "
          str = str .. "parallel.id = " .. processid .. " "
          str = str .. "parallel.parent = {id = " .. id .. "} "
          str = str .. "require([[parallel]]) "
          str = str .. "parallel.parent.socketrd = parallel.zmqctx:socket(zmq.REP) "
          str = str .. "parallel.parent.socketrd:connect([[tcp://"..ip..":"..portreq.."]]) "
          str = str .. "parallel.parent.socketwr = parallel.zmqctx:socket(zmq.REQ) "
          str = str .. "parallel.parent.socketwr:connect([[tcp://"..ip..":"..portrep.."]]) "
          str = str .. "parallel.parent.socketmsg = parallel.zmqctx:socket(zmq.REP) "
          str = str .. "parallel.parent.socketmsg:connect([[tcp://"..ip..":"..portmsg.."]]) "
          local args = {...}
          str = str .. "parallel.args = {}"
          for i = 1,glob.select('#',...) do
             str = str .. 'table.insert(parallel.args, ' .. tostring(args[i]) .. ') '
          end
          str = str .. "loadstring(parallel.parent:receive())() "

          -- (3) fork a lua process, running the code dumped above
          if protocol then
             os.execute(protocol .. ' ' .. rip ..
                        ' "' .. rlua .. " -e '" .. str .. "' " .. '" &')
          else
             os.execute('lua -e "' .. str .. '" &')
          end

          -- (4) register child process for future reference
          local child = {id=processid, 
                         join=join, send=send, receive=receive, exec=exec, 
                         socketwr=sockreq, socketrd=sockrep, socketmsg=sockmsg}
          glob.table.insert(children, child)

          -- (5) incr counter for next process
          processid = processid + 1
          return child
       end

--------------------------------------------------------------------------------
-- exec code in given process
--------------------------------------------------------------------------------
exec = function(process, code)
          local processes = process
          if not process[1] then processes = {process} end
          -- make sure no process is already running code
          for _,process in ipairs(processes) do
             if process.running then
                error('<parallel.exec> process already running code, cannot exec again')
             end
             process.running = true
          end
          -- load all processes with code
          send(processes, code)
       end

--------------------------------------------------------------------------------
-- join = synchronize processes that have yielded, blocking call
--------------------------------------------------------------------------------
join = function(process, msg)
          msg = msg or ''
          if process[1] then
             -- a list of processes to join
             for _,proc in ipairs(process) do
                proc.socketmsg:send(msg)
             end
             for _,proc in ipairs(process) do
                proc.socketmsg:recv()
             end
          else 
             -- a single process to join
             process.socketmsg:send(msg)
             process.socketmsg:recv()
          end
       end

--------------------------------------------------------------------------------
-- yield = interupt execution flow to allow parent to join
--------------------------------------------------------------------------------
yield = function()
           local msg = parent.socketmsg:recv()
           parent.socketmsg:send('!')
           return msg
        end

--------------------------------------------------------------------------------
-- transmit data
--------------------------------------------------------------------------------
send = function(process, object)
          if process[1] then 
             -- multiple processes
             local processes = process
             -- a list of processes to send data to
             if not (torch.typename(object) and torch.typename(object):find('torch.*Storage')) then
                -- serialize data once for all transfers
                local f = torch.MemoryFile()
                f:binary()
                f:writeObject(object)
                object = f:storage()
                f:close()
             end
             -- broadcast storage to all processes
             for _,process in ipairs(processes) do
                object.zmq.send(object, process.socketwr)
             end
             -- get acks from all processes
             for _,process in ipairs(processes) do
                process.socketwr:recv()
             end
          else
             if torch.typename(object) and torch.typename(object):find('torch.*Storage') then
                -- raw transfer ot storage
                object.zmq.send(object, process.socketwr)
                process.socketwr:recv()
             else
                -- serialize data first
                local f = torch.MemoryFile()
                f:binary()
                f:writeObject(object)
                local s = f:storage()
                -- then transmit raw storage
                send(process, s)
                f:close()
             end
          end
       end

--------------------------------------------------------------------------------
-- receive data
--------------------------------------------------------------------------------
receive = function(process, object)
             if process[1] then 
                -- receive all objects
                if object and object[1] and torch.typename(object[1]) and torch.typename(object[1]):find('torch.*Storage') then
                   -- user objects are storages, just fill them
                   local objects = object
                   for i,proc in ipairs(process) do
                      object[i].zmq.recv(object[i], proc.socketrd)
                      proc.socketrd:send('!')
                   end
                else
                   -- receive raw storages
                   local storages = {}
                   for i,proc in ipairs(process) do
                      storages[i] = torch.CharStorage()
                      storages[i].zmq.recv(storages[i], proc.socketrd)
                      proc.socketrd:send('!')
                   end
                   -- then un-serialize data objects
                   object = object or {}
                   for i = 1,#process do
                      local f = torch.MemoryFile(storages[i])
                      f:binary()
                      object[i] = f:readObject()
                      f:close()
                   end
                end
             else
                if object and torch.typename(object) and torch.typename(object):find('torch.*Storage') then
                   -- raw receive of storage
                   object.zmq.recv(object, process.socketrd)
                   process.socketrd:send('!')
                else
                   -- first receive raw storage
                   local s = torch.CharStorage()
                   receive(process, s)
                   -- then un-serialize data object
                   local f = torch.MemoryFile(s)
                   f:binary()
                   object = f:readObject()
                   f:close()
                end
             end
             return object
          end

--------------------------------------------------------------------------------
-- all processes should use this print method
--------------------------------------------------------------------------------
print = function(...)
           glob.print('<parallel#' .. glob.string.format('%03d',id) .. '>', ...)
        end

--------------------------------------------------------------------------------
-- reset = forget all children, go back to initial state
-- TODO: this is the right place to properly terminate children
--------------------------------------------------------------------------------
reset = function()
           id = assignedid or 0
           parent = parent or {id = -1}
           children = {}
           processid = 1
           if parent.id ~= -1 then
              parent.receive = receive
              parent.send = send
           end
           children.join = join
           children.send = send
           children.receive = receive
           children.exec = exec
           autoip()
        end
reset()
