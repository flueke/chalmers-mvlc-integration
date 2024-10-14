// main() is in drasi/lwroc/lwroc_main.c
// Structure and code taken from nrudlib/fuser/fuser.c

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
#	include <f_user_daq.h>
#	include <util/thread.h>

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
untriggered_loop(int *start_no_stop)
{
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
