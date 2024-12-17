Based on info from [subexp-daq](https://fy.chalmers.se/subatom/subexp-daq/) and
[minidaq_v2718_mdpp16.txt](https://fy.chalmers.se/subatom/subexp-daq/minidaq_v2718_mdpp16.txt)

# General

To build everything:
```bash
git submodule update --init --recursive
./build-daqs.sh
```

Running the SiCy nurdlib daq:
```bash
cd scripts && ./free.bash
```

# DAQ tests / demos

## daq0

nurdlib SiCy daq. Uses the MVLC command pipe and its smaller buffer for the DAQ.

## daq1-drasi-lwroc / daq2-drasi-lwroc-no-mvlc-threads

* [daq1-drasi-lwroc](src/daq1-drasi-lwroc/daq1.cc)
* [daq2-drasi-lwroc-no-mvlc-threads](src/daq2-drasi-lwroc-no-mvlc-threads/daq2.cc)

Turns configuration around: a mvlc .yaml crateconfig file is used to initialize
the daq and configure the readout parser. Crateconfig .yaml files can be exported
from mvme setups.

Parsed data is used to fill events (`lwroc_new_event`) and subevents
(`lwroc_new_subevent`). The incoming eventIndex (== trigger stack number) and
module index (== index of the module in the readout stack) are combined to form
a uint16_t linear_index which is stored in `sevInfo.subtype`.

The high level mvlc::ReadoutWorker is used to perform initialization, readout
and parsing. For performance reasons it internally spawns two threads, one for
the readout and one listfile_writer thread. This is so that listfile compression
and writing can happen in parallel to reading from the network/usb.

### Issue with daq1-drasi-lwroc

The readout and the mvlc::readout_parser run in their own threads spawned via
std::thread or std::async. The main thread idles, waiting for the DAQ to finish
or `_lwroc_main_thread->_terminate` to be set.

The main readout data callback `readout_parser_callback_eventdata` calls into
lwroc. If any of these calls lead to drasi emitting a log message the DAQ kills
itself as the internal thread is not prepared for logging
(`"Message not logged - thread has no error buffer yet...\n"`)

### daq2-drasi-lwroc-no-mvlc-threads

Same das daq1 but no more extra mvlc threads. Still calls `init_readout` with
the mvlc crateconfig data but `readout_loop` is now doing readout and parsing
directly. A new free `mvlc::readout()` function was added and
`mvlc::parse_readout_buffer` is called directly which means the callbacks are
invoked in the thread of `readout_loop` itself.

### mvlcc_mini_daq.c

Similar to the `mvlc-mini-daq` tool but implemented using only the new mvlcc C
wrappers. Usage is `mvlcc_mini_daq <crateconfig> <duration_s>`.

Needs a bit more work so that it logs readout parser counters and warns on parse
error. The code is [here](external/mvlcc/example/mvlcc_mini_daq.c).

Run `make -C example` in the mvlcc directory to build this.

Start it with `./example/mvlcc_mini_daq
~/src/chalmers-mvlc-integration/chalmers-mvme-dev-workspace/01-mdpp16_scp.yaml
10` or any of the other .yaml configs.

### daq3-nurdlib

Unfinished, was trying to get crate/module information from the nurdlib config system.

# Documentation

* mesytec-mvlc:

  * [mesytec-mvlc Readme](external/mesytec-mvlc/README.md)
  * [mesytec-mvlc Command format](external/mesytec-mvlc/doc/command_format.md)
  * [mesytec-mvlc Data format](external/mesytec-mvlc/doc/data_format.md)

* mvme (contains some info about the Trigger/IO system):
  * [mvme manual](https://mesytec.com/downloads/mvme/mvme.pdf)

* mvlcc: TODO, the header will get some docstrings *soon*.

# GSI TRIVA template

mvme configuration and exported .yaml/.json mvlc crateconfigs are located [here](chalmers-mvme-dev-workspace/).

This is a setup containing the basic structure for a TRIVA readout running in
MVLC DAQ mode. Readout commands for several modules including mesytec, caen and
gsi modules are also included. Nick Kurz and Joern Adamczewski-Musch use this as
a template for the creation of concrete DAQ configurations. The mvme .vme config
is exported to mvlc .yaml. mvlc-mini-daq is used to initialize the readout.
Afterwards their own tool is started, takes over the USB and does the readout
and data processing.

## The trigger dispatch

 TRIVA raises VME IRQ4 which is processed in
 `event_0_catch_triva_trigger_type`. The mvlc stack accumulator is used to
 extract a trigger number from TRIVA.  `mvlc_signal_accu` then dispatches to the
 correct readout stack based on the extracted trigger number (``Readout Loop ->
 triva7_master``). The mapping of TRIVA trigger numbers to MVLC IRQ values is
 configured in `triva7_master -> Module Init`.

This could potentially also work for TRLOII as the trigger numbers and general
mechanism look very similar.

# nurdlib

Ideas for mvlc <-> nurdlib integrations.

## Combined per module readout_dt and readout.

Combine readout_dt and readout for each module. Assumes that the `readout`
part reads out all events, not just a single one in case of multi event
readouts. MVLC would need to be triggered via VME IRQ or NIM input.

- init for mesytec modules. caen might offer a similar interface.
```
0x601C 0    # 0 -> the following register specifies the number of events
0x601E 12   # IRQ-FIFO threshold, events (raise IRQ when 12 events are in the buffer)
0x6036 0xb  # 0xb -> multievent, transmits number of events specified
0x601A 10   # multi event mode == 0xb -> Berr is emitted when the specified number
            # of events has been transmitted.
            # read out up to 10 events or until BERR.
```

- readout script in mvme

```
# readout_dt
read a32 d16 0x6092 # evctr_lo
read a32 d16 0x6094 # evctr_hi
read a32 d16 0x6030 # buffer_data_length
# readout
mbltfifo a32 0x0000 65535
```

- readout_parser output:

With the above readout, parsed data will look like this
(``mvlcc_module_data_t`` from
[mvlcc_wrap.h](external/mvlcc/include/mvlcc_wrap.h)).

```
module_data.prefix = { evctr_lo, evctr_hi, buffer_data_length }
module_data.dynamic = { block read data until berr. multiple events if setup for multievent }
```

- Split `ModuleProps.readout_dt()` into `handle_dt()` and `check_dt()`:

handle_dt() gets the *prefix* part and assigns to the Module instance:
```
a_mxdc32->module.event_counter_value = data[0] | (data[1] << 16);
a_mxdc32->buffer_data_length = data[3];
```

check_dt() is called after handle_dt():
```
if (a_mxdc32->module.event_counter_value == 42) // Whatever checks need to be done.
  return CRATE_READOUT_FAIL_GENERAL;
```

Have to add custom config and readouts for all modules. Could load these from
flat text, json or yaml files using the new mvlcc_command_list_from_*()
functions.

## Interactive readout but using the larger MVLC readout buffers

Create one *readout_dt* and one *readout* stack per trigger (or tag set). MVLC
Trigger/IO: use `TriggerResource` units to activate the readout stacks on
SoftTrigger signals.

In the code you can now manually create triggers by writing to the soft_trigger
output activation register. So the *readout_dt* stack could be executed once,
followed by activating the *readout* stack multiple times in a loop to readout
all pending events.

An example mvme setup is located
[here](chalmers-mvme-dev-workspace/soft_triggers.vme). Start the DAQ, then run
the scripts unter `Manual` to trigger one of the 4 defined readout events. To
see resulting data open *Debug & Stats* in the mvme analysis windows and click
*Debug next buffer (ignore timeticks)*.

# Copyright and mvlcc future

I'd like to use the current mvlcc base to create a c-layer integrated into the
mesytec-mvlc lib. I'll use different prefixes for the function names so nothing
should collide. mesytec-mvlc is under the permissive *Boost Software License*
which I'd also like to use for the mvlcc code.

Code added/modified in other projects is under the license of the parent
project.
