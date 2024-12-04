Based on info from [subexp-daq](https://fy.chalmers.se/subatom/subexp-daq/) and
[minidaq_v2718_mdpp16.txt](https://fy.chalmers.se/subatom/subexp-daq/minidaq_v2718_mdpp16.txt)

# General

# DAQ tests / demos

## daq0

nurdlib SiCy daq. Uses the MVLC command pipe and its smaller buffer for the DAQ.

  ```bash
  git submodule update --init --recursive
  ./build-daq0.sh && cd scripts && ./free.bash
  ```
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


The high level mvlc::ReadoutWorker is used to perform initialization and readout
and parsing. For performance reasons it internally spawns two threads, one for
the readout and one listfile_writer thread. This is so that listfile compression
and writing can happen in parallel to reading from the network/usb.

### The problem with daq1-drasi-lwroc

The readout and the mvlc::readout_parser run in their own threads spawned via
std::thread or std::async. The main thread idles, waiting for the DAQ to finish
or `_lwroc_main_thread->_terminate` to be set.

The main readout data callback `readout_parser_callback_eventdata` calls into
lwroc. If any of these calls lead to drasi emitting a log message the DAQ kills
itself as the internal thread is not prepared for logging
(`"Message not logged - thread has no error buffer yet...\n"`)

## daq2-drasi-lwroc-no-mvlc-threads


Same das daq1 but no more extra mvlc threads. Still calls `init_readout` with
the mvlc crateconfig data but `readout_loop` is now doing readout and parsing
directly. A new free `mvlc::readout()` function was added and
`mvlc::parse_readout_buffer` is called directly which means the callbacks are
invoked in the thread of `readout_loop` itself.

## triggered reaoduts with triva/trlloII?

## Collection of readout_dt steps
# caen_v1n90
```
read status and CRATE_READOUT_FAIL_GENERAL if status is not ok
read event_fifo_status -> done or continue
read was_full -> log error
read event_counter -> set module.event_counter.value
```

# caen_v7nn
```
read event_counter ->set module_event_counter.value
```

# mesytec_mxdc
```
read event_counter -> set a_mxdc32->module.event_counter.value
read buffer_data_length -> a_mxdc32->buffer_data_length
```
