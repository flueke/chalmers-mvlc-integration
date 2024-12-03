#include <cassert>
#include <cstdint>
#include <mesytec-mvlc/mesytec-mvlc.h>
#include <spdlog/spdlog-inl.h>
#include <spdlog/sinks/stdout_color_sinks.h>

extern "C"
{

#include <lwroc_mon_block.h>
#include <lwroc_net_conn_monitor.h>
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
  {
    assert(write);
  }

  size_t write(const uint8_t *data, size_t size) override
  {
    return write_ ? write_(userContext_, data, size) : 0;
  }

  listfile_write_function write_;
  void *userContext_;
};

extern struct lwroc_readout_functions _lwroc_readout_functions;
extern lwroc_mon_block *_lwroc_mon_main_handle;
extern lwroc_net_conn_monitor *_lwroc_mon_main_system_handle;
std::shared_ptr<spdlog::logger> logger;

struct DaqContext
{
  lmd_stream_handle *lmd_stream = nullptr;
  const char *crateConfigFilename = nullptr;
  mesytec::mvlc::CrateConfig crateConfig;
  mesytec::mvlc::readout_parser::ReadoutParserCallbacks parserCallbacks;
  mesytec::mvlc::readout_parser::ReadoutParserState readoutParser;
  mesytec::mvlc::readout_parser::ReadoutParserCounters readoutParserCounters;
  std::unique_ptr<mesytec::mvlc::ReadoutWorker> readoutWorker;
  mesytec::mvlc::MVLC mvlc;
  size_t bufferNumber = 0;
  uint32_t outputEventNumber = 0;
};

DaqContext g_ctx;

