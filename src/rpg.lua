
--[[package.path = './deps/?.lua;./deps/socket/?.lua;'..package.path
package.cpath = './deps/?.dll;./deps/socket/?.dll;'..package.cpath
package.cpath = './deps/?.so;./deps/socket/?.so;'..package.cpath

require 'socket'
local debuggee = require 'vscode-debuggee'

local json = require 'dkjson'
local startResult, breakerType = debuggee.start(json)
print('debuggee start ->', startResult, breakerType)
]]--
local json = require 'dkjson'
function init(param)
  --print("base param:".. param)

  local dvalue = json.decode(param);
  pelagia.Set2("global", "global_x", dvalue.global_x);
  pelagia.Set2("global", "global_y", dvalue.global_y);
  pelagia.Set2("global", "view_x", dvalue.view_x);
  pelagia.Set2("global", "view_y", dvalue.view_y);
  pelagia.Set2("global", "npc_count", dvalue.npc_count);
  pelagia.Set2("global", "food_count", dvalue.food_count);
  pelagia.Set2("global", "wall_count", dvalue.wall_count);

  local npc_count = dvalue.npc_count;
  local food_count = dvalue.food_count;
  local wall_count = dvalue.wall_count;
  local all_count = dvalue.global_x * dvalue.global_y;

  --for player
  for x = 0, dvalue.global_x - 1, 1 do
    for y = 0, dvalue.global_y - 1, 1 do
      repeat
        if(x == 0 and y == 0) then 
          break;
        end 

        local r = math.random(1, all_count);
        
        local Mov_Arg = {};
        Mov_Arg.event = dvalue.event;

        if r <= (all_count - npc_count - food_count - wall_count)  then
            --empty
            all_count = all_count - 1;
            break;
        elseif r <= (all_count - npc_count - food_count) then
            --wall_count
            --1 player, 2 npc , 3 wall, 4 gift
            pelagia.Set2("type", ""..all_count, 3);
            pelagia.Set2("hp", ""..all_count, math.random(1, 10));
            Mov_Arg.type = 3;
            wall_count = wall_count - 1;
        elseif r <= (all_count - npc_count) then
            --food_count
            pelagia.Set2("type", ""..all_count, 4);
            pelagia.Set2("hp", ""..all_count, math.random(1, 20));
            Mov_Arg.type = 4;
            food_count = food_count - 1;
        elseif r <= (all_count) then
            --npc_count
            pelagia.Set2("type", ""..all_count, 2);
            pelagia.Set2("hp", ""..all_count, math.random(10, 30));
            Mov_Arg.type = 2;
            npc_count = npc_count - 1;
        end
        if all_count == 1 then
            break;
        end
        Mov_Arg.id = ""..all_count;--id
        Mov_Arg.x = x;--x
        Mov_Arg.y = y;--y
        local json_arg = json.encode(Mov_Arg);
        pelagia.RemoteCall("entry", json_arg);     
        all_count = all_count - 1;
      until true
      if (npc_count + food_count + wall_count) <= 0  or all_count == 1 then
          break;
      end
    end
    if (npc_count + food_count + wall_count) <= 0 or all_count == 1 then
      break;
    end
  end

  print("npc_count: "..all_count.."    "..dvalue.npc_count.."   "..npc_count);
  --init plaery, 1 player, 2 npc , 3 wall, 4 gift
  pelagia.Set2("type", "1", 1);
  pelagia.Set2("hp", "1", 100);

  pelagia.Commit();
  local Mov_Player_Arg = {};
  Mov_Player_Arg.event = dvalue.event;
  Mov_Player_Arg.id = "1";--id
  Mov_Player_Arg.type = 1;--type
  Mov_Player_Arg.x = 0;--x
  Mov_Player_Arg.y = 0;--y
  local json_arg = json.encode(Mov_Player_Arg);
  pelagia.RemoteCall("entry", json_arg)

  local map_init_Arg = {};
  map_init_Arg.id = "1";
  map_init_Arg.ms = 0;
  map_init_Arg.event = dvalue.event;
  local json_arg = json.encode(map_init_Arg);
  pelagia.RemoteCall("npc_tick", json_arg)
  return 1;
end

