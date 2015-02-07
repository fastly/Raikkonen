# Räikkönen

Räikkönen tests races.

## Wat?

[Kimi Räikkönen][1] is a Formula-1 race car driver. This is a utility that
tests race conditions in software. The joke is only funny when you explain
it.

It is written as a combination race condition tester and debugger. Often
it seems easy to identify the conditions required for a race condition to
occur. In nearly all cases, it is excruciatingly difficult to replicate the
race. In some cases, although the race exists in theory, the program must
make an impossible state transition. Often, simply being able to reach a
particular program state is enough to prove the existence of a race
condition.

Räikkönen exists to help reason about races by making them repeatably
testable through enforcing particular execution histories. By carefully
controlling state transitions within a program, race conditions can be
effectively understood, reasoned about, and confidently corrected.

This software does not detect race conditions and is not intended for that
purpose.

## Overview

Räikkönen has a DSL for describing code interactions and how to run code in
such a way that data races are exacerbated. It differs significantly from
existing software like [Spin][2] and [ThreadSanitizer][3] in that it relies
on being able to modify source code to enforce particular execution orders
and execution histories.

Both Spin and ThreadSanitizer are fantastic utilities, but aren't always
practical. Spin is extremely useful for demonstrating safety of concurrent
algorithms, but faces insurmountable issues with state space explosion when
integrated into programs of significant complexity.

ThreadSanitizer is fast and extremely useful for identifying races, but
doesn't guarantee that any particular race occurs (and cannot detect races
when they do not happen). It is therefore not suitable for proof-by-negation
that a particular race is fixed by a code change. Additionally, tsan does
not provide the developer with any means of understanding the conditions
required to exacerbate a race. Räikkönen allows developers to force
particular execution orders, helping them to understand more fully the
conditions required to make a race occur.

That all said, tsan is a fantastic utility and when used in conjunction with
Räikkönen, it can help proving elimiation of data races by negation.

Räikkönen requires source code instrumentation and pauses execution at key
intervals to guarantee execution order. Its overhead otherwise is minimal.
It runs a scheduler thread alongside the program that communicates with
an external test program. This external test program lets the scheduler
know which states it should wait for and how to schedule threads once a
particular state is achieved.

## Design

Räikkönen is enabled by calling `rk_start`. This function will likely need
to be called prior to spawning other threads, and must be called from the
process being tested. This function spawns and detaches a thread and 
then blocks. The spawned thread waits for a configuration to be delivered.
When the configuration is fully delivered, the scheduler thread allows the
program to resume.

The configuration contains program and test state information including
which states are interesting for the particular test and what to do when
particular states are achieved. As the program executes, it uses inline
functions (inserted by the developer) that read the configuration state
and behave as specified when interesting states are achieved. When Räikkönen
is disabled, these function bodies are empty and these function calls are
no-ops.

Since the configuration is not modified at runtime, it is safe for reading
by multiple threads. Threads are only serialized when the configuration
mandates they wait on further states to be achieved.

### Protocol

The protocol describing race tests is not designed to be interactive, but
is designed to make it easy to compile a test description to a format
easily consumable by a Räikkönen backend implementation. The protocol is
therefore not designed to be human readable. 

The protocol is called "Finnish" and its commands and bytecode are modeled
on the [Finnish language][4]. I don't actually speak Finnish, so if any of
this is doesn't make real sense, that's a good enough reason to bump the
protocol version.

All values are full-width and packed. Any value specified in a size
greater than 1 byte is encoded in network byte order.

#### Server

##### Ei

The `ei` command is a response sent by the server when it encounters a
request that it deems to be invalid, that it can't understand, or for any
number of other reasons it may have encountered an error. The `ei` response
is two bytes:

    0x65 0x69
     e    i

If the server sends `ei`, it must close the connection.

##### Joo

The `joo` response signifies that the server has accepted a client request
or command. This response is 3 bytes:

    0x6a 0x6f 0x6f
     j    o    o

##### Hei hei

This response is sent by the server when the client has signified it is done
with its requests.

    0x68 0x65 0x69 0x20 0x68 0x65 0x69
     h    e    i         h    e    i

##### Vaihtaa

When an epoch transition occurs, the server can notify the client. When
this occurs, the server sends a `vaihtaa` packet with the ID of the new time
slice.

    0x76 0x61 0x69 0x68 0x74 0x71 0x71 0x00000000
     v    a    i    h    t    a    a    new epoch

#### Client

##### Hei

