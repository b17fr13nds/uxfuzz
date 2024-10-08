# uxfuzz
##### NOTE: archived - use lxfuzz for linux
uxfuzz is a black-box kernel fuzzer used for unix. it is scalable because of qemu being used to emulate in a way to be able to freely choose the number of instances and their memory.

## setup and run

you can essentially build the fuzzer for any kernel/OS you like, however minor adjustments to header files and standard functions might be needed. when configuring your
kernel, make sure qemu exits if the kernel panics. obviously you'll need an address/memory/... sanitizer to detect most of the vulnerabilities. enabling extra kernel
config options that add more code to be fuzzed is always a good idea.

now, to build the fuzzer simply run
```
make all
```
this will build a custom qemu emulator (x86-64) plus the fuzzer, a reproducer and manager programs for both of those.

before running the manager, you have to configure how your kernel should be started. you can do that by editing the `cmdline.cfg` file (which already contains an example configuration)

you're completely free in choosing how the kernel should be running. however make sure to have qemu exit on a kernel panic or similar. be careful to use the modified qemu emulator. (located in `./tools/qemu-7.1.0/build/`)

if everything is set up you can start the fuzzing manager
```
./fuzz_manager <no of instances> <fuzzer options...>
```
you can choose as many instances as your hardware can take. as of now, you can specify `1` for fuzzer options if you want to make use of user namespaces.
you may need to run the fuzzing manager as root to recieve fuzzing stats and log data.

## logs and crashes

all fuzzing logs are saved in `./kernel/data/`. each instance got an own directory in which each core/thread got an own log file. 

WARNING: the log folders and files grow extremely large after some time. make sure to keep track of them and keep removing old log data (i.e. by a shell script)

if the manager encounters a crash, the whole log directory of the corresponding instance is copied and saved.

to reproduce crashes, copy the folder containing the crashes to the default startup working directory of the machine alongside with the `reproducer` binary. make sure the folder is named `crash/`. running `repro_manager` will try to reproduce the crash and will notify if successful. you can try to reduce the log files in `crash/` while still reproducing successfully, until you get to a point where you're able to understand the crash and create a POC.