function entry(param)
  --print("base param:".. param)
  local dvalue = json.decode(param);
  if (dvalue.type == 1) then
    --empty so begin move
    pelagia.Set2("occupy", dvalue.x.."_".. dvalue.y, dvalue.id);
    pelagia.Set2("x", dvalue.id, dvalue.x);
    pelagia.Set2("y", dvalue.id, dvalue.y);
    
    --return view
    local view_x = pelagia.Get2("global", "view_x");
    local view_y = pelagia.Get2("global", "view_y");

    local global_x = pelagia.Get2("global", "global_x");
    local global_y = pelagia.Get2("global", "global_y");

    local ret_player_arg = {};
    ret_player_arg.msg = "entry";

    --all infomation
    local o_x = math.ceil(dvalue.x - view_x / 2);
    local o_y = math.ceil(dvalue.y - view_y / 2);
    local e_x = o_x + view_x;
    local e_y = o_y + view_y;

    if o_x < 0 then
        o_x = 0;
    end

    if o_y < 0 then
      o_y = 0;
    end

    if e_x >= global_x then
      e_x = global_x - 1;
    end

    if e_y >= global_y then
      e_y = global_y - 1;
    end


    local count = 1;
    ret_player_arg.npc = {};
    for i = o_x, e_x, 1 do
      for j = o_y, e_y, 1 do
        local other_id = pelagia.Get2("occupy", i.."_".. j);
        if other_id ~= nil then
          local type = pelagia.Get2("type", other_id);
          if type ~= 1 then
            ret_player_arg.npc[count] = {};
            ret_player_arg.npc[count].id = other_id;
            ret_player_arg.npc[count].type = type;
            ret_player_arg.npc[count].x = i;
            ret_player_arg.npc[count].y = j;
            count = count + 1;              
          end
      end
      end
    end

    ret_player_arg.id = dvalue.id;
    ret_player_arg.x = dvalue.x;
    ret_player_arg.y = dvalue.y;
    local json_arg = json.encode(ret_player_arg);
    pelagia.EventSend(dvalue.event, json_arg);
  elseif (dvalue.type == 2) or (dvalue.type == 3) or (dvalue.type == 4) then
    --npc wall gift
    --empty so begin move
    pelagia.Set2("occupy", dvalue.x.."_".. dvalue.y, dvalue.id);
    pelagia.Set2("x", dvalue.id, dvalue.x);
    pelagia.Set2("y", dvalue.id, dvalue.y);
    if dvalue.type == 2 then

     -- print(" add_tick npc id "..dvalue.id.. dvalue.x..dvalue.y );
      local map_init_Arg = {};
      map_init_Arg.id = dvalue.id;
      map_init_Arg.ms = pelagia.MS();
      map_init_Arg.event = dvalue.event;
      local json_arg = json.encode(map_init_Arg);
      pelagia.RemoteCall("add_tick", json_arg)
    end
  end

  return 1;
end

function add_tick(param)
  local dvalue = json.decode(param);
  pelagia.Set2("tick", dvalue.id, dvalue.ms);
end