A `hei` request initiates a discussion between a client and a Räikkönen
server. The command is 5 bytes and has the format:

    0x68 0x65 0x69  0x0000  
     h    e    i    dialect

The server may respond with `ei` and close the connection for a number of
reasons:

 * It has already serviced a previous client
 * It is in the process of serving a client
 * It does not understand the dialect of Finnish the client speaks
 * It encounters some other internal error

If the server wishes to continue the discussion with the client, it responds
with a `joo` response.

The only dialect defined at this time is 0x0000.

##### Ota se / loppu

The `ota se` request is a a 14 byte packet followed by a variable amount of
data. The 14 byte header packet looks like:

    0x6f 0x74 0x61 0x20 0x73 0x66 0x00000000 0x00000000
     o    t    a         s    e

This packet is followed by `length` bytes of data, which checksum to the value
in `crc32`. The client sends the bytes:

    0x6c 0x6f 0x70 0x70 0x75
     l    o    p    p    u

immediately after sending `length` bytes of data.

The server may respond with `ei` and close the connection if:

 * It times out before receiving `length` bytes of data. The server-side
   timeout value is intentionally undefined.
 * The computed checksum after receiving `length` bytes of data does not match
   the value of `crc32`.
 * The client sends any bytes other than 0x6c 0x6f 0x70 0x70 0x75 after
   sending `length` bytes of data
 * The data sent after the header is not a valid compiled Kimi specification.

If the server accepts the data, it replies with a `joo` response.

##### Hei hei

When the client is finished with it's `ota se` request, it sends `hei hei` to
signify it is done. If the client disconnects before sending `hei hei`, a server
*MAY* interpret a valid `ota se` request as completed.

The server may respond with `ei` (signifying an error) or `hei hei`.

##### Jatka

When a `vaihtaa` packet is received, the client must respond with `jatka` once
it is finished with its asynchronous processing. This packet is 5 bytes:

     0x6a 0x61 0x74 0x6b 0x61
      j    a    t    k    a

#### Bytecode

The Kimi language specified below is compiled to bytecode sent during the
`ota se` request, and is therefore considered part of the protocol. Changes to
bytecode generation or the protocol MUST result in a new dialect.

The bytecode for the language is described in-line with the language
specification, for ease of reading purposes.

### Language

The DSL for Räikkönen specifies expected states, actions associated with
those states, and state transitions between and within threads in a program.
The language is called Kimi.

All language instructions are specified on single lines. Collapsing of
commands on single lines is not permitted.

#### Comments

Kimi supports single-line, hash-style comments anywhere in the code. Any data
appearing from the hash to the end of the line is considered a comment.

Comments are ignored for purposes of bytecode generation.

#### Epochs

Epochs are abstract representations of program state at any particular time.
All commands in Kimi take place within the scope of an epoch. An epoch is
specified by `t[N]` as the first non-whitespace data on a line. Since time
increases monotonically, so must `N` (this is enforced).

Räikkönen transitions into the next epoch when one of two conditions are true:

 1. The specification for the epoch ends with `waitstate` and all states
    specified up to that point have been fulfilled.

 2. The specification for the epoch does not end with waitstate and all
    operations in the epoch are completed.

Commands specified within a epoch are executed sequentially.

The maximum value of `N` is `2^32 - 1`. Negative time values are invalid.

Because it may be useful to interact with the running program while the
scheduler manages thread execution, the scheduler can optionally send a
notification over the calling socket to notify of epoch transitions. The
scheduler aborts execution if a client requests a notification on state
transition, but is unreachable when the transition is made.

Notifications are flagged by specifying e.g. `tn[0]` instead of `t[0]`.

##### Bytecode

Timeslices are specified in 12 bytes:

    0x76 0x04 0x6c 0x00 0x00000000 0x00
                         slice id  notify?

The `notify` byte is 0 when disabled and 1 when enabled. Any other value is
invalid.

Timeslices are terminated with the bytes:

    0xde 0xad 0x76 0x00

If these bytes are seen while looking for a new command, the following bytes
*MUST* define a new epoch.

#### Define

The `define` command instructs the bytecode compiler how to compile state and
callback names. These exist purely for readability purposes; internally, they
are referred to numerically (both for ease-of-implementation and performance)
in the tested program. For example:

    define STATE_SCHEDULER_PRERACE 0
    define STATE_PRERACE 1
    define STATE_RACE 2

    define CALLBACK_FOO 0
    define CALLBACK_BAR 1

