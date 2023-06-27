# Mett Eagle

Mett Eagle is an L4 server that provides serverless function execution. Clients
can upload serverless functions and invoke them.

# Multithreading

## Threads

The manager will have a main thread that is constantly waiting for new clients
to register. On registration a new thread (inside the manager) will be created
for each client. This thread is responsible for all requests of that specific
client and for all workers that are started by it. The threads will end when the
client leaves.

TODO notification on capability revocation??  
TODO add exit() function for client.

## Cores

Each new thread serving a client will get assigned to a new core. This thread as
well as all faas functions started by the client and recursively by its
functions will be executed on this core.

## IPC

The per client thread will wait for incoming ipc messages from the client. If
the client invokes a faas function, a new worker process will be created and the
invoke will block the client until the worker is finished or crashes. Because
the reply capability is needed to answer the clients invoke, the thread has to
use a closed wait to communicate with the currently running worker. This
communication is necessary to e.g. get notified if the worker exits or wants to
request the start of another faas function recursively. If a worker invokes
another functions it also will be blocked until the function returns.