function play_move(param)
  --print("base param:".. param)
  local dvalue = json.decode(param)

  --player
  local src_x = pelagia.Get2("x", "1");
  local src_y = pelagia.Get2("y", "1");

  local global_x = pelagia.Get2("global", "global_x");
  local global_y = pelagia.Get2("global", "global_y");

  local x = dvalue.x + src_x;
  local y = dvalue.y + src_y;

  local ret_player_arg = {};
  ret_player_arg.msg = "play_move";

  if (x < 0 or x >= global_x) or (y < 0 or y >= global_y) then
      ret_player_arg.npc = {};
      ret_player_arg.id = "1";
      ret_player_arg.sx = src_x;
      ret_player_arg.sy = src_y;
      ret_player_arg.x = src_x;
      ret_player_arg.y = src_y;
      local json_arg = json.encode(ret_player_arg);
      pelagia.EventSend(dvalue.event, json_arg);
      return;
  end

  local occupy_id = pelagia.Get2("occupy", x.."_".. y);

  if occupy_id == nil then

    --empty so begin move
    pelagia.Del2("occupy", src_x.."_".. src_y);
    pelagia.Set2("occupy", x.."_".. y, "1");
    pelagia.Set2("x", "1", x);
    pelagia.Set2("y", "1", y);
    
    --return view
    local view_x = pelagia.Get2("global", "view_x");
    local view_y = pelagia.Get2("global", "view_y");
    
    --incr information
    if dvalue.x ~= 0 then
      local jy;
      if src_y > y then
        jy =  y - view_y / 2;
      else
        jy =  y + view_y / 2;
      end

      local count = 1;
      ret_player_arg.npc = {};
      for i = x - view_x / 2, x + view_x / 2, 1 do
        if i >= 0  and jy >= 0 then
          local other_id = pelagia.Get2("occupy", i.."_".. jy);
          if other_id ~= nil then
            local type = pelagia.Get2("type", other_id);
            ret_player_arg.npc[count] = {};
            ret_player_arg.npc[count].id = other_id;
            ret_player_arg.npc[count].type = type;
            ret_player_arg.npc[count].x = i;
            ret_player_arg.npc[count].y = jy;
            count = count + 1;
          end              
        end
      end

    elseif 0 ~= dvalue.y then
      local ix;
      if src_x > x then
        ix =  x - view_x / 2;
      else
        ix =  x + view_x / 2;
      end

      local count = 1;
      ret_player_arg.npc = {};
      for j = y - view_y / 2, y + view_y / 2, 1 do
        if j >=0  and ix >= 0 then
          local other_id = pelagia.Get2("occupy", ix.."_".. j);
          if other_id ~= nil then
            local type = pelagia.Get2("type", other_id);
            ret_player_arg.npc[count] = {};
            ret_player_arg.npc[count].id = other_id;
            ret_player_arg.npc[count].type = type;
            ret_player_arg.npc[count].x = ix;
            ret_player_arg.npc[count].y = j;
            count = count + 1;
          end
        end
      end
    end

    ret_player_arg.id = "1";
    ret_player_arg.sx = src_x;
    ret_player_arg.sy = src_y;
    ret_player_arg.x = x;
    ret_player_arg.y = y;
    local json_arg = json.encode(ret_player_arg);
    pelagia.EventSend(dvalue.event, json_arg);

  else
    --attack
    local type = pelagia.Get2("type", occupy_id);

    --3 wall, 4 gift
    if type == 3 or type == 4 then

      --remove      
      local hp = pelagia.Get2("hp", occupy_id);
      if (type == 3 and hp == 0) or type == 4 then
        pelagia.Del2("occupy", x.."_".. y);
        local ret_player_arg = {};
        ret_player_arg.id = "1";--id
        ret_player_arg.target = occupy_id;--target
        ret_player_arg.x = x;
        ret_player_arg.y = y;
        ret_player_arg.del = 1;
        ret_player_arg.event = dvalue.event;
        local json_arg = json.encode(ret_player_arg);
        pelagia.RemoteCall("play_attack", json_arg);
      elseif (type == 3 and hp ~= 0) or type == 4 then
        --attack
        local Attack_Arg = {};
        Attack_Arg.id = "1";--id
        Attack_Arg.target = occupy_id;--target
        ret_player_arg.x = x;
        ret_player_arg.y = y;
        ret_player_arg.del = 0;
        Attack_Arg.event = dvalue.event;
        local json_arg = json.encode(Attack_Arg);
        pelagia.RemoteCall("play_attack", json_arg);
      end    
    end

    ret_player_arg.npc = {};
    ret_player_arg.id = "1";
    ret_player_arg.sx = src_x;
    ret_player_arg.sy = src_y;
    ret_player_arg.x = src_x;
    ret_player_arg.y = src_y;
    local json_arg = json.encode(ret_player_arg);
    pelagia.EventSend(dvalue.event, json_arg);
    return;
  end
end

function play_attack(param)
  --print("base param:".. param)

  local dvalue = json.decode(param)
  local fire = 1;
  local hp_hero = pelagia.Get2("hp", dvalue.id);
  local hp = pelagia.Get2("hp", dvalue.target);
  local type = pelagia.Get2("type", dvalue.target);

  --3 wall, 4 gift
  if type == 3 then
    local ret = 0
    if hp >= fire then
      ret = hp - fire;
    end
    hp = 0;
    pelagia.Set2("hp", dvalue.target, ret);
  elseif type == 4  then
      hp_hero = hp_hero + hp;
      pelagia.Set2("hp", dvalue.id, hp_hero);
  end
  
  local ret_atatck_arg = {};
  ret_atatck_arg.msg = "play_attack";
  ret_atatck_arg.id = dvalue.id;
  ret_atatck_arg.target = dvalue.target;
  ret_atatck_arg.x = dvalue.x;
  ret_atatck_arg.y = dvalue.y;
  ret_atatck_arg.del = dvalue.del;
  ret_atatck_arg.hp = hp;
  local json_arg = json.encode(ret_atatck_arg);      
  pelagia.EventSend(dvalue.event, json_arg);

  return 1;