void lwroc_readout_pre_parse_functions(void)
{
  if (!logger)
  {
    logger = spdlog::stdout_color_mt("daq1");
    logger->set_level(spdlog::level::info);
  }
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


size_t listfile_write_callback(void *userContext, const uint8_t *data, size_t size)
{
  auto ctx = static_cast<DaqContext *>(userContext);

  assert(size % 4 == 0); // the readout buffer should only contain 32-bit words

  const uint32_t *buf = reinterpret_cast<const uint32_t *>(data);
  const size_t bufWords = size / sizeof(uint32_t);

  auto parseResult = mesytec::mvlc::readout_parser::parse_readout_buffer(
    ctx->mvlc.connectionType(),
    ctx->readoutParser,
    ctx->parserCallbacks,
    ctx->readoutParserCounters,
    ctx->bufferNumber++,
    buf, bufWords);

    if (parseResult != mesytec::mvlc::readout_parser::ParseResult::Ok)
    {
      logger->error("listfile_write_callback: Error parsing readout buffer: {}",
        mesytec::mvlc::readout_parser::get_parse_result_name(parseResult));
      return 0;
    }
    else
    {
      logger->debug("listfile_write_callback: processed buffer #{} of size {} B",
        ctx->bufferNumber-1, size);
    }

  return 0;
}

void readout_parser_callback_eventdata(
  void *userContext, int crateIndex, int eventIndex,
  const mesytec::mvlc::readout_parser::ModuleData *moduleDataList, unsigned moduleCount)
{
  logger->trace("readout_parser_callback_eventdata(): crateIndex={}, eventIndex={}, moduleCount={}",
    crateIndex, eventIndex, moduleCount);

  auto context = static_cast<DaqContext *>(userContext);

  if (logger->should_log(spdlog::level::trace))
  {
      for (uint32_t moduleIndex = 0; moduleIndex < moduleCount; ++moduleIndex)
      {
          auto &moduleData = moduleDataList[moduleIndex];

          if (moduleData.data.size)
              mesytec::mvlc::util::log_buffer(
                  std::cout, std::basic_string_view<uint32_t>(moduleData.data.data, moduleData.data.size),
                  fmt::format("module data: crateId={} eventIndex={}, moduleIndex={}", crateIndex, eventIndex, moduleIndex));
      }
  }

  size_t totalWords = 0u;

  for (uint32_t moduleIndex = 0; moduleIndex < moduleCount; ++moduleIndex)
  {
    totalWords += moduleDataList[moduleIndex].data.size;
  }

  if (totalWords == 0)
    return;

  const size_t totalBytes = totalWords * sizeof(uint32_t);
  struct lwroc_lmd_subevent_info sevInfo;
  lmd_event_10_1_host *event;
  lmd_subevent_10_1_host *sev;
  uint8_t *buf;
  uint8_t *end;
  size_t event_size;
  event_size = sizeof (lmd_subevent_10_1_host) * moduleCount + totalBytes;
  lwroc_reserve_event_buffer(context->lmd_stream, context->outputEventNumber++,
      event_size, 0, 0);
  lwroc_new_event(context->lmd_stream, &event, 1);

  for (uint32_t moduleIndex = 0; moduleIndex < moduleCount; ++moduleIndex)
  {
    auto moduleData = moduleDataList[moduleIndex];
    auto moduleBytes = moduleData.data.size * sizeof(uint32_t);

    if (moduleBytes == 0)
      continue;

    sevInfo.type = 10;
    sevInfo.subtype = moduleIndex;
    sevInfo.procid = 13;
    sevInfo.control = 1;
    sevInfo.subcrate = 0;
    buf = (uint8_t *)lwroc_new_subevent(context->lmd_stream, LWROC_LMD_SEV_NORMAL,
        &sev, &sevInfo);

    memcpy(buf, moduleData.data.data, moduleBytes);
    end = buf + moduleBytes;
    lwroc_finalise_subevent(context->lmd_stream, LWROC_LMD_SEV_NORMAL,
        end);
  }

  lwroc_finalise_event_buffer(context->lmd_stream);

  LWROC_MON_CHECK_COPY_BLOCK(_lwroc_mon_main_handle,
      &_lwroc_mon_main, 0);
  LWROC_MON_CHECK_COPY_CONN_MON_BLOCK(
      _lwroc_mon_main_system_handle, 0);

  logger->trace("readout_parser_callback_eventdata: finalized event buffer #{} of size {} B ({} words)",
    context->outputEventNumber-1, totalBytes, totalBytes/sizeof(uint32_t));
}

void readout_parser_callback_systemevent(
  void *userContext, int crateIndex, const uint32_t *header, uint32_t size)
{
  logger->trace("readout_parser_callback_systemevent(): crateIndex={}, header={:08x}, size={}",
    crateIndex, *header, size);
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
    else
    {
      logger->info("init_readout: cmd={}, ec={}", to_string(cmdResult.cmd), cmdResult.ec.message());
    }
  }

  if (errorSeen)
    return;

  static const uint8_t crateId = 0;

  auto listfileWriteHandle = std::make_shared<CWriteHandle>(
    listfile_write_callback, static_cast<void *>(&g_ctx));

  g_ctx.readoutWorker = std::make_unique<mesytec::mvlc::ReadoutWorker>(
    g_ctx.mvlc, listfileWriteHandle, crateId);

  g_ctx.readoutWorker->setMcstDaqStartCommands(g_ctx.crateConfig.mcstDaqStart);
  g_ctx.readoutWorker->setMcstDaqStopCommands(g_ctx.crateConfig.mcstDaqStop);

  g_ctx.readoutParser = mesytec::mvlc::readout_parser::make_readout_parser(
    g_ctx.crateConfig.stacks, static_cast<void *>(&g_ctx));

  g_ctx.parserCallbacks.eventData = readout_parser_callback_eventdata;
  g_ctx.parserCallbacks.systemEvent = readout_parser_callback_systemevent;

  g_ctx.lmd_stream = lwroc_get_lmd_stream("READOUT_PIPE");
  lwroc_init_timestamp_track();

  logger->info("leaving init()");
}

void read_event(uint64_t cycle, uint16_t trig)
{
  assert(!"read_event() not implemented!");
}

void readout_loop(int *start_no_stop)
{
  logger->info("entering readout_loop()");
  unsigned int cycle = 0;

  if (auto ec = g_ctx.readoutWorker->start().get())
  {
    logger->error("Error starting readout: {}", ec.message());
    return;
  }

  for (cycle = 1; !_lwroc_main_thread->_terminate; cycle++)
  {
    logger->trace("readout_loop(): cycle {}", cycle);

    if (g_ctx.readoutWorker->state() == mesytec::mvlc::ReadoutWorker::State::Idle)
    {
      logger->info("readout_worker state became Idle, leaving readout_loop()");
      break;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  if (auto ec = g_ctx.readoutWorker->stop())
  {
    logger->warn("Error stopping readout: {}", ec.message());
  }

  while (g_ctx.readoutWorker->state() != mesytec::mvlc::ReadoutWorker::State::Idle)
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  logger->info("leaving readout_loop()");
}

void cmdline_usage(void)
{
  printf ("  --mvlc-crateconfig=<crateconfig.yaml>            mvlc createconfig file.\n");
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
