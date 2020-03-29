local json = require 'dkjson'

function benchmark(param)
  print("benchmark: ")

  local var = json.decode(param);
  local loop  = 0;
  if var.argc >= 1 then
    loop = tonumber(var.argv[1]);
    if loop == nil then
      loop = 0;
    end
  end

  --TableClear and get
  local error = 1
  pelagia.TableClear("t2")
  local r = pelagia.Get("t2", "a")
  if (r == nil) then
    error = 0;
  end

  if error == 1 then
    print("fail TableClear and get!\n")
    return 1;
  end

  if loop and (var.argc < 2 or (var.argc >= 2 and var.argv[2] == "set")) then
    --set
    local ms = pelagia.MS();
    for i=0,loop,1 do
      error = 1;
      pelagia.Set("t2", "a"..i, "b")
    end
    print(loop.." requests in "..((pelagia.MS() - ms)/1000).." seconds for set");

    --set and get
    error = 1;
    r = pelagia.Get("t2", "a0")
    if ( r == "b") then
      error = 0;
    end

    if error == 1 then
      print("fail set and get!\n")
      return 1;
    end
  end

  if loop and (var.argc < 2 or (var.argc >= 2 and var.argv[2] == "get")) then
    --get
    ms = pelagia.MS();
    for i=0,loop,1 do
      error = 1;
      r = pelagia.Get("t2", "a"..i)
    end
    print(loop.." requests in "..((pelagia.MS() - ms)/1000).." seconds for get");
  end
  print("job all pass!\n");
  return 1;
end