end

function npc_attack(param)
  --print("base param:".. param)
  local dvalue = json.decode(param)
  local aggress = pelagia.Get2("hp", dvalue.id);
  local hp = pelagia.Get2("hp", dvalue.target);

  if hp >= aggress then
    hp = hp - aggress;
    pelagia.Set2("hp", dvalue.target, hp);
  else
    hp = 0;
    pelagia.Set2("hp", dvalue.target, hp);
  end
  
  local ret_atatck_arg = {};
  ret_atatck_arg.msg = "npc_attack";
  ret_atatck_arg.id = dvalue.id;
  ret_atatck_arg.target = dvalue.target;
  ret_atatck_arg.hp = hp;
  ret_atatck_arg.src_x = dvalue.src_x;--target
  ret_atatck_arg.src_y = dvalue.src_y;--target
  local json_arg = json.encode(ret_atatck_arg);      
  pelagia.EventSend(dvalue.event, json_arg);

  return 1;
end

function npc_move(param)
  --print("npc_move base param:".. param)
  local dvalue = json.decode(param)

  --npc
  local src_x = pelagia.Get2("x", dvalue.id);
  local src_y = pelagia.Get2("y", dvalue.id);

  local player_x = pelagia.Get2("x", "1");
  local player_y = pelagia.Get2("y", "1");

  local view_x = pelagia.Get2("global", "view_x");
  local view_y = pelagia.Get2("global", "view_y");

  if math.abs(player_x - src_x) <= 1 and math.abs(player_y - src_y) <= 1  then
    --attack
    local Attack_Arg = {};
    Attack_Arg.event = dvalue.event;
    Attack_Arg.id = dvalue.id;--id
    Attack_Arg.target = "1";--target
    Attack_Arg.src_x = src_x;--target
    Attack_Arg.src_y = src_y;--target
    local json_arg = json.encode(Attack_Arg);
    pelagia.RemoteCall("npc_attack", json_arg);
    return 1;
  end

  local des_x, des_y;
  local xy = math.random(1, 2);
  local player_r = math.random(1, 4);
  
  if(xy == 1) then
    des_y = src_y;
    if math.abs(player_x - src_x) < view_x then
      if player_r ~= 1 then
        --in view
        if player_x > src_x then
          des_x = src_x + 1;
        elseif player_x < src_x then
            des_x = src_x - 1;
        end
      end
    end

    local r = math.random(1, 2);
    if des_x == nil then
      if  r == 1 then
        des_x = src_x + 1;
      else
        des_x = src_x - 1;
      end
    end 
  else
    des_x = src_x;
    if math.abs(player_y - src_y) < view_y then
      if player_r ~= 1 then
        --in view
        if player_y > src_y then
          des_y = src_y + 1;
        elseif player_y < src_y then
          des_y = src_y - 1;
        end
      end
    end
  
    local r = math.random(1, 2);
    if des_y == nil then
      if  r == 1 then
        des_y = src_y + 1;
      else
        des_y = src_y - 1;
      end
    end
  end

  local global_x = pelagia.Get2("global", "global_x");
  local global_y = pelagia.Get2("global", "global_y");

  if not(des_y == src_y and des_x == src_x) and (des_x >= 0 and des_y >= 0) and (des_x < global_x and des_y < global_y) then

    local ret_npc_arg = {};
    ret_npc_arg.msg = "npc_move";
    local occupy_id = pelagia.Get2("occupy", des_x.."_".. des_y);
    if occupy_id == nil then

      --empty so begin move
      pelagia.Del2("occupy", src_x.."_".. src_y);
      pelagia.Set2("occupy", des_x.."_".. des_y, dvalue.id);
      pelagia.Set2("x", dvalue.id, des_x);
      pelagia.Set2("y", dvalue.id, des_y);

      local left_x = player_x - view_x / 2;
      local right_x = player_x + view_x / 2;

      if left_x < 0 then
        left_x = 0;
      end

      if right_x >= global_x then
        right_x = global_x - 1;
      end

      local left_y = player_y - view_y / 2;
      local right_y = player_y + view_y / 2;

      if left_y < 0 then
        left_y = 0;
      end

      if right_y >= global_y then
        right_y = global_y - 1;
      end

      --in view
      if ((src_x >= left_x and src_x <= right_x) and (src_y >= left_y and src_y <= right_y)) or ((des_x >= left_x and des_x <= right_x) and (des_y >= left_y and des_y <= right_y))then
        ret_npc_arg.id = dvalue.id;
        ret_npc_arg.sx = src_x;
        ret_npc_arg.sy = src_y;
        ret_npc_arg.x = des_x;
        ret_npc_arg.y = des_y;
        if (des_x < left_x or des_x > right_x) or (des_y < left_y or des_y > right_y) then
          ret_npc_arg.del = 1;
        else
          ret_npc_arg.del = 0;
        end
        local json_arg = json.encode(ret_npc_arg);
        pelagia.EventSend(dvalue.event, json_arg);
      end

    else
      
      --player
      local type = pelagia.Get2("type", occupy_id);
      if type == 1 then

        --attack
        local Attack_Arg = {};
        Attack_Arg.event = dvalue.event;
        Attack_Arg.id = dvalue.id;--id
        Attack_Arg.target = occupy_id;--target
        Attack_Arg.src_x = src_x;--target
        Attack_Arg.src_y = src_y;--target
        local json_arg = json.encode(Attack_Arg);
        pelagia.RemoteCall("npc_attack", json_arg);
      end
    end
  end
