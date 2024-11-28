## cputemp - A simple tool to keep CPU below specified temperature on Linux computer

Original distribution site : https://github.com/shibatch/cputemp


### Introduction

Some modern CPUs can reach temperatures near 100 degrees Celsius even
when used at stock clock frequency without overclocking. However, it
is indeed scary to use a CPU regularly near Tjunction. If possible, I
would like to avoid the CPU fan to hum and the high temperature
exhaust to spout out from the back of my PC.

There are various theories, but it is said that the safe temperature
for regular use of CPUs is about 80 degrees Celsius. If we want to use
our CPUs below this temperature, the clock frequency must be
lowered. But how much the frequency should be set to keep the
temperature below 80 degrees depends on the ambient temperature. If a
CPU has many cores, the CPU temperature depends not only on the clock
frequency but also on the usage of each core. More to the point, even
when all cores are running at 100% load, the CPU temperature can vary
considerably depending on what kind of code is being executed. Since
the CPU cooler is a piece of metal and has a good amount of heat
capacity, the CPU temperature will not exceed 80 degrees if the time
of running at a relatively high clock frequency is short.

This tool monitors CPU temperature and dynamically changes the clock
frequency to keep the CPU below the specified temperature on a Linux
computer. This is basically a simple alternative to thermald, but this
tool controls the CPU temperature more aggressively than thermald.


### How to build

1. Check out the source code from our GitHub repository :
`git clone https://github.com/shibatch/cputemp`

2. Run make :
`cd cputemp && make`


### Synopsis

`cputemp [<options>]`


### Description

This tool monitors temperature obtained by the sensor specified by
sensor ID every specified period, and control the CPU clock frequency
so that the temperature is kept below the specified target
temperature.

The CPU clock frequency is controlled by changing the value of
`/sys/devices/system/cpu/cpu*/cpufreq/scaling_max_freq`. This means
that this tool requires the cpufreq driver to honor the value of
scaling_max_freq in order to work correctly. Some CPUs need kernel
parameters to be set appropriately for this. For example, you may
need to add `amd_pstate=passive` to the kernel parameters to use
this tool with AMD CPUs.

To use this tool, you first need to check which sensor gives the CPU
temperature. You can see the list of available sensor IDs by executing
the command without an argument, like the following.

```
$ ./cputemp
Usage : ./cputemp [<options>]

This utility controls the CPU frequency to make its temperature close to the target

Options :
  --sensor <sensor name>         Specify sensor name
  --period <seconds>             Specify period
  --temp <target temperature>    Specify target CPU temperature
  --daemon <pid file name>       Daemonize
  --kill-daemon <pid file name>  Kill already running daemon
  --verbose                      Turn on verbose mode

Available sensors : nvme (41.85 C), k10temp (58.75 C), mt7921_phy0 (50 C), amdgpu (52 C),
$
```

In the above example, there are 4 sensors, and the sensor ID that
gives the CPU temperature is k10temp.

Then, you can specify this sensor ID and start the command as
root. You can see the verbose output of how the temperature is
controlled by `--verbose` option.

```
$ sudo ./cputemp --sensor k10temp --temp 80 --verbose
[sudo] password for shibatch:
Sensor file name : /sys/class/hwmon/hwmon1/temp1_input
Max freq : 5881 MHz
Min freq : 400 MHz
Cur freq : 5488.64 MHz
CPU freq = 476.966 MHz, scaling_max_freq = 400 MHz, CPU temp = 53.75 C, target temp = 80 C
CPU freq = 564.43 MHz, scaling_max_freq = 925 MHz, CPU temp = 53.75 C, target temp = 80 C
CPU freq = 1112.53 MHz, scaling_max_freq = 1450 MHz, CPU temp = 53.75 C, target temp = 80 C
CPU freq = 1496.2 MHz, scaling_max_freq = 1975 MHz, CPU temp = 53.75 C, target temp = 80 C
CPU freq = 2500 MHz, scaling_max_freq = 2500 MHz, CPU temp = 53.75 C, target temp = 80 C
^C
$ 
```

You should be able to observe that the CPU frequency is lowered and
the CPU temperature is kept below the specified temperature by
executing some task in another terminal. If you are satisfied with the
results, you can copy the executable to /usr/local/bin and start it in
/etc/rc.local. In this case, `--daemon` option can be specified to
start this tool as a daemon.

```
$ cat /etc/rc.local
#!/bin/sh

/usr/local/bin/cputemp --sensor k10temp --temp 80 --daemon /var/run/cputemp.pid
$
```

Instead of starting cputemp from /etc/rc.local, you can also start it
using systemd.

To do this, edit cputemp.service and copy it to the
/etc/systemd/system directory :
`sudo cp cputemp.service /etc/systemd/system/`

Next, reload the service file :
`sudo systemctl daemon-reload`

Then, start cputemp service :
`sudo systemctl start cputemp.service`

To enable cputemp service on every reboot :
`sudo systemctl enable cputemp.service`

Typically, there is no need to make any special changes to the
settings for thermald or cpufreq in order to use cputemp.


### License

The software is distributed under the Zero-Clause BSD, which means
that this software is in public domain.

The fact that this software is released under an open source license
only means that you can use the current version of the software for
free. If you want this software to be maintained, you need to
financially support the project. Please see
[CODE_OF_CONDUCT.md](https://github.com/shibatch/nofreelunch?tab=coc-ov-file).

Copyright Naoki Shibata 2024.
