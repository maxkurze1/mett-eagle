import faas

def main(arg):
  print "Hello from function, got '" + arg + "'"

  ret = faas.action_invoke(name="function2",arg="hey from fn1");

  return "function: fn2 returned =" + ret