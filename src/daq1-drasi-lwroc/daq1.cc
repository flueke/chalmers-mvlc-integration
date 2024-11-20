#include <cstdint>
#include <mesytec-mvlc/mesytec-mvlc.h>

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

// these are assigned to _lwroc_readout_functions
void init(void);
void read_event(uint64_t cycle, uint16_t trig);
void readout_loop(int *start_no_stop);
void cmdline_usage(void);
int parse_cmdline_arg(const char *request);

typedef size_t (*listfile_write_function)(void *userContext, const uint8_t *, size_t);

struct CWriteHandle: public mesytec::mvlc::listfile::WriteHandle
{
  explicit CWriteHandle(listfile_write_function write, void *userContext = nullptr)
    : write_(write)
    , userContext_(userContext)
  { }

  size_t write(const uint8_t *data, size_t size) override
  {
    return write_ ? write_(userContext_, data, size) : 0;
  }

  listfile_write_function write_;
  void *userContext_;
};

extern struct lwroc_readout_functions _lwroc_readout_functions;
std::shared_ptr<spdlog::logger> logger;

struct DaqContext
{
  lmd_stream_handle *lmd_stream = nullptr;
  const char *crateConfigFilename = nullptr;
  mesytec::mvlc::CrateConfig crateConfig;
  mesytec::mvlc::readout_parser::ReadoutParserCallbacks parserCallbacks;
  mesytec::mvlc::readout_parser::ReadoutParserState readoutParser;
  std::unique_ptr<mesytec::mvlc::ReadoutWorker> readoutWorker;
  mesytec::mvlc::MVLC mvlc;
};

DaqContext g_ctx;

void lwroc_readout_pre_parse_functions(void)
{
  logger = mesytec::mvlc::get_logger("daq1");
  logger->set_level(spdlog::level::debug);
  logger->info("entered lwroc_readout_pre_parse_functions()");

  _lwroc_readout_functions.init = init;
  _lwroc_readout_functions.read_event = read_event;
  _lwroc_readout_functions.untriggered_loop = readout_loop;
  _lwroc_readout_functions.cmdline_fcns.usage = cmdline_usage;
  _lwroc_readout_functions.cmdline_fcns.parse_arg = parse_cmdline_arg;
  _lwroc_readout_functions.fmt = &_lwroc_lmd_format_functions;

  logger->info("leaving lwroc_readout_pre_parse_functions()");
}

void lwroc_readout_setup_functions(void)
{
  logger->info("entering lwroc_readout_setup_functions()");
  logger->info("leaving lwroc_readout_setup_functions()");
}

void callback_eventdata(void *userContext, int crateIndex, int eventIndex,
                        const mesytec::mvlc::readout_parser::ModuleData *moduleDataList, unsigned moduleCount)
{
  assert(false);
}

void callback_systemevent(void *userContext, int crateIndex, const uint32_t *header, uint32_t size)
{
  assert(false);
}

void init(void)
{
  logger->info("entering init()");

  if (!g_ctx.crateConfigFilename)
  {
    logger->error("No crate config file specified.");
    return;
  }

  try
  {
    g_ctx.crateConfig = mesytec::mvlc::crate_config_from_yaml_file(g_ctx.crateConfigFilename);
  }
  catch(const std::exception& e)
  {
    logger->error("Failed to read MVLC crate config from '{}': {}", g_ctx.crateConfigFilename, e.what());
    return;
  }

  g_ctx.mvlc = mesytec::mvlc::make_mvlc(g_ctx.crateConfig);
  g_ctx.mvlc.setDisableTriggersOnConnect(true);

  if (auto ec = g_ctx.mvlc.connect())
  {
    logger->error("Error connecting to MVLC: {}", ec.message());
    return;
  }

  auto initResults = mesytec::mvlc::init_readout(g_ctx.mvlc, g_ctx.crateConfig);

  if (initResults.ec)
  {
    logger->error("Error during DAQ init sequence: {}", initResults.ec.message());
    return;
  }

  bool errorSeen = false;

  for (const auto &cmdResult: initResults.init)
  {
    if (cmdResult.ec)
    {
      logger->error("Error during DAQ init sequence: cmd={}, ec={}",
          to_string(cmdResult.cmd), cmdResult.ec.message());
      errorSeen = true;
    }
  }

  if (errorSeen)
    return;

  g_ctx.lmd_stream = lwroc_get_lmd_stream("READOUT_PIPE");
  lwroc_init_timestamp_track();

  logger->info("leaving init()");
}

void read_event(uint64_t cycle, uint16_t trig)
{
  assert(false);
}

void readout_loop(int *start_no_stop)
{
  logger->info("entering readout_loop()");
  unsigned int cycle;

  for (cycle = 1; !_lwroc_main_thread->_terminate; cycle++)
  {
  }
  logger->info("leaving readout_loop()");
}

void cmdline_usage(void)
{
  printf ("  --mvlc-crateconfig=<crateconfig.yaml>                 mvlc createconfig file.\n");
  printf ("  --log-level=error|warn|info|debug|trace          set main log level.\n");
  printf ("  --mvlc-log-level=error|warn|info|debug|trace     set mesytec-mvlc library log level.\n");
  printf ("\n");
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
  else if (LWROC_MATCH_C_PREFIX("--mvlc-crateconfig=", post))
  {
    g_ctx.crateConfigFilename = post;
  }
  else
    return 0;

  return 1;
}

}
