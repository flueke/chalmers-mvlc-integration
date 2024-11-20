#include <cstdint>
#include <mesytec-mvlc/util/logging.h>

extern "C"
{

#include "lwroc_parse_util.h"
#include "lwroc_readout.h"

#include "lmd/lwroc_lmd_event.h"
#include "lmd/lwroc_lmd_ev_sev.h"
#include "lmd/lwroc_lmd_util.h"

// drasi links against these
void lwroc_readout_pre_parse_functions(void);
void lwroc_readout_setup_functions(void);

void init(void);
void read_event(uint64_t cycle, uint16_t trig);
void readout_loop(int *start_no_stop);

void cmdline_usage(void);
int parse_cmdline_arg(const char *request);

extern struct lwroc_readout_functions _lwroc_readout_functions;
lmd_stream_handle *lmd_stream = NULL;
std::shared_ptr<spdlog::logger> logger;

void lwroc_readout_pre_parse_functions(void)
{
  logger = mesytec::mvlc::get_logger("daq1");
  logger->set_level(spdlog::level::debug);
  logger->debug(__func__);

  _lwroc_readout_functions.init = init;
  _lwroc_readout_functions.read_event = read_event;
  _lwroc_readout_functions.untriggered_loop = readout_loop;
  _lwroc_readout_functions.cmdline_fcns.usage = cmdline_usage;
  _lwroc_readout_functions.cmdline_fcns.parse_arg = parse_cmdline_arg;
  _lwroc_readout_functions.fmt = &_lwroc_lmd_format_functions;
}

void lwroc_readout_setup_functions(void)
{
    logger->debug(__func__);
}

void init(void)
{
    lmd_stream = lwroc_get_lmd_stream("READOUT_PIPE");

    lwroc_init_timestamp_track();
}

void read_event(uint64_t cycle, uint16_t trig)
{
}

void readout_loop(int *start_no_stop)
{
}

void cmdline_usage(void)
{
}

int parse_cmdline_arg(const char *request)
{
  const char *post;

  if (LWROC_MATCH_C_PREFIX("--log-level=", post))
  {
    logger->set_level(spdlog::level::from_str(post));
  }
  else if (LWROC_MATCH_C_PREFIX("--mvlc-log-level=", post))
  {
    mesytec::mvlc::set_global_log_level(spdlog::level::from_str(post));
  }
  else
    return 0;

  return 1;
}

}