The defined values must match with the values of the same name in the tested
application for the tests to work.

State names may only contain ASCII-safe "word" characters as defined by an
unmodified `\w` in the `perlre` character class specification. You cannot
write Kimi scripts in Finnish.

##### Bytecode

This does not have a direct bytecode interpretation. It is simply there to
instruct further state-reliant instructions in how to represent the state
value. The minimum value is 0, the maximum value is `2^32 - 1`.

#### Trigger

Triggers allow us to specify when states should be entered. A trigger can
be specified to execute on function entry, function exit, when a particular
source file / line tuple, or binary / address tuple is executed. Triggers
function similarly to the `define` command in that they represent state or
callback behavior. However, a `trigger` command does result in bytecode that
is sent to the traced process, and can allow tracing to occur without
manual program instrumentation.

The `trigger` command has a few forms:

    trigger state STATE_SCHED src/sched.c:123
    trigger state STATE_FOO libfoo.so:foo_start
    trigger state STATE_BAR foobard:main
    trigger callback CALLBACK_ENTRY handle_session:entry
    trigger callback CALLBACK_EXIT handle_session:exit

Because triggers implement dynamic instrumentation functionality, it is
important to ensure that one enables Raikkonen prior to any code execution
that may have an associated trigger.

Triggers may be specified globally, in which case they will always execute.
They may also be specified in the context of an epoch, in which case they
will only execute if the program is currently executing within that state.

##### Bytecode

    0x74 0x65 0x68 0x64 0xe4 0x00 0x00000000 0x0000 ... 0x00
    prologue                 s/c? id         len    len end

 * `s/c?` is a single byte that declares whether we are entering a state or
   executing a callback. A value of 0 represents "state", any non-zero value
   means "callback".
 * `id` is the ID of the state or callback to be entered or executed.
 * `len` is 2 bytes signifying a variable length string containing the tuple
   to process the trigger
 * A trailing NUL byte after `len` bytes signifies the end of the message.

#### When

The `when` command provides an interface for specifying behavior when a
particular state is achieved. This command specifies how to treat all threads
that reach the state. For example:

    t[0]
        when STATE_SCHEDULER_PRERACE
            1: wait
            N: panic
        end

This specifies that a single thread should reach the state
*STATE_SCHEDULER_PRERACE*. Any further threads reaching this state will cause
a panic. Thread ranges may also be specified:

    t[0]
        when STATE_PRERACE
            1-2: continue
            3-4: wait
            N: wait
        end

This behavior is useful when the racy state is achieved only after some number
of threads transition through it. In this case, the first two threads continue
execution. The next two are paused. The rest of the threads entering this state
are also paused; however, they are tracked within a different grouping.

Valid commands for actions are:

 * `callback CB`: Causes the callback named by `CB` to be called. The callback
   receives the state as its first argument and any data specified at the
   callsite (or `NULL`). When the callback returns, the thread forgets that it
   has entered the state.

 * `continue`: Causes the state-transitioning thread to continue execution and
   possibly enter a future state. This thread does not remember that it has
   entered the state.
 
 * `panic`: Halt execution of the program and exit with a non-zero status.

 * `sleep N`: Sleep for a period of `N` units of realtime. `N` may be
   specified in seconds, milliseconds, microseconds, or nanoseconds by
   suffixing the number with `s`, `ms`, `μs` or `us`, or `ns`. The default is
   to consider the time value units in seconds. When the sleep returns, the
   thread forgets it ahs entered the state.

 * `wait`: Pauses the thread and places it in a group of 1 or more threads.
   The thread forgets that it is in the state if it is resumed.

Ordering of `when` commands is unimportant with respect to other commands
within an epoch, since they only define expected states and they are
interpreted at initialization time. However, ordering of these expectations
within epochs is extremely important: a "happens-before" state specified in an
epoch after it was logically reached will have a net effect of 0.

A Kimi program:

    t[1]
        when STATE_RACE
		1: wait
		N: panic
	end
	waitstate
	when STATE_RACE2
		1: wait
		N: panic
	end

Will wait on both *STATE_RACE* and *STATE_RACE2* to be achieved in the
specified epoch.

Thread ranges for a state once specified cannot change. For example:

    t[0]
        when STATE_PRERACE
            1-2: wait
            N: panic
        end
        waitstate

    t[1]
        when STATE_PRERACE
            1: wait
            2: continue
            N: panic
        end
        resume STATE_PRERACE[1-2]
        waitstate

