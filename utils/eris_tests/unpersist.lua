permtable = { 1234 }

function testcounter(counter)
  local a = counter.cur()
  counter.inc()
  return counter.cur() == a + 1
end

function testuvinthread(func)
  local success, result = coroutine.resume(func)
  return success and result == 5
end

function test(rootobj)
  local passed = 0
  local total = 0
  local dotest = function(name, cond)
    total = total + 1
    if cond then
      print(name, " PASSED")
      passed = passed + 1
    else
      print(name, "*FAILED")
    end
  end

  dotest("Permanent value        ", rootobj.testperm == permtable)
  dotest("Nil value              ", rootobj.testnil == nil)
  dotest("Boolean FALSE          ", rootobj.testfalse == false)
  dotest("Boolean TRUE           ", rootobj.testtrue == true)
  dotest("Light userdata         ", checkludata(rootobj.testludata))
  dotest("Number 7               ", rootobj.testseven == 7)
  dotest("String 'foobar'        ", rootobj.testfoobar == "foobar")
  dotest("Table                  ", rootobj.testtbl.a == 2 and rootobj.testtbl[2] == 4)
  dotest("NaN value              ", rootobj.testnan[1] ~= rootobj.testnan[1])
  dotest("Looped tables          ", rootobj.testlooptable.testloopb.testloopa == rootobj.testlooptable)
  dotest("Table metatable        ", rootobj.testmt() == 21)
  dotest("__newindex metamethod  ", rootobj.testniinmt.a == 3)
  dotest("Udata literal persist  ", unboxinteger(rootobj.testliteraludata) == 71)
  dotest("Func returning 4       ", rootobj.testfuncreturnsfour() == 4)
  dotest("Lua closure            ", rootobj.testclosure() == 11)
  dotest("Function env           ", rootobj.testfenv() == 456)
  dotest("Nil in closure         ", rootobj.testnilclosure() == nil)
  dotest("Nested func            ", rootobj.testnest(1) == 6)
  dotest("Upvalue cycles         ", rootobj.testuvcycle()[1] == rootobj.testuvcycle()[2])
  dotest("Table special persist  ", rootobj.testsptable.a == 6)
  dotest("Udata special persist  ", unboxboolean(rootobj.testspudata1) == true and unboxboolean(rootobj.testspudata2) == false)
  dotest("Identical tables       ", rootobj.testsharedrefa ~= rootobj.testsharedrefb)
  dotest("Shared reference       ", rootobj.testsharedrefa.sharedref == rootobj.testsharedrefb.sharedref)
  dotest("Shared upvalues        ", testcounter(rootobj.testsharedupval))
  dotest("Debug info             ", (rootobj.testdebuginfo(2)) == "foo")
  dotest("Thread start           ", coroutine.resume(rootobj.testnthread) == true, 4)
  dotest("Thread resume          ", coroutine.resume(rootobj.testthread) == true, 14)
  dotest("Thread dead            ", coroutine.resume(rootobj.testdthread) == false)
  dotest("Open upvalues          ", testuvinthread(rootobj.testuvinthread))
  dotest("Yielded pcall          ", coroutine.resume(rootobj.testprotthr) == true, "test")
  dotest("Yielded xpcall         ", coroutine.resume(rootobj.testxprotthr) == true, "handler:test")
  dotest("Yielded metafunc       ", coroutine.resume(rootobj.testymtthr) == true, true)
  dotest("Deep callstack         ", rootobj.testdeep() == 100)
  dotest("Tail call              ", rootobj.testtail() == 100)

  print()
  if passed == total then
    print("All tests passed.")
  else
    print(passed .. "/" .. total .. " tests passed.")
  end

  if rootobj.testlife then
    print(select(2, coroutine.resume(rootobj.testlife)))
    print(select(2, coroutine.resume(rootobj.testlife)))
  end
end

-------------------------------------------------------------------------------

infile, err = io.open(..., "rb")
if infile == nil then
  error("While opening: " .. (err or "unknown error"))
end

buf, err = infile:read("*a")
if buf == nil then
  error("While reading: " .. (err or "unknown error"))
end

infile:close()

-------------------------------------------------------------------------------

uperms = {
  _ENV = _ENV,
  [1] = coroutine.yield,
  [2] = permtable,
  [3] = pcall,
  [4] = xpcall,
}
rootobj = eris.unpersist(uperms, buf)

test(rootobj)

os.remove(...)