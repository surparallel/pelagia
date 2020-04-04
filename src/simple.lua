local json = require 'dkjson'

function acc_insert(param)
  print("acc_insert:"..param)
  local sparam = json.decode(param);
  pelagia.Set("acc_title", sparam.argv[1], sparam.argv[2])
end

function acc_del(param)
  print("acc_del:"..param)
  local sparam = json.decode(param);
  pelagia.Del("acc_title", sparam.argv[1])
end

function acc_alt(param)
  print("acc_alt:"..param)
  local sparam = json.decode(param);
  pelagia.Set("acc_title", sparam.argv[1], sparam.argv[2])
end

function book_insert(param)
  print("book_insert:"..param)
  local sparam = json.decode(param);
  pelagia.Set("book_level", sparam.argv[1], sparam.argv[2])
end

function book_del(param)
  print("book_del:"..param)
  local sparam = json.decode(param);
  pelagia.Del("book_level", sparam.argv[1])
end

function book_borrow(param)
  print("book_borrow:"..param)
  local sparam = json.decode(param);
  local acc_title = pelagia.Get("acc_title", sparam.argv[1])
  local book_level = pelagia.Get("book_level", sparam.argv[2])
  if book_level == "collection" and acc_title ~= "professor"then
    print("No permission to borrow")
    return 0;
  end
  pelagia.SetAdd("book_records", sparam.argv[1], sparam.argv[2]);
  return 1;
end

function book_returned(param)
  print("book_returned:"..param)
  local sparam = json.decode(param);
  pelagia.SetDel("book_records", sparam.argv[1], sparam.argv[2]);
end

function init_test(param)
  print("main_test:"..param)

  local sparam={};
  sparam.argc = 2;
  sparam.argv = {}
  sparam.argv[1] = "Peter"
  sparam.argv[2] = "professor"
  local jparam = json.encode(sparam);
  pelagia.RemoteCall("acc_insert", jparam)

  sparam.argv[1] = "Walter"
  sparam.argv[2] = "student"
  jparam = json.encode(sparam);
  pelagia.RemoteCall("acc_insert", jparam)

  sparam.argv[1] = "Love and family"
  sparam.argv[2] = "collection"
  jparam = json.encode(sparam);
  pelagia.RemoteCall("book_insert", jparam)

  sparam.argv[1] = "Hamlett"
  sparam.argv[2] = "student"
  jparam = json.encode(sparam);
  pelagia.RemoteCall("book_insert", jparam)

end

function borrow_test(param)
  print("borrow_test:"..param)

  local sparam={};
  sparam.argv = {}
  sparam.argc = 2;
  sparam.argv[1] = "Walter"
  sparam.argv[2] = "Love and family"
  local jparam = json.encode(sparam);
  pelagia.RemoteCall("book_borrow", jparam)

  sparam.argv[1] = "Peter"
  sparam.argv[2] = "Hamlett"
  jparam = json.encode(sparam);
  pelagia.RemoteCall("book_borrow", jparam)
end