This specification is invalid because it breaks up the grouping at the first
definition of the state. Changing the first specification in `t[0]` to:

    when STATE_PRERACE
        1: wait
        2: wait
        N: panic
    end

would be sufficient to make this specification valid.

When conditions *must* specify the behavior for all threads entering the state.
Since Räikkönen can't possibly know how many threads will enter any given
state, the implementer must specify the behavior. Therefore, every `when` block
*must* end with an `N` range specifier.

##### Bytecode

The prologue for `when` bytecode is:

    0x6a 0x6f 0x73 0x00 0x00000000 0x00
                         STATE_*

Thread ranges are 8 bytes long: 4 bytes for the start of the range and 4 bytes
for the end value. The special range value `N` is always `0xffffffff`.
Although the language does not require ranges to be specified for individual
values, the bytecode always contains a range. A single value range contains
the same value for the start and end value of the range. A single value range
of `N` contains a start value one higher than the last end value specified in
the `when` block.

This is followed by a 2-byte command and up to 8 bytes for the command
arguments (if they exist). Commands are:

    0x0000 callback
    0x0001 continue
    0x0002 panic
    0x0004 sleep
    0x0008 wait

A callback command is suffixed with a 4-byte ID of the callback.

A sleep command is trailed by one byte specifying the unit type and 4 bytes
containing the sleep duration. The possible unit values are:
   
 * `0x00` - seconds
 * `0x01` - milliseconds
 * `0x02` - microseconds
 * `0x03` - nanoseconds

`When` blocks are terminated by the bytes

    0xde 0xad 0x6a 0x00

Any bytecode following this *MUST* either terminate the epoch or define a new
command. This `when` block terminator also implies that the thread ID
`3735906816` is reserved. If you have this many threads, this software is
probably obsolete.

For clarity, consider the following example and its resulting bytecode:

    # define STATE_PRERACE 0
    # define STATE_RACE 127
    # define CB_FOO 0

    # when STATE_PRERACE
    0x6a 0x6f 0x73 0x00 0x00000000 0x00
        # 1-2: continue
        0x00000001 0x00000002 0x0001

        # 3-4: wait
        0x00000003 0x00000004 0x0008

        # N: sleep 50ns
        0x00000005 0xffffffff 0x0004 0x03 0x00000032

    # end
    0xde 0xad 0x6a 0x00

    # when STATE_RACE
    0x6a 0x6f 0x73 0x00 0x0000007f 0x00
        # 1: callback CB_FOO
        0x00000001 0x00000001 0x0000 0x00000000

        # N: panic
        0x00000002 0xffffffff 0x0002

    # end
    0xde 0xad 0x6a 0x00

#### Resuming Threads

Threads waited on in a previous epoch may be resumed by specifying
`resume STATE_FOO[<group>]`, where group represents the waited-on threads.
For example:

    t[0]
        when STATE_PRERACE
            1: wait
            2-3: wait
            N: panic
        end
        waitstate

    t[1]
        resume STATE_PRERACE[2-3]
        resume STATE_PRERACE[1]

This waits on 3 threads to enter *STATE_PRERACE*, and resumes threads 2 and 3
prior to resuming thread 1.

Between two epochs e1 and eN, at least one `waitstate` *must* exist in the
range `[e1, eN)`.

##### Bytecode

Resume commands are 16 bytes. A 4 byte prologue, 4 bytes to represent the
state name, 4 bytes for the start value of the thread range, and 4 bytes for
the end value of the thread range.

    0x6a 0x04 0x61 0x00 0x00000000 0x00000000 0x00000000
    prefix               STATE_*    START      END

#### Timeout

A test can be considered passing when a timeout succeeds within an epoch. A
timeout occurring within a timeslice causes further commands to be delayed by
at least the specified time. Time specifications follow the same constraints
as in `sleep` commands to `when`.

##### Bytecode

Timeouts consist of a 4 byte prologue followed by a time specification of the
same format as the `sleep` command.

    0x75 0x6e 0x69 0x00 0x00 0x00000000
                        unit  duration

#### Waitstate

The `waitstate` command forces the scheduler thread to wait for all specified
states to be achieved before moving to the next epoch.

##### Bytecode

This command consists of 4 bytes:

    0x6f 0x05 0x61 0x00

#### Line-based nature

Kimi is a line-based language. Every line of the file is either:

 1. A comment,
 2. Empty with optional whitespace,
 3. Optional whitespace, command specification, optional whitespace and an
    optional comment.

