local rootobj = {}

-------------------------------------------------------------------------------
-- Used by C callbacks, so let's define it first.

function booleanpersist(udata)
  local b = unboxboolean(udata)
  return function()
    return boxboolean(b)
  end
end

-------------------------------------------------------------------------------
-- Permanent values.

local permtable = { 1234 }

rootobj.testperm = permtable

-------------------------------------------------------------------------------
-- Basic value types.

rootobj.testnil = nil
rootobj.testfalse = false
rootobj.testtrue = true
rootobj.testludata = createludata()
rootobj.testseven = 7
rootobj.testfoobar = "foobar"

-------------------------------------------------------------------------------
-- Tables.

local testtbl = { a = 2, [2] = 4 }

rootobj.testtbl = testtbl

-------------------------------------------------------------------------------
-- NaNs in tables (checks that this doesn't break the internal ref table).

local nantable = {}
nantable[1] = 0/0

rootobj.testnan = nantable

-------------------------------------------------------------------------------
-- Cycles in tables.

local testloopa = {}
local testloopb = { testloopa = testloopa }
testloopa.testloopb = testloopb

rootobj.testlooptable = testloopa

-------------------------------------------------------------------------------
-- Metatables.

local twithmt = {}
setmetatable( twithmt, { __call = function() return 21 end } )

rootobj.testmt = twithmt

-------------------------------------------------------------------------------
-- Yet more metatables.

local niinmt = { a = 3 }
setmetatable(niinmt, {__newindex = function(key, val) end })

rootobj.testniinmt = niinmt

-------------------------------------------------------------------------------
-- Literal userdata.

local literaludata = boxinteger(71)

rootobj.testliteraludata = literaludata

-------------------------------------------------------------------------------
-- Functions (closures without upvalues).

local function func()
  return 4
end

rootobj.testfuncreturnsfour = func

-------------------------------------------------------------------------------
-- Environment. This is really redundant in 5.2 since envs are just upvalues,
-- but it may still be considered somewhat special.
local testfenv = (function()
  local _ENV = { abc = 456 }
  return function()
    return abc
  end
end)()

rootobj.testfenv = testfenv

-------------------------------------------------------------------------------
-- Closures.

local function funcreturningclosure(n)
  return function()
    return n
  end
end

rootobj.testclosure = funcreturningclosure(11)
rootobj.testnilclosure = funcreturningclosure(nil)

-------------------------------------------------------------------------------
-- More closures.

local function nestedfunc(n)
  return (function(m) return m+2 end)(n+3)
end

rootobj.testnest = nestedfunc

-------------------------------------------------------------------------------
-- Cycles in upvalues.

local function GenerateObjects()
  local Table = {}

  function Table:Func()
    return { Table, self }
  end

  function uvcycle()
    return Table:Func()
  end
end

GenerateObjects()

rootobj.testuvcycle = uvcycle

-------------------------------------------------------------------------------
-- Special callback for persisting tables.

local sptable = { a = 3 }

setmetatable(sptable, { 
  __persist = function(tbl)
    local a = tbl.a
    return function()
      return { a = a+3 }
    end
  end 
})

rootobj.testsptable = sptable

-------------------------------------------------------------------------------
-- Special callbacks for persisting userdata.

rootobj.testspudata1 = boxboolean(true)
rootobj.testspudata2 = boxboolean(false)

-------------------------------------------------------------------------------
-- Reference correctness.

local sharedref = {}
refa = {sharedref = sharedref}
refb = {sharedref = sharedref}

rootobj.testsharedrefa = refa
rootobj.testsharedrefb = refb

-------------------------------------------------------------------------------
-- Shared upvalues (like reference correctness for upvalues).

local function makecounter()
  local a = 0
  return {
    inc = function() a = a + 1 end,
    cur = function() return a end
  }
end

rootobj.testsharedupval = makecounter()

-------------------------------------------------------------------------------
-- Debug info.

local function debuginfo(foo)
  foo = foo + foo
  return debug.getlocal(1,1)
end

rootobj.testdebuginfo = debuginfo

-------------------------------------------------------------------------------
-- Suspended thread.

local function fc(i)
  local ic = i + 1
  coroutine.yield()
  return ic*2
end

local function fb(i)
  local ib = i + 1
  ib = ib + fc(ib)
  return ib
end

local function fa(i)
  local ia = i + 1
  return fb(ia)
end

local thr = coroutine.create(fa)
coroutine.resume(thr, 2)

rootobj.testthread = thr

-------------------------------------------------------------------------------
-- Not yet started thread.

rootobj.testnthread = coroutine.create(function() return func() end)

-------------------------------------------------------------------------------
-- Dead thread.

local deadthr = coroutine.create(function() return func() end)
coroutine.resume(deadthr)

rootobj.testdthread = deadthr

-------------------------------------------------------------------------------
-- Open upvalues (stored in thread stack).

local function uvinthreadfunc()
  local a = 1
  local b = function()
    a = a+1
    coroutine.yield(a)
    a = a+1
  end
  a = a+1
  b()
  a = a+1
  return a
end

local uvinthread = coroutine.create(uvinthreadfunc)
coroutine.resume(uvinthread)

rootobj.testuvinthread = uvinthread

-------------------------------------------------------------------------------
-- Yield across pcall.

local function protf(arg)
  coroutine.yield()
  error(arg, 0)
end
local function protthreadfunc()
  local res, err = pcall(protf, "test")
  return err
end

local protthr = coroutine.create(protthreadfunc)
coroutine.resume(protthr)

rootobj.testprotthr = protthr

-------------------------------------------------------------------------------
-- Yield across xpcall with message handler.

local function xprotthreadfunc()
  local function handler(msg)
    return "handler:" .. msg
  end
  local res, err = xpcall(protf, handler, "test")
  return err
end

local xprotthr = coroutine.create(xprotthreadfunc)
coroutine.resume(xprotthr)

rootobj.testxprotthr = xprotthr

-------------------------------------------------------------------------------
-- Yield out of metafunction.

local function ymtf(arg)
  coroutine.yield()
  return true
end
function ymtthreadfunc()
  local t = setmetatable({}, {__lt = ymtf})
  return t < 5
end

local ymtthr = coroutine.create(ymtthreadfunc)
coroutine.resume(ymtthr)

rootobj.testymtthr = ymtthr

-------------------------------------------------------------------------------

-- I considered supporting the hook callback from the debug library, but then
-- Eris would also have to persist the registry table the debug library uses,
-- and things go quickly out of hand that way, so I decided against that.
--[[
function hookthrfunc()
  local hookRan = false
  local function callback()
    print("hook!")
    hookRan = true
  end
  debug.sethook(callback, "", 100000)
  coroutine.yield("yielded")
  for i = 1, 10000000 do
    if hookRan then break end
  end
  return hookRan
end

hookthr = coroutine.create(hookthrfunc)
print(coroutine.resume(hookthr))

rootobj.testhookthr = hookthr
]]

-------------------------------------------------------------------------------
-- Deep callstacks (100 levels).

local function deepfunc(x)
  x = x or 0
  if x == 100 then
    coroutine.yield()
    return x
  end
  local result = deepfunc(x + 1) -- no tailcall
  return result
end

local deepcall = coroutine.wrap(deepfunc)
deepcall()

rootobj.testdeep = deepcall

-------------------------------------------------------------------------------
-- Tail calls.

local function tailfunc()
  local function tailer(x)
    x = x or 0
    if x == 100 then
      coroutine.yield()
      return x
    end
    return tailer(x + 1)
  end
  local result = tailer()
  return result
end

function wrap(t)
  local co = coroutine.create(t)
  return function(...)
    local res = {coroutine.resume(co, ...)}
    if res[1] then return select(2, table.unpack(res)) end
    error(select(2, table.unpack(res)), 0)
  end
end

local tailcall = wrap(tailfunc)
tailcall()

rootobj.testtail = tailcall

-------------------------------------------------------------------------------
-- From the Lua test cases, as a more complex piece of code. Since this is
-- easier to verify visually it spams the output quite a bit, so it's disabled
-- per default.

--[[
local lifethr = coroutine.create(function()
  local _ENV = { write = coroutine.yield }

  -- life.lua
  -- original by Dave Bollinger <DBollinger@compuserve.com> posted to lua-l
  -- modified to use ANSI terminal escape sequences
  -- modified to use for instead of while
  -- modified for this test

  ALIVE="¥" DEAD="þ"
  ALIVE="O" DEAD="-"

  function ARRAY2D(w,h)
    local t = {w=w,h=h}
    for y=1,h do
      t[y] = {}
      for x=1,w do
        t[y][x]=0
      end
    end
    return t
  end

  _CELLS = {}

  -- give birth to a "shape" within the cell array
  function _CELLS:spawn(shape,left,top)
    for y=0,shape.h-1 do
      for x=0,shape.w-1 do
        self[top+y][left+x] = shape[y*shape.w+x+1]
      end
    end
  end

  -- run the CA and produce the next generation
  function _CELLS:evolve(next)
    local ym1,y,yp1,yi=self.h-1,self.h,1,self.h
    while yi > 0 do
      local xm1,x,xp1,xi=self.w-1,self.w,1,self.w
      while xi > 0 do
        local sum = self[ym1][xm1] + self[ym1][x] + self[ym1][xp1] +
                    self[y][xm1] + self[y][xp1] +
                    self[yp1][xm1] + self[yp1][x] + self[yp1][xp1]
        next[y][x] = ((sum==2) and self[y][x]) or ((sum==3) and 1) or 0
        xm1,x,xp1,xi = x,xp1,xp1+1,xi-1
      end
      ym1,y,yp1,yi = y,yp1,yp1+1,yi-1
    end
  end

  -- output the array to screen
  function _CELLS:draw()
    local out="" -- accumulate to reduce flicker
    for y=1,self.h do
     for x=1,self.w do
        out=out..(((self[y][x]>0) and ALIVE) or DEAD)
      end
      out=out.."\n"
    end
    write(out)
  end

  -- constructor
  function CELLS(w,h)
    local c = ARRAY2D(w,h)
    c.spawn = _CELLS.spawn
    c.evolve = _CELLS.evolve
    c.draw = _CELLS.draw
    return c
  end

  --
  -- shapes suitable for use with spawn() above
  --
  HEART = { 1,0,1,1,0,1,1,1,1; w=3,h=3 }
  GLIDER = { 0,0,1,1,0,1,0,1,1; w=3,h=3 }
  EXPLODE = { 0,1,0,1,1,1,1,0,1,0,1,0; w=3,h=4 }
  FISH = { 0,1,1,1,1,1,0,0,0,1,0,0,0,0,1,1,0,0,1,0; w=5,h=4 }
  BUTTERFLY = { 1,0,0,0,1,0,1,1,1,0,1,0,0,0,1,1,0,1,0,1,1,0,0,0,1; w=5,h=5 }

  -- the main routine
  function LIFE(w,h)
    -- create two arrays
    local thisgen = CELLS(w,h)
    local nextgen = CELLS(w,h)

    -- create some life
    -- about 1000 generations of fun, then a glider steady-state
    thisgen:spawn(GLIDER,5,4)
    thisgen:spawn(EXPLODE,25,10)
    thisgen:spawn(FISH,4,12)

    -- run until break
    local gen=1
    while 1 do
      thisgen:evolve(nextgen)
      thisgen,nextgen = nextgen,thisgen
      thisgen:draw()
      gen=gen+1
      if gen>2000 then break end
    end
  end

  LIFE(40,20)
end)
print(select(2, coroutine.resume(lifethr)))
print(select(2, coroutine.resume(lifethr)))

rootobj.testlife = lifethr
--]]
-------------------------------------------------------------------------------
-- Do actual persisting with some perms.

perms = {
  [_ENV] = "_ENV",
  [coroutine.yield] = 1,
  [permtable] = 2,
  [pcall] = 3,
  [xpcall] = 4,
}
buf = eris.persist(perms, rootobj)

-------------------------------------------------------------------------------
-- Write to file.

outfile = io.open(..., "wb")
outfile:write(buf)
outfile:close()