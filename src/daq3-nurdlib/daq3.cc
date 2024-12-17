// main() is in drasi/lwroc/lwroc_main.c
// Structure and code taken from nurdlib/fuser/fuser.c

#include <cassert>

extern "C" {

#include <nurdlib.h>
#include <nurdlib/base.h>
#include <nurdlib/config.h>
#include <nurdlib/crate.h>

#	include <lwroc_message_inject.h>
#	include <lwroc_mon_block.h>
#	include <lwroc_net_conn_monitor.h>
#	include <lwroc_readout.h>
#	include <lwroc_thread_util.h>
#	include <lwroc_track_timestamp.h>
#	include <lmd/lwroc_lmd_ev_sev.h>
#	include <mvlcc_wrap.h>
#	include <f_user_daq.h>
#	include <util/thread.h>

#include <module/module.h>

enum MapType {
	MAP_TYPE_SICY,
	MAP_TYPE_BLT,
	MAP_TYPE_USER
};
struct Map {
	enum	MapType type;
	enum	Keyword mode;
	uint32_t	address;
	size_t	bytes;
	int	do_mblt_swap;
	void	*private_;
};

} // extern "C"

#include <mesytec-mvlc/mesytec-mvlc.h>

#define CONFIG_NAME_PRIMARY "main.cfg"

extern struct lwroc_readout_functions _lwroc_readout_functions;

extern "C" int f_user_get_virt_ptr(long *, long []);
extern "C" int f_user_init(unsigned char, long *, long *, long *);
extern "C" int f_user_readout(unsigned char, unsigned char, long *,
    long *, long *, void *, long *, long *);
static void untriggered_loop(int *start_no_stop);
static void	log_callback(char const *, int, unsigned, char const *);

/* Callback into drasi threading infrastructure. */
static struct Mutex g_thread_no_mutex;
static int g_thread_no;

extern struct lwroc_readout_functions _lwroc_readout_functions;
extern lwroc_mon_block *_lwroc_mon_main_handle;
extern lwroc_net_conn_monitor *_lwroc_mon_main_system_handle;
static lmd_stream_handle *g_lmd_stream;
extern mvlcc_t mvlc; // resolves to the one in map_mvlc.c

#if 0
VECTOR_HEAD(ModuleRefVector, struct Module *);
VECTOR_HEAD(CounterRefVector, struct CrateCounter *);
struct CrateTag {
	char	name[32];
	struct	ModuleRefVector module_ref_vec;
	struct	CounterRefVector counter_ref_vec;
	unsigned	module_num;
	unsigned	event_max;
	int	gsi_pex_is_needed;
	int	do_pedestals;
	TAILQ_ENTRY(CrateTag)	next;
};
#endif

static void
thread_callback(void *a_data)
{
	int no;

	(void)a_data;
	thread_mutex_lock(&g_thread_no_mutex);
	/* Make sure to start with 1. */
	no = ++g_thread_no;
	thread_mutex_unlock(&g_thread_no_mutex);
	/* TODO: Can we give a good purpose? */
	lwroc_thread_user_init(no, NULL);
}

/* Nurdlib crate context. */
static struct Crate *g_crate;

/*
 * This example couples each TRIVA trigger to a readout tag, such that eg we
 * can read one set of modules on trigger 1 and another set of modules on
 * trigger 2. Note that the modules in different tags can overlap, nurdlib
 * will track each module's counter!
 */
static struct CrateTag *g_tag[16];

/* Size of event memory for each trigger as configured by MBS/drasi. */
static uint32_t g_ev_bytes[16];

/*
 * Data for the deadtime releasing function. Could just be global primitives,
 * but let's use some fancy tricks.
 */
struct DT {
	unsigned	char trig_typ;
	long	*read_stat;
};
static struct DT g_dt;

int
f_user_get_virt_ptr(long *pl_loc_hwacc, long *pl_rem_cam)
{
	(void)pl_loc_hwacc;
	(void)pl_rem_cam;
	/*
	 * untriggered_loop will only be called if no trigger module has been
	 * set for drasi (--triva/trimi).
	 */
	_lwroc_readout_functions.untriggered_loop = untriggered_loop;
	return 0;
}

int
f_user_init(unsigned char bh_crate_nr, long *pl_loc_hwacc, long *pl_rem_cam,
    long *pl_stat)
{
	static int is_setup = 0;
	char cfg_path[256] = CONFIG_NAME_PRIMARY;
	unsigned i;

	(void)bh_crate_nr;
	(void)pl_loc_hwacc;
	(void)pl_rem_cam;
	(void)pl_stat;

	if (is_setup) {
		return 0;
	}
	is_setup = 1;

	/* Setup DAQ backend logging first of all. */
	log_callback_set(log_callback);

	/* Figure out the max event size. */
	{
		g_ev_bytes[0] = fud_get_max_event_length();
		for (i = 1; i < LENGTH(g_ev_bytes); ++i) {
			g_ev_bytes[i] = g_ev_bytes[0];
		}
		lwroc_init_timestamp_track();
	}

	/*
	 * An optional file 'nurdlib_def_path.txt' to set the default config
	 * path.
	 */
	{
		FILE *file;

		file = fopen("nurdlib_def_path.txt", "rb");
		if (file) {
			char path[256];

			if (fgets(path, sizeof path, file)) {
				char *p;

				p = strchr(path, '\n');
				if (NULL != p) {
					*p = '\0';
				}
				config_default_path_set(path);
			}
			fclose(file);
		}
	}

	if (!thread_mutex_init(&g_thread_no_mutex)) {
		log_die(LOGL, "thread_mutex_init failed.");
	}
	g_thread_no = 0;
	thread_start_callback_set(thread_callback, NULL);

	/* Provide DAQ backend logging (again) and load config file. */
	g_crate = nurdlib_setup(log_callback, cfg_path);

	/*
	 * Get the "Default" tag and tags "1", "2" etc, one for each TRIVA
	 * trigger number.
	 */
	g_tag[0] = crate_get_tag_by_name(g_crate, "Default");
	for (i = 1; i < LENGTH(g_tag); ++i) {
		char name[10];

		snprintf(name, sizeof name, "%u", i);
		g_tag[i] = crate_get_tag_by_name(g_crate, name);
	}
	if (NULL == g_tag[1]) {
		g_tag[1] = g_tag[0];
	}

	return 0;
}

static mesytec::mvlc::StackCommandBuilder make_module_readout_commands(const std::string &moduleTypename)
{
	mesytec::mvlc::StackCommandBuilder result;

	auto filename = fmt::format("readout-{}.mvlccmds", moduleTypename);

	LOGF(info)(LOGL, "Reading readout commands from file '%s'", filename.c_str());

	std::ifstream file(filename);

	for (std::string line; std::getline(file, line);)
	{
		auto cmd = mesytec::mvlc::stack_command_from_string(line);
		result.addCommand(cmd);
	}

	return result;
}

uint32_t module_get_vme_address(struct Module *mod)
{
	auto theMap = mod->props->get_map(mod);
	return theMap->address;
}

static mesytec::mvlc::StackCommandBuilder make_crate_readout_commands(Crate *crate)
{
	const size_t moduleCount = crate_module_get_num(crate);

	mesytec::mvlc::StackCommandBuilder result("crate_readout");

	for (size_t mi=0; mi<moduleCount; ++mi)
	{
		struct Module *mod = crate_module_get_by_index(crate, mi);

		if (mod)
		{
			auto typeString = mesytec::mvlc::str_tolower(keyword_get_string(mod->type));
			uint32_t vmeAddress = module_get_vme_address(mod);

			LOGF(info)(LOGL, "Module: idx=%lu, typename=%s, vmeAddress=0x%08x", mi, typeString.c_str(), vmeAddress);

			auto modReadoutCommands = make_module_readout_commands(typeString);

			result.beginGroup(fmt::format("{}_0x{:08x}", typeString, vmeAddress));
			for (auto &cmd: modReadoutCommands.getCommands())
			{
				cmd.address += vmeAddress; // add the modules base address to the readout commands
				result.addCommand(cmd);
			}
		}
	}

	return result;
}

static const unsigned ReadoutStackId = 1; // stack#0 is reserved for direct command execution (library/mvme convention only).

static void mvlc_readout_setup()
{
	CrateTag *default_tag = g_tag[0];

	for (unsigned i=0; i<LENGTH(g_tag); ++i)
	{
		if (g_tag[i] && g_tag[i] != default_tag)
		{
			log_die(LOGL, "mvlc_readout_setup(): currently only one tag is supported.");
		}
	}

	mesytec::mvlc::StackCommandBuilder crateReadoutCommands = make_crate_readout_commands(g_crate);
	LOGF(info)(LOGL, "Crate readout commands:\n%s", mesytec::mvlc::to_yaml(crateReadoutCommands).c_str());

	auto mvlcObj = reinterpret_cast<mesytec::mvlc::MVLC *>(mvlcc_get_mvlc_object(mvlc));

	const unsigned triggerValue = 0; // no trigger, we trigger via request/response in mvlc_readout_loop().
	auto ec = mesytec::mvlc::setup_readout_stack(*mvlcObj, crateReadoutCommands, ReadoutStackId, triggerValue);

	if (ec)
	{
		log_die(LOGL, "mvlc_readout_setup(): setup_readout_stack failed: %s", ec.message().c_str());
	}
}