Specifications that collapse lines are invalid. All the following are invalid
specifications:

    when STATE_PRERACE 1: continue end

    when STATE_PRERACE
        1: continue N: wait
    end

    resume STATE_PRERACE[1] waitstate

This restriction is intended both to keep Kimi scripts easy to read as well as
to make the bytecode compiler easy to write.

#### Examples

Here are a couple example specifications.

```
define STATE_VBP_POLL 0
define STATE_CLI_STOP 1
define STATE_CLI_STOP_REAPED 2

# Wait for poll thread and cli thread to hit critical points
t[0]
        when STATE_VBP_POLL
                1: wait
                N: continue
        end
        when STATE_CLI_STOP
                1: wait
                N: panic
        end
        waitstate

# Install handler for next CLI thread state and allow it to
# resume
t[1]
        when STATE_CLI_STOP_REAPED
                1: wait
                N: panic
        end
        resume STATE_CLI_STOP[1]
        waitstate

# If we timeout here, there's no way the race happened. Otherwise
# we crashed.
t[2]
        timeout 60
```

The bytecode for this Kimi program is:

```
76 04 6c 00 00 00 00 00  00 6a 6f 73 00 00 00 00
00 00 00 00 00 01 00 00  00 01 00 08 00 00 00 02
ff ff ff ff 00 01 de ad  6a 00 6a 6f 73 00 00 00
00 01 00 00 00 00 01 00  00 00 01 00 08 00 00 00
02 ff ff ff ff 00 02 de  ad 6a 00 6f 05 61 00 de
ad 76 00 76 04 6c 00 00  00 00 01 00 6a 6f 73 00
00 00 00 02 00 00 00 00  01 00 00 00 01 00 08 00
00 00 02 ff ff ff ff 00  02 de ad 6a 00 6a 04 61
00 00 00 00 01 00 00 00  01 00 00 00 01 6f 05 61
00 de ad 76 00 76 04 6c  00 00 00 00 02 00 75 6e
69 00 00 00 00 00 3c de  ad 76 00
```

------
```
# Put poller to sleep for a good long time.
t[0]
        when STATE_VBP_POLL
                1: sleep 120
                N: continue
        end
        when STATE_CLI_STOP
                1: wait
                N: panic
        end
        waitstate

# t[1] only executes when all t[0] states are fulfilled. If the
# timeout finishes, we couldn't have raced. Otherwise, the parent
# killed us.
t[1]
        resume STATE_CLI_STOP[1]
        timeout 120
```

## Backends

It should be possible with some ease to integrate Raikkonen into other
platforms and languages by wrapping its public C API.

## Integration

Integrating Raikkonen into your software is relatively simple:

 1. Add `-DRK_ENABLED` to your project's CFLAGS
 2. Get a reference to the running configuration by calling `rk_config_get()`
 3. Register all implemented states you will use with `rk_state_register`.
    The return value of this function gives you the index you should use when
    waiting on the state in software.
 4. Set up a `union rk_sockaddr` with the address and port you would like to
    use for pushing the config to the running program.
 5. Call `rk_start`, passing the address of your `union rk_sockaddr`.

A minimal [test file][5] shows this process.

The instrumented binary will pause until a configuration is loaded. To load a
configuration, first compile it with `bin/kimi.pl -i in.km -o out.fi`. Then
send the configuration to your program by running
`bin/fi_client.pl -a "addr:port" -i out.fi`.

## Sidenotes

### UTF-8

This software used to be written in UTF-8, but apparently that's still too
complex for "modern" compilers, editors, build utilities, and filesystems.

### Semaphores

[Apple really got my goat][6].

### ptrace

["A lot of people seem to move to Mac OS X from a Linux or BSD background and
therefore expect the ptrace() syscall to be useful."][7]

[1]: http://en.wikipedia.org/wiki/Kimi_R%C3%A4ikk%C3%B6nen "Kimi Räikkönen"
[2]: http://spinroot.com/spin/whatispin.html "Spin"
[3]: https://code.google.com/p/thread-sanitizer/ "ThreadSanitizer"
[4]: http://en.wikipedia.org/wiki/Finnish_language "Finnish Language"
[5]: tests/test.c "test.c"
[6]: http://stackoverflow.com/questions/27736618/why-are-sem-init-sem-getvalue-sem-destroy-deprecated-on-mac-os-x-and-w/27847103#27847103 "sem_init on OS X"
[7]: http://uninformed.org/index.cgi?v=4&a=3&p=14 "Replacing ptrace()"