end

function npc_tick(param)
  --print("base param:".. param)
  local dvalue = json.decode(param)
  local count = 0;
  local next = dvalue.id;

  local Tick_Arg = {};
  Tick_Arg.event = dvalue.event;

  local send = 0;
  local ms = pelagia.MS(); 
  if (ms - dvalue.ms)/1000 <= 1 then
    send = 1;
    local wait = 1 - (ms - dvalue.ms)/1000;
    Tick_Arg.id = "1";--id
    Tick_Arg.ms = ms;
    local json_arg = json.encode(Tick_Arg);
    pelagia.Timer(wait, "npc_tick", json_arg);
    --print("npc_tick Timer:"..next);
  end
 
  for i = 1, 10000, 1 do
    local lv;
    if next ==  "1" then 
      local rjson = pelagia.Order2("tick", 0, 1);
      local rd = json.decode(rjson);
      next = nil;
      for k, v in pairs(rd) do
        next = k;
        lv = v;
        break;
      end
  else
    local rjson = pelagia.Point2("tick", next, 1, 1);
    local rd = json.decode(rjson);
    next = nil;
    for k, v in pairs(rd) do
      next = k;
      lv = v;
      break;
    end
  end


    if next == nil then
      break;
    else
      --print("npc id "..next);
      if (ms - lv)/1000 >= 4 then
        pelagia.Set2("tick", next, ms);
        local Attack_Arg = {};
        Attack_Arg.event = dvalue.event;
        Attack_Arg.id = next;--id
        local json_arg = json.encode(Attack_Arg);
        pelagia.RemoteCall("npc_move", json_arg);       
      end
    end
  end

  if next == nil then
    next = "1";
  end

  if send == 0 then
    Tick_Arg.id = next;--id
    Tick_Arg.ms = ms;
    local json_arg = json.encode(Tick_Arg);
    pelagia.RemoteCall("npc_tick", json_arg);
    --print("npc_tick RemoteCall:"..next);
  end
end

function test(param)
  print("base param:".. param)
  local dvalue = json.decode(param)

  local map_init_Arg = {};
  map_init_Arg.global_x = 100;
  map_init_Arg.global_y = 100;--id
  map_init_Arg.view_x = 10;
  map_init_Arg.view_y = 10;
  map_init_Arg.npc_count = 8000;
  map_init_Arg.food_count = 300;
  map_init_Arg.wall_count = 300;
  map_init_Arg.event = dvalue.event;
  local json_arg = json.encode(map_init_Arg);
  pelagia.RemoteCall("init", json_arg)

  return 1;
end

function test2(param)
  print("base param:".. param)
  local dvalue = json.decode(param)

  for i = 1, 500, 1 do
      pelagia.Set2("tick", ""..i, "");
  end

  print("test2 RemoteCall");
  local map_init_Arg = {};
  map_init_Arg.id = "1";
  map_init_Arg.ms = 0;
  local json_arg = json.encode(map_init_Arg);
  pelagia.RemoteCall("npc_tick", json_arg)

  return 1;
end