static void mvlc_readout_loop()
{
	uint64_t cycle = 0;

	namespace registers = mesytec::mvlc::registers;
	namespace stacks = mesytec::mvlc::stacks;

	mesytec::mvlc::SuperCommandBuilder execStackCommands;
	execStackCommands.addReferenceWord(cycle);
	execStackCommands.addWriteLocal(registers::stack_exec_status0, 0);
	execStackCommands.addWriteLocal(registers::stack_exec_status1, 0);
	execStackCommands.addWriteLocal(stacks::get_trigger_register(ReadoutStackId), 1u << stacks::ImmediateShift);
    auto execStackBuffer = mesytec::mvlc::make_command_buffer(execStackCommands);
	std::vector<std::uint32_t> execStackResponse;

	auto mvlcObj = reinterpret_cast<mesytec::mvlc::MVLC *>(mvlcc_get_mvlc_object(mvlc));
	assert(mvlcObj);

	for (cycle = 1; !_lwroc_main_thread->_terminate; cycle++)
	{
		struct lwroc_lmd_subevent_info info;
		lmd_event_10_1_host *event;
		lmd_subevent_10_1_host *sev;
		void *buf;
		void *end;
		size_t event_size;
		long bytes_read = 0;

		event_size = sizeof (lmd_subevent_10_1_host) + g_ev_bytes[0];
		lwroc_reserve_event_buffer(g_lmd_stream, (uint32_t)cycle,
		    event_size, 0, 0);
		lwroc_new_event(g_lmd_stream, &event, 1);
		info.type = 10;
		info.subtype = 1;
		info.procid = 13;
		info.control = 1;
		info.subcrate = 0;
		buf = lwroc_new_subevent(g_lmd_stream, LWROC_LMD_SEV_NORMAL,
		    &sev, &info);

		while (!_lwroc_main_thread->_terminate) {
			#if 0
			long read_status;

			bytes_read = 0;
			read_status = 0;
			f_user_readout(1, 0, NULL, NULL, buf, (void *)&sev,
			    &bytes_read, &read_status);
			if (sizeof(uint32_t) < (size_t)bytes_read) {
				/* More than just custom header written. */
				break;
			}
			/*
			 * Use the monitor timer to force generation of
			 * an event even if empty.  Currently 10 Hz.
			 */
			if (LWROC_MON_UPDATE_PENDING(_lwroc_mon_main_handle))
			{
				break;
			}
			sched_yield();
			#else
			auto ec = mvlcObj->superTransaction(execStackCommands, execStackResponse);
			assert(!ec);
			LOGF(info)(LOGL, "mvlc_readout_loop(): superTransaction response: %s", ec.message().c_str());
			#endif
		}

		end = (uint8_t *)buf + bytes_read;
		lwroc_finalise_subevent(g_lmd_stream, LWROC_LMD_SEV_NORMAL,
		    end);
		lwroc_finalise_event_buffer(g_lmd_stream);

		LWROC_MON_CHECK_COPY_BLOCK(_lwroc_mon_main_handle,
		    &_lwroc_mon_main, 0);
		LWROC_MON_CHECK_COPY_CONN_MON_BLOCK(
		    _lwroc_mon_main_system_handle, 0);
	}
}

void
untriggered_loop(int *start_no_stop)
{
	if (start_no_stop)
	{
		*start_no_stop = 0; // from nurdlib/fuser/fuser.c
	}

	f_user_init(0, NULL, NULL, NULL);

	if (!mvlc)
	{
		log_die(LOGL, "untriggered_loop(): mvlc not initialized.");
	}

	g_lmd_stream = lwroc_get_lmd_stream("READOUT_PIPE");
	crate_free_running_set(g_crate, 1);

	mvlc_readout_setup();
	mvlc_readout_loop();
}

int
f_user_readout(unsigned char bh_trig_typ, unsigned char bh_crate_nr,
    long *pl_loc_hwacc,  long *pl_rem_cam, long *pl_dat, void
    *ps_veshe, long *l_se_read_len, long *l_read_stat)
{
	(void)bh_crate_nr;
	(void)pl_loc_hwacc;
	(void)pl_rem_cam;
	(void)ps_veshe;
    assert(!"f_user_readout() not implemented");
	return 0;
}


void
log_callback(char const *a_file, int a_line_no, unsigned a_level, char const
    *a_str)
{
	int lwroc_log_lvl;

	switch (a_level) {
#define LOG_LEVEL(FROM, TO) \
	case KW_##FROM: lwroc_log_lvl = LWROC_MSGLVL_##TO;  break;
	LOG_LEVEL(INFO, INFO);
	LOG_LEVEL(VERBOSE, LOG);
	LOG_LEVEL(DEBUG, DEBUG);
	LOG_LEVEL(SPAM, SPAM);
	LOG_LEVEL(ERROR, ERROR);
	default:
		lwroc_log_lvl = LWROC_MSGLVL_BUG;
		break;
	}
	lwroc_message_internal(lwroc_log_lvl, NULL, a_file, a_line_no, "%s",
	    a_str);
}
