# linux_udp_poll_latency

Measures latency between Linux packet RX and UDP `poll()` return.

## Requirements

Newest Linux kernel packet timestamping interface `SO_TIMESTAMPNS_NEW`. Requires kernel v5.1 or later.

If using an older kernel, you could use plain `SO_TIMESTAMP` which as been around forever but is not Y2038-safe.

## Usage

Generate data:

```
$ echo "a" | nc -n4u -w1 127.0.0.1 9001
```

Example output:

```
$ ./timestamp
Latency between packet and poll return: 123292 ns
 realtime before poll = 1653046981 s 030207629 ns
 packet time          = 1653046982 s 345149554 ns
 realtime after poll  = 1653046982 s 345251334 ns
 monotime after poll  =      47723 s 713906136 ns
 realtime after recv  = 1653046982 s 345272846 ns
```

## Notes

Monotime is provided to correspond with kernel timestamps in `perf script`.

Used https://github.com/bibiboot/kernel_so_timestamp to understand how to setup the control message and iovec.

