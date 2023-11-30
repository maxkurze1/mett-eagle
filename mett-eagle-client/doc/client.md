# Mett Eagle client

This package provides a benchmark implementation of a Mett-Eagle client.

This can be invoked by ned as follows:

```lua
local channel = L4.default_loader:new_channel()

L4.default_loader:startv({
  caps = {
        server = channel:svr(),
        ...
  },
  ...
}, "rom/mett-eagle")

L4.default_loader:startv({
    caps = {
        manager = channel,
    },
    log = log,
}, "rom/mett-eagle-client", "--threads", "10", "--iterations", "1000")
```

Note: The benchmark client [configuration](../../mett-eagle.cfg) forwards the
command line argument `CLIENT_ARGS` instead of passing the number of threads and
iterations explicitly.
