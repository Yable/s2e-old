/*
 * QEMU System Emulator
 *
 * Copyright (c) 2003-2008 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

/*
 * The file was modified for S2E Selective Symbolic Execution Framework
 *
 * Copyright (c) 2010, Dependable Systems Laboratory, EPFL
 *
 * Currently maintained by:
 *    Volodymyr Kuznetsov <vova.kuznetsov@epfl.ch>
 *    Vitaly Chipounov <vitaly.chipounov@epfl.ch>
 *
 * All contributors are listed in S2E-AUTHORS file.
 *
 */

/* the following is needed on Linux to define ptsname() in stdlib.h */
#if defined(__linux__)
#define _GNU_SOURCE 1
#endif

#include "qemu-common.h"
#include "hw/hw.h"
#include "hw/boards.h"
#include "hw/usb.h"
#include "hw/pcmcia.h"
#include "hw/pc.h"
#include "hw/pci.h" //S2E
#include "hw/audiodev.h"
#include "hw/isa.h"
#include "hw/baum.h"
#include "hw/goldfish_nand.h"
#include "net.h"
#include "console.h"
#include "sysemu.h"
#include "gdbstub.h"
#include "qemu-timer.h"
#include "qemu-char.h"
#include "blockdev.h"
#include "audio/audio.h"

#include "qemu_file.h"
#include "android/android.h"
#include "charpipe.h"
#include "modem_driver.h"
#include "android/gps.h"
#include "android/hw-qemud.h"
#include "android/hw-kmsg.h"
#include "android/charmap.h"
#include "android/globals.h"
#include "android/utils/bufprint.h"
#include "android/utils/debug.h"
#include "android/utils/filelock.h"
#include "android/utils/path.h"
#include "android/utils/stralloc.h"
#include "android/utils/tempfile.h"
#include "android/display-core.h"
#include "android/utils/timezone.h"
#include "android/snapshot.h"
#include "targphys.h"
#include "tcpdump.h"

#ifdef CONFIG_MEMCHECK
#include "memcheck/memcheck.h"
#endif  // CONFIG_MEMCHECK

#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>
#include <errno.h>
#include <sys/time.h>
#include <zlib.h>

/* Needed early for CONFIG_BSD etc. */
#include "config-host.h"

#ifndef _WIN32
#include <libgen.h>
#include <pwd.h>
#include <sys/times.h>
#include <sys/wait.h>
#include <termios.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#if defined(__NetBSD__)
#include <net/if_tap.h>
#endif
#ifdef __linux__
#include <linux/if_tun.h>
#endif
#include <arpa/inet.h>
#include <dirent.h>
#include <netdb.h>
#include <sys/select.h>
#ifdef CONFIG_BSD
#include <sys/stat.h>
#if defined(__FreeBSD__) || defined(__DragonFly__)
#include <libutil.h>
#else
#include <util.h>
#endif
#elif defined (__GLIBC__) && defined (__FreeBSD_kernel__)
#include <freebsd/stdlib.h>
#else
#ifdef __linux__
#include <pty.h>
#include <malloc.h>
#include <linux/rtc.h>

/* For the benefit of older linux systems which don't supply it,
   we use a local copy of hpet.h. */
/* #include <linux/hpet.h> */
#include "hpet.h"

#include <linux/ppdev.h>
#include <linux/parport.h>
#endif
#ifdef __sun__
#include <sys/stat.h>
#include <sys/ethernet.h>
#include <sys/sockio.h>
#include <netinet/arp.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h> // must come after ip.h
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <net/if.h>
#include <syslog.h>
#include <stropts.h>
#endif
#endif
#endif

#if defined(__OpenBSD__)
#include <util.h>
#endif

#if defined(CONFIG_VDE)
#include <libvdeplug.h>
#endif

#ifdef _WIN32
#include <windows.h>
#include <malloc.h>
#include <sys/timeb.h>
#include <mmsystem.h>
#define getopt_long_only getopt_long
#define memalign(align, size) malloc(size)
#endif


#ifdef CONFIG_COCOA
#undef main
#define main qemu_main
#endif /* CONFIG_COCOA */

#include "hw/hw.h"
#include "hw/boards.h"
#include "hw/usb.h"
#include "hw/pcmcia.h"
#include "hw/pc.h"
#include "hw/audiodev.h"
#include "hw/isa.h"
#include "hw/baum.h"
#include "hw/bt.h"
#include "hw/watchdog.h"
#include "hw/smbios.h"
#include "hw/xen.h"
#include "bt-host.h"
#include "net.h"
#include "monitor.h"
#include "console.h"
#include "sysemu.h"
#include "gdbstub.h"
#include "qemu-timer.h"
#include "qemu-char.h"
#include "cache-utils.h"
#include "block.h"
#include "dma.h"
#include "audio/audio.h"
#include "migration.h"
#include "kvm.h"
#include "balloon.h"
#include "android/hw-lcd.h"
#include "android/boot-properties.h"
#include "android/core-init-utils.h"
#include "android/audio-test.h"

#ifdef CONFIG_STANDALONE_CORE
/* Verbose value used by the standalone emulator core (without UI) */
unsigned long   android_verbose;
#endif  // CONFIG_STANDALONE_CORE

#if !defined(CONFIG_STANDALONE_CORE)
/* in android/qemulator.c */
extern void  android_emulator_set_base_port(int  port);
#endif

#if defined(CONFIG_SKINS) && !defined(CONFIG_STANDALONE_CORE)
#undef main
#define main qemu_main
#endif

#include "disas.h"

#include "exec-all.h"

#ifdef CONFIG_TRACE
#include "trace.h"
#include "dcache.h"
#endif

#include "qemu_socket.h"

#if defined(CONFIG_SLIRP)
#include "libslirp.h"
#endif

#ifdef CONFIG_S2E
extern const char* s2e_output_dir;
#endif


#ifdef CONFIG_LLVM
#include <tcg-llvm.h>
#endif

#ifdef CONFIG_S2E
#include <s2e/s2e_qemu.h>
#endif


#define DEFAULT_RAM_SIZE 128

/* Max number of USB devices that can be specified on the commandline.  */
#define MAX_USB_CMDLINE 8

/* Max number of bluetooth switches on the commandline.  */
#define MAX_BT_CMDLINE 10

/* XXX: use a two level table to limit memory usage */

static const char *data_dir;
const char *bios_name = NULL;
static void *ioport_opaque[MAX_IOPORTS];
static IOPortReadFunc *ioport_read_table[3][MAX_IOPORTS];
static IOPortWriteFunc *ioport_write_table[3][MAX_IOPORTS];
#ifdef MAX_DRIVES
/* Note: drives_table[MAX_DRIVES] is a dummy block driver if none available
   to store the VM snapshots */
DriveInfo drives_table[MAX_DRIVES+1];
int nb_drives;
#endif
enum vga_retrace_method vga_retrace_method = VGA_RETRACE_DUMB;
DisplayType display_type = DT_DEFAULT;
const char* keyboard_layout = NULL;
int64_t ticks_per_sec;
ram_addr_t ram_size;
int nb_nics;
NICInfo nd_table[MAX_NICS];
int vm_running;
int autostart;
static int rtc_utc = 1;
static int rtc_date_offset = -1; /* -1 means no change */
int cirrus_vga_enabled = 1;
int std_vga_enabled = 0;
int vmsvga_enabled = 0;
int xenfb_enabled = 0;
QEMUClock *rtc_clock;
#ifdef TARGET_SPARC
int graphic_width = 1024;
int graphic_height = 768;
int graphic_depth = 8;
#else
int graphic_width = 800;
int graphic_height = 600;
int graphic_depth = 15;
#endif
static int full_screen = 0;
#ifdef CONFIG_SDL
static int no_frame = 0;
#endif
int no_quit = 0;
CharDriverState *serial_hds[MAX_SERIAL_PORTS];
int              serial_hds_count;

CharDriverState *parallel_hds[MAX_PARALLEL_PORTS];
CharDriverState *virtcon_hds[MAX_VIRTIO_CONSOLES];
#ifdef TARGET_I386
int win2k_install_hack = 0;
int rtc_td_hack = 0;
#endif
int usb_enabled = 0;
int singlestep = 0;
int smp_cpus = 1;
const char *vnc_display;
int acpi_enabled = 1;
int no_hpet = 1;
int no_virtio_balloon = 0;
int fd_bootchk = 1;
int no_reboot = 0;
int no_shutdown = 0;
int cursor_hide = 1;
int graphic_rotate = 0;
#ifndef _WIN32
int daemonize = 0;
#endif
WatchdogTimerModel *watchdog = NULL;
int watchdog_action = WDT_RESET;
const char *option_rom[MAX_OPTION_ROMS];
int nb_option_roms;
int semihosting_enabled = 0;
#ifdef TARGET_ARM
int old_param = 0;
#endif
const char *qemu_name;
int alt_grab = 0;
#if defined(TARGET_SPARC) || defined(TARGET_PPC)
unsigned int nb_prom_envs = 0;
const char *prom_envs[MAX_PROM_ENVS];
#endif
#ifdef MAX_DRIVES
int nb_drives_opt;
struct drive_opt drives_opt[MAX_DRIVES];
#endif
int nb_numa_nodes;
uint64_t node_mem[MAX_NODES];
uint64_t node_cpumask[MAX_NODES];

static CPUState *cur_cpu;
static CPUState *next_cpu;
static QEMUTimer *nographic_timer;

uint8_t qemu_uuid[16];


int   qemu_cpu_delay;
extern char* audio_input_source;

extern char* android_op_ports;
extern char* android_op_port;
extern char* android_op_report_console;
extern char* op_http_proxy;
// Path to the file containing specific key character map.
char* op_charmap_file = NULL;

/* Path to hardware initialization file passed with -android-hw option. */
char* android_op_hwini = NULL;

/* Memory checker options. */
char* android_op_memcheck = NULL;

/* -dns-server option value. */
char* android_op_dns_server = NULL;

/* -radio option value. */
char* android_op_radio = NULL;

/* -gps option value. */
char* android_op_gps = NULL;

/* -audio option value. */
char* android_op_audio = NULL;

/* -cpu-delay option value. */
char* android_op_cpu_delay = NULL;

#ifdef CONFIG_NAND_LIMITS
/* -nand-limits option value. */
char* android_op_nand_limits = NULL;
#endif  // CONFIG_NAND_LIMITS

/* -netspeed option value. */
char* android_op_netspeed = NULL;

/* -netdelay option value. */
char* android_op_netdelay = NULL;

/* -netfast option value. */
int android_op_netfast = 0;

/* -tcpdump option value. */
char* android_op_tcpdump = NULL;

/* -lcd-density option value. */
char* android_op_lcd_density = NULL;

/* -ui-port option value. This port will be used to report the core
 * initialization completion.
 */
char* android_op_ui_port = NULL;

/* -ui-settings option value. This value will be passed to the UI when new UI
 * process is attaching to the core.
 */
char* android_op_ui_settings = NULL;

/* -android-avdname option value. */
char* android_op_avd_name = "unknown";

extern int android_display_width;
extern int android_display_height;
extern int android_display_bpp;

extern void  dprint( const char* format, ... );

const char* dns_log_filename = NULL;
const char* drop_log_filename = NULL;
static int rotate_logs_requested = 0;

const char* savevm_on_exit = NULL;

#define TFR(expr) do { if ((expr) != -1) break; } while (errno == EINTR)


#if !defined(CONFIG_S2E)
fake_pci_t g_fake_pci;
#endif

/* Reports the core initialization failure to the error stdout and to the UI
 * socket before exiting the application.
 * Parameters that are passed to this macro are used to format the error
 * mesage using sprintf routine.
 */
#ifdef CONFIG_ANDROID
#define  PANIC(...) android_core_init_failure(__VA_ARGS__)
#else
#define  PANIC(...) do { fprintf(stderr, __VA_ARGS__);  \
                         exit(1);                       \
                    } while (0)
#endif  // CONFIG_ANDROID

/* Exits the core during initialization. */
#ifdef CONFIG_ANDROID
#define  QEMU_EXIT(exit_code) android_core_init_exit(exit_code)
#else
#define  QEMU_EXIT(exit_code) exit(exit_code)
#endif  // CONFIG_ANDROID

/***********************************************************/
/* x86 ISA bus support */

target_phys_addr_t isa_mem_base = 0;
PicState2 *isa_pic;

static IOPortReadFunc default_ioport_readb, default_ioport_readw, default_ioport_readl;
static IOPortWriteFunc default_ioport_writeb, default_ioport_writew, default_ioport_writel;

static uint32_t ioport_read(int index, uint32_t address)
{
    static IOPortReadFunc *default_func[3] = {
        default_ioport_readb,
        default_ioport_readw,
        default_ioport_readl
    };
    IOPortReadFunc *func = ioport_read_table[index][address];
    if (!func)
        func = default_func[index];
    return func(ioport_opaque[address], address);
}

static void ioport_write(int index, uint32_t address, uint32_t data)
{
    static IOPortWriteFunc *default_func[3] = {
        default_ioport_writeb,
        default_ioport_writew,
        default_ioport_writel
    };
    IOPortWriteFunc *func = ioport_write_table[index][address];
    if (!func)
        func = default_func[index];
    func(ioport_opaque[address], address, data);
}

static uint32_t default_ioport_readb(void *opaque, uint32_t address)
{
#ifdef DEBUG_UNUSED_IOPORT
    fprintf(stderr, "unused inb: port=0x%04x\n", address);
#endif
    return 0xff;
}

static void default_ioport_writeb(void *opaque, uint32_t address, uint32_t data)
{
#ifdef DEBUG_UNUSED_IOPORT
    fprintf(stderr, "unused outb: port=0x%04x data=0x%02x\n", address, data);
#endif
}

/* default is to make two byte accesses */
static uint32_t default_ioport_readw(void *opaque, uint32_t address)
{
    uint32_t data;
    data = ioport_read(0, address);
    address = (address + 1) & (MAX_IOPORTS - 1);
    data |= ioport_read(0, address) << 8;
    return data;
}

static void default_ioport_writew(void *opaque, uint32_t address, uint32_t data)
{
    ioport_write(0, address, data & 0xff);
    address = (address + 1) & (MAX_IOPORTS - 1);
    ioport_write(0, address, (data >> 8) & 0xff);
}

static uint32_t default_ioport_readl(void *opaque, uint32_t address)
{
#ifdef DEBUG_UNUSED_IOPORT
    fprintf(stderr, "unused inl: port=0x%04x\n", address);
#endif
    return 0xffffffff;
}

static void default_ioport_writel(void *opaque, uint32_t address, uint32_t data)
{
#ifdef DEBUG_UNUSED_IOPORT
    fprintf(stderr, "unused outl: port=0x%04x data=0x%02x\n", address, data);
#endif
}

/*
 * Sets a flag (rotate_logs_requested) to clear both the DNS and the
 * drop logs upon receiving a SIGUSR1 signal. We need to clear the logs
 * between the tasks that do not require restarting Qemu.
 */
void rotate_qemu_logs_handler(int signum) {
  rotate_logs_requested = 1;
}

/*
 * Resets the rotate_log_requested_flag. Normally called after qemu
 * logs has been rotated.
 */
void reset_rotate_qemu_logs_request(void) {
  rotate_logs_requested = 0;
}

/*
 * Clears the passed qemu log when the rotate_logs_requested
 * is set. We need to clear the logs between the tasks that do not
 * require restarting Qemu.
 */
FILE* rotate_qemu_log(FILE* old_log_fd, const char* filename) {
  FILE* new_log_fd = NULL;
  if (old_log_fd) {
    if (fclose(old_log_fd) == -1) {
      fprintf(stderr, "Cannot close old_log fd\n");
      exit(errno);
    }
  }

  if (!filename) {
    fprintf(stderr, "The log filename to be rotated is not provided");
    exit(-1);
  }

  new_log_fd = fopen(filename , "wb+");
  if (new_log_fd == NULL) {
    fprintf(stderr, "Cannot open the log file: %s for write.\n",
            filename);
    exit(1);
  }

  return new_log_fd;
}

/***********************************************************/
void hw_error(const char *fmt, ...)
{
    va_list ap;
    CPUState *env;

    va_start(ap, fmt);
    fprintf(stderr, "qemu: hardware error: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    for(env = first_cpu; env != NULL; env = env->next_cpu) {
        fprintf(stderr, "CPU #%d:\n", env->cpu_index);
#ifdef TARGET_I386
        cpu_dump_state(env, stderr, fprintf, X86_DUMP_FPU);
#else
        cpu_dump_state(env, stderr, fprintf, 0);
#endif
    }
    va_end(ap);
    abort();
}

/***************/
/* ballooning */

static QEMUBalloonEvent *qemu_balloon_event;
void *qemu_balloon_event_opaque;

void qemu_add_balloon_handler(QEMUBalloonEvent *func, void *opaque)
{
    qemu_balloon_event = func;
    qemu_balloon_event_opaque = opaque;
}

void qemu_balloon(ram_addr_t target)
{
    if (qemu_balloon_event)
        qemu_balloon_event(qemu_balloon_event_opaque, target);
}

ram_addr_t qemu_balloon_status(void)
{
    if (qemu_balloon_event)
        return qemu_balloon_event(qemu_balloon_event_opaque, 0);
    return 0;
}

/***********************************************************/
/* real time host monotonic timer */

/* compute with 96 bit intermediate result: (a*b)/c */
uint64_t muldiv64(uint64_t a, uint32_t b, uint32_t c)
{
    union {
        uint64_t ll;
        struct {
#ifdef HOST_WORDS_BIGENDIAN
            uint32_t high, low;
#else
            uint32_t low, high;
#endif
        } l;
    } u, res;
    uint64_t rl, rh;

    u.ll = a;
    rl = (uint64_t)u.l.low * (uint64_t)b;
    rh = (uint64_t)u.l.high * (uint64_t)b;
    rh += (rl >> 32);
    res.l.high = rh / c;
    res.l.low = (((rh % c) << 32) + (rl & 0xffffffff)) / c;
    return res.ll;
}

/***********************************************************/
/* host time/date access */
void qemu_get_timedate(struct tm *tm, int offset)
{
    time_t ti;
    struct tm *ret;

    time(&ti);
    ti += offset;
    if (rtc_date_offset == -1) {
        if (rtc_utc)
            ret = gmtime(&ti);
        else
            ret = localtime(&ti);
    } else {
        ti -= rtc_date_offset;
        ret = gmtime(&ti);
    }

    memcpy(tm, ret, sizeof(struct tm));
}

int qemu_timedate_diff(struct tm *tm)
{
    time_t seconds;

    if (rtc_date_offset == -1)
        if (rtc_utc)
            seconds = mktimegm(tm);
        else
            seconds = mktime(tm);
    else
        seconds = mktimegm(tm) + rtc_date_offset;

    return seconds - time(NULL);
}


#ifdef CONFIG_TRACE
static int tbflush_requested;
static int exit_requested;

void start_tracing()
{
  if (trace_filename == NULL)
    return;
  if (!tracing) {
    fprintf(stderr,"-- start tracing --\n");
    start_time = Now();
  }
  tracing = 1;
  tbflush_requested = 1;
  qemu_notify_event();
}

void stop_tracing()
{
  if (trace_filename == NULL)
    return;
  if (tracing) {
    end_time = Now();
    elapsed_usecs += end_time - start_time;
    fprintf(stderr,"-- stop tracing --\n");
  }
  tracing = 0;
  tbflush_requested = 1;
  qemu_notify_event();
}

#ifndef _WIN32
/* This is the handler for the SIGUSR1 and SIGUSR2 signals.
 * SIGUSR1 turns tracing on.  SIGUSR2 turns tracing off.
 */
void sigusr_handler(int sig)
{
  if (sig == SIGUSR1)
    start_tracing();
  else
    stop_tracing();
}
#endif

/* This is the handler to catch control-C so that we can exit cleanly.
 * This is needed when tracing to flush the buffers to disk.
 */
void sigint_handler(int sig)
{
  exit_requested = 1;
  qemu_notify_event();
}
#endif /* CONFIG_TRACE */


#ifdef CONFIG_S2E
static void s2e_cleanup(void)
{
    if(g_s2e) {
        s2e_close(g_s2e);
        g_s2e = NULL;
    }
}
#endif

#ifdef CONFIG_LLVM
static void tcg_llvm_cleanup(void)
{
    if(tcg_llvm_ctx) {
        tcg_llvm_close(tcg_llvm_ctx);
        tcg_llvm_ctx = NULL;
    }
}
#endif


/***********************************************************/
/* Bluetooth support */
static int nb_hcis;
static int cur_hci;
static struct HCIInfo *hci_table[MAX_NICS];

static struct bt_vlan_s {
    struct bt_scatternet_s net;
    int id;
    struct bt_vlan_s *next;
} *first_bt_vlan;

/* find or alloc a new bluetooth "VLAN" */
static struct bt_scatternet_s *qemu_find_bt_vlan(int id)
{
    struct bt_vlan_s **pvlan, *vlan;
    for (vlan = first_bt_vlan; vlan != NULL; vlan = vlan->next) {
        if (vlan->id == id)
            return &vlan->net;
    }
    vlan = qemu_mallocz(sizeof(struct bt_vlan_s));
    vlan->id = id;
    pvlan = &first_bt_vlan;
    while (*pvlan != NULL)
        pvlan = &(*pvlan)->next;
    *pvlan = vlan;
    return &vlan->net;
}

static void null_hci_send(struct HCIInfo *hci, const uint8_t *data, int len)
{
}

static int null_hci_addr_set(struct HCIInfo *hci, const uint8_t *bd_addr)
{
    return -ENOTSUP;
}

static struct HCIInfo null_hci = {
    .cmd_send = null_hci_send,
    .sco_send = null_hci_send,
    .acl_send = null_hci_send,
    .bdaddr_set = null_hci_addr_set,
};

struct HCIInfo *qemu_next_hci(void)
{
    if (cur_hci == nb_hcis)
        return &null_hci;

    return hci_table[cur_hci++];
}

static struct HCIInfo *hci_init(const char *str)
{
    char *endp;
    struct bt_scatternet_s *vlan = 0;

    if (!strcmp(str, "null"))
        /* null */
        return &null_hci;
    else if (!strncmp(str, "host", 4) && (str[4] == '\0' || str[4] == ':'))
        /* host[:hciN] */
        return bt_host_hci(str[4] ? str + 5 : "hci0");
    else if (!strncmp(str, "hci", 3)) {
        /* hci[,vlan=n] */
        if (str[3]) {
            if (!strncmp(str + 3, ",vlan=", 6)) {
                vlan = qemu_find_bt_vlan(strtol(str + 9, &endp, 0));
                if (*endp)
                    vlan = 0;
            }
        } else
            vlan = qemu_find_bt_vlan(0);
        if (vlan)
           return bt_new_hci(vlan);
    }

    fprintf(stderr, "qemu: Unknown bluetooth HCI `%s'.\n", str);

    return 0;
}

static int bt_hci_parse(const char *str)
{
    struct HCIInfo *hci;
    bdaddr_t bdaddr;

    if (nb_hcis >= MAX_NICS) {
        fprintf(stderr, "qemu: Too many bluetooth HCIs (max %i).\n", MAX_NICS);
        return -1;
    }

    hci = hci_init(str);
    if (!hci)
        return -1;

    bdaddr.b[0] = 0x52;
    bdaddr.b[1] = 0x54;
    bdaddr.b[2] = 0x00;
    bdaddr.b[3] = 0x12;
    bdaddr.b[4] = 0x34;
    bdaddr.b[5] = 0x56 + nb_hcis;
    hci->bdaddr_set(hci, bdaddr.b);

    hci_table[nb_hcis++] = hci;

    return 0;
}

static void bt_vhci_add(int vlan_id)
{
    struct bt_scatternet_s *vlan = qemu_find_bt_vlan(vlan_id);

    if (!vlan->slave)
        fprintf(stderr, "qemu: warning: adding a VHCI to "
                        "an empty scatternet %i\n", vlan_id);

    bt_vhci_init(bt_new_hci(vlan));
}

static struct bt_device_s *bt_device_add(const char *opt)
{
    struct bt_scatternet_s *vlan;
    int vlan_id = 0;
    char *endp = strstr(opt, ",vlan=");
    int len = (endp ? endp - opt : strlen(opt)) + 1;
    char devname[10];

    pstrcpy(devname, MIN(sizeof(devname), len), opt);

    if (endp) {
        vlan_id = strtol(endp + 6, &endp, 0);
        if (*endp) {
            fprintf(stderr, "qemu: unrecognised bluetooth vlan Id\n");
            return 0;
        }
    }

    vlan = qemu_find_bt_vlan(vlan_id);

    if (!vlan->slave)
        fprintf(stderr, "qemu: warning: adding a slave device to "
                        "an empty scatternet %i\n", vlan_id);

    if (!strcmp(devname, "keyboard"))
        return bt_keyboard_init(vlan);

    fprintf(stderr, "qemu: unsupported bluetooth device `%s'\n", devname);
    return 0;
}

static int bt_parse(const char *opt)
{
    const char *endp, *p;
    int vlan;

    if (strstart(opt, "hci", &endp)) {
        if (!*endp || *endp == ',') {
            if (*endp)
                if (!strstart(endp, ",vlan=", 0))
                    opt = endp + 1;

            return bt_hci_parse(opt);
       }
    } else if (strstart(opt, "vhci", &endp)) {
        if (!*endp || *endp == ',') {
            if (*endp) {
                if (strstart(endp, ",vlan=", &p)) {
                    vlan = strtol(p, (char **) &endp, 0);
                    if (*endp) {
                        fprintf(stderr, "qemu: bad scatternet '%s'\n", p);
                        return 1;
                    }
                } else {
                    fprintf(stderr, "qemu: bad parameter '%s'\n", endp + 1);
                    return 1;
                }
            } else
                vlan = 0;

            bt_vhci_add(vlan);
            return 0;
        }
    } else if (strstart(opt, "device:", &endp))
        return !bt_device_add(endp);

    fprintf(stderr, "qemu: bad bluetooth parameter '%s'\n", opt);
    return 1;
}

/***********************************************************/
/* QEMU Block devices */

#define HD_ALIAS "index=%d,media=disk"
#define CDROM_ALIAS "index=2,media=cdrom"
#define FD_ALIAS "index=%d,if=floppy"
#define PFLASH_ALIAS "if=pflash"
#define MTD_ALIAS "if=mtd"
#define SD_ALIAS "index=0,if=sd"

static int drive_init_func(QemuOpts *opts, void *opaque)
{
    int *use_scsi = opaque;
    int fatal_error = 0;

    if (drive_init(opts, *use_scsi, &fatal_error) == NULL) {
        if (fatal_error)
            return 1;
    }
    return 0;
}

static int drive_enable_snapshot(QemuOpts *opts, void *opaque)
{
    if (NULL == qemu_opt_get(opts, "snapshot")) {
        qemu_opt_set(opts, "snapshot", "on");
    }
    return 0;
}

#ifdef MAX_DRIVES
static int drive_opt_get_free_idx(void)
{
    int index;

    for (index = 0; index < MAX_DRIVES; index++)
        if (!drives_opt[index].used) {
            drives_opt[index].used = 1;
            return index;
        }

    return -1;
}

static int drive_get_free_idx(void)
{
    int index;

    for (index = 0; index < MAX_DRIVES; index++)
        if (!drives_table[index].used) {
            drives_table[index].used = 1;
            return index;
        }

    return -1;
}

int drive_add(const char *file, const char *fmt, ...)
{
    va_list ap;
    int index = drive_opt_get_free_idx();

    if (nb_drives_opt >= MAX_DRIVES || index == -1) {
        fprintf(stderr, "qemu: too many drives\n");
        return -1;
    }

    drives_opt[index].file = file;
    va_start(ap, fmt);
    vsnprintf(drives_opt[index].opt,
              sizeof(drives_opt[0].opt), fmt, ap);
    va_end(ap);

    nb_drives_opt++;
    return index;
}

void drive_remove(int index)
{
    drives_opt[index].used = 0;
    nb_drives_opt--;
}

int drive_get_index(BlockInterfaceType type, int bus, int unit)
{
    int index;

    /* seek interface, bus and unit */

    for (index = 0; index < MAX_DRIVES; index++)
        if (drives_table[index].type == type &&
	    drives_table[index].bus == bus &&
	    drives_table[index].unit == unit &&
	    drives_table[index].used)
        return index;

    return -1;
}

int drive_get_max_bus(BlockInterfaceType type)
{
    int max_bus;
    int index;

    max_bus = -1;
    for (index = 0; index < nb_drives; index++) {
        if(drives_table[index].type == type &&
           drives_table[index].bus > max_bus)
            max_bus = drives_table[index].bus;
    }
    return max_bus;
}

const char *drive_get_serial(BlockDriverState *bdrv)
{
    int index;

    for (index = 0; index < nb_drives; index++)
        if (drives_table[index].bdrv == bdrv)
            return drives_table[index].serial;

    return "\0";
}

BlockInterfaceErrorAction drive_get_onerror(BlockDriverState *bdrv)
{
    int index;

    for (index = 0; index < nb_drives; index++)
        if (drives_table[index].bdrv == bdrv)
            return drives_table[index].onerror;

    return BLOCK_ERR_STOP_ENOSPC;
}

static void bdrv_format_print(void *opaque, const char *name)
{
    fprintf(stderr, " %s", name);
}

void drive_uninit(BlockDriverState *bdrv)
{
    int i;

    for (i = 0; i < MAX_DRIVES; i++)
        if (drives_table[i].bdrv == bdrv) {
            drives_table[i].bdrv = NULL;
            drives_table[i].used = 0;
            drive_remove(drives_table[i].drive_opt_idx);
            nb_drives--;
            break;
        }
}

int drive_init(struct drive_opt *arg, int snapshot, void *opaque)
{
    char buf[128];
    char file[1024];
    char devname[128];
    char serial[21];
    const char *mediastr = "";
    BlockInterfaceType type;
    enum { MEDIA_DISK, MEDIA_CDROM } media;
    int bus_id, unit_id;
    int cyls, heads, secs, translation;
    BlockDriverState *bdrv;
    BlockDriver *drv = NULL;
    QEMUMachine *machine = opaque;
    int max_devs;
    int index;
    int cache;
    int bdrv_flags, onerror;
    int drives_table_idx;
    char *str = arg->opt;
    static const char * const params[] = { "bus", "unit", "if", "index",
                                           "cyls", "heads", "secs", "trans",
                                           "media", "snapshot", "file",
                                           "cache", "format", "serial", "werror",
                                           NULL };

    if (check_params(buf, sizeof(buf), params, str) < 0) {
         fprintf(stderr, "qemu: unknown parameter '%s' in '%s'\n",
                         buf, str);
         return -1;
    }

    file[0] = 0;
    cyls = heads = secs = 0;
    bus_id = 0;
    unit_id = -1;
    translation = BIOS_ATA_TRANSLATION_AUTO;
    index = -1;
    cache = 3;

    if (machine->use_scsi) {
        type = IF_SCSI;
        max_devs = MAX_SCSI_DEVS;
        pstrcpy(devname, sizeof(devname), "scsi");
    } else {
        type = IF_IDE;
        max_devs = MAX_IDE_DEVS;
        pstrcpy(devname, sizeof(devname), "ide");
    }
    media = MEDIA_DISK;

    /* extract parameters */

    if (get_param_value(buf, sizeof(buf), "bus", str)) {
        bus_id = strtol(buf, NULL, 0);
	if (bus_id < 0) {
	    fprintf(stderr, "qemu: '%s' invalid bus id\n", str);
	    return -1;
	}
    }

    if (get_param_value(buf, sizeof(buf), "unit", str)) {
        unit_id = strtol(buf, NULL, 0);
	if (unit_id < 0) {
	    fprintf(stderr, "qemu: '%s' invalid unit id\n", str);
	    return -1;
	}
    }

    if (get_param_value(buf, sizeof(buf), "if", str)) {
        pstrcpy(devname, sizeof(devname), buf);
        if (!strcmp(buf, "ide")) {
	    type = IF_IDE;
            max_devs = MAX_IDE_DEVS;
        } else if (!strcmp(buf, "scsi")) {
	    type = IF_SCSI;
            max_devs = MAX_SCSI_DEVS;
        } else if (!strcmp(buf, "floppy")) {
	    type = IF_FLOPPY;
            max_devs = 0;
        } else if (!strcmp(buf, "pflash")) {
	    type = IF_PFLASH;
            max_devs = 0;
	} else if (!strcmp(buf, "mtd")) {
	    type = IF_MTD;
            max_devs = 0;
	} else if (!strcmp(buf, "sd")) {
	    type = IF_SD;
            max_devs = 0;
        } else if (!strcmp(buf, "virtio")) {
            type = IF_VIRTIO;
            max_devs = 0;
	} else if (!strcmp(buf, "xen")) {
	    type = IF_XEN;
            max_devs = 0;
	} else {
            fprintf(stderr, "qemu: '%s' unsupported bus type '%s'\n", str, buf);
            return -1;
	}
    }

    if (get_param_value(buf, sizeof(buf), "index", str)) {
        index = strtol(buf, NULL, 0);
	if (index < 0) {
	    fprintf(stderr, "qemu: '%s' invalid index\n", str);
	    return -1;
	}
    }

    if (get_param_value(buf, sizeof(buf), "cyls", str)) {
        cyls = strtol(buf, NULL, 0);
    }

    if (get_param_value(buf, sizeof(buf), "heads", str)) {
        heads = strtol(buf, NULL, 0);
    }

    if (get_param_value(buf, sizeof(buf), "secs", str)) {
        secs = strtol(buf, NULL, 0);
    }

    if (cyls || heads || secs) {
        if (cyls < 1 || cyls > 16383) {
            fprintf(stderr, "qemu: '%s' invalid physical cyls number\n", str);
	    return -1;
	}
        if (heads < 1 || heads > 16) {
            fprintf(stderr, "qemu: '%s' invalid physical heads number\n", str);
	    return -1;
	}
        if (secs < 1 || secs > 63) {
            fprintf(stderr, "qemu: '%s' invalid physical secs number\n", str);
	    return -1;
	}
    }

    if (get_param_value(buf, sizeof(buf), "trans", str)) {
        if (!cyls) {
            fprintf(stderr,
                    "qemu: '%s' trans must be used with cyls,heads and secs\n",
                    str);
            return -1;
        }
        if (!strcmp(buf, "none"))
            translation = BIOS_ATA_TRANSLATION_NONE;
        else if (!strcmp(buf, "lba"))
            translation = BIOS_ATA_TRANSLATION_LBA;
        else if (!strcmp(buf, "auto"))
            translation = BIOS_ATA_TRANSLATION_AUTO;
	else {
            fprintf(stderr, "qemu: '%s' invalid translation type\n", str);
	    return -1;
	}
    }

    if (get_param_value(buf, sizeof(buf), "media", str)) {
        if (!strcmp(buf, "disk")) {
	    media = MEDIA_DISK;
	} else if (!strcmp(buf, "cdrom")) {
            if (cyls || secs || heads) {
                fprintf(stderr,
                        "qemu: '%s' invalid physical CHS format\n", str);
	        return -1;
            }
	    media = MEDIA_CDROM;
	} else {
	    fprintf(stderr, "qemu: '%s' invalid media\n", str);
	    return -1;
	}
    }

    if (get_param_value(buf, sizeof(buf), "snapshot", str)) {
        if (!strcmp(buf, "on"))
	    snapshot = 1;
        else if (!strcmp(buf, "off"))
	    snapshot = 0;
	else {
	    fprintf(stderr, "qemu: '%s' invalid snapshot option\n", str);
	    return -1;
	}
    }

    if (get_param_value(buf, sizeof(buf), "cache", str)) {
        if (!strcmp(buf, "off") || !strcmp(buf, "none"))
            cache = 0;
        else if (!strcmp(buf, "writethrough"))
            cache = 1;
        else if (!strcmp(buf, "writeback"))
            cache = 2;
        else {
           fprintf(stderr, "qemu: invalid cache option\n");
           return -1;
        }
    }

    if (get_param_value(buf, sizeof(buf), "format", str)) {
       if (strcmp(buf, "?") == 0) {
            fprintf(stderr, "qemu: Supported formats:");
            bdrv_iterate_format(bdrv_format_print, NULL);
            fprintf(stderr, "\n");
	    return -1;
        }
        drv = bdrv_find_format(buf);
        if (!drv) {
            fprintf(stderr, "qemu: '%s' invalid format\n", buf);
            return -1;
        }
    }

    if (arg->file == NULL)
        get_param_value(file, sizeof(file), "file", str);
    else
        pstrcpy(file, sizeof(file), arg->file);

    if (!get_param_value(serial, sizeof(serial), "serial", str))
	    memset(serial, 0,  sizeof(serial));

    onerror = BLOCK_ERR_STOP_ENOSPC;
    if (get_param_value(buf, sizeof(serial), "werror", str)) {
        if (type != IF_IDE && type != IF_SCSI && type != IF_VIRTIO) {
            fprintf(stderr, "werror is no supported by this format\n");
            return -1;
        }
        if (!strcmp(buf, "ignore"))
            onerror = BLOCK_ERR_IGNORE;
        else if (!strcmp(buf, "enospc"))
            onerror = BLOCK_ERR_STOP_ENOSPC;
        else if (!strcmp(buf, "stop"))
            onerror = BLOCK_ERR_STOP_ANY;
        else if (!strcmp(buf, "report"))
            onerror = BLOCK_ERR_REPORT;
        else {
            fprintf(stderr, "qemu: '%s' invalid write error action\n", buf);
            return -1;
        }
    }

    /* compute bus and unit according index */

    if (index != -1) {
        if (bus_id != 0 || unit_id != -1) {
            fprintf(stderr,
                    "qemu: '%s' index cannot be used with bus and unit\n", str);
            return -1;
        }
        if (max_devs == 0)
        {
            unit_id = index;
            bus_id = 0;
        } else {
            unit_id = index % max_devs;
            bus_id = index / max_devs;
        }
    }

    /* if user doesn't specify a unit_id,
     * try to find the first free
     */

    if (unit_id == -1) {
       unit_id = 0;
       while (drive_get_index(type, bus_id, unit_id) != -1) {
           unit_id++;
           if (max_devs && unit_id >= max_devs) {
               unit_id -= max_devs;
               bus_id++;
           }
       }
    }

    /* check unit id */

    if (max_devs && unit_id >= max_devs) {
        fprintf(stderr, "qemu: '%s' unit %d too big (max is %d)\n",
                        str, unit_id, max_devs - 1);
        return -1;
    }

    /*
     * ignore multiple definitions
     */

    if (drive_get_index(type, bus_id, unit_id) != -1)
        return -2;

    /* init */

    if (type == IF_IDE || type == IF_SCSI)
        mediastr = (media == MEDIA_CDROM) ? "-cd" : "-hd";
    if (max_devs)
        snprintf(buf, sizeof(buf), "%s%i%s%i",
                 devname, bus_id, mediastr, unit_id);
    else
        snprintf(buf, sizeof(buf), "%s%s%i",
                 devname, mediastr, unit_id);
    bdrv = bdrv_new(buf);
    drives_table_idx = drive_get_free_idx();
    drives_table[drives_table_idx].bdrv = bdrv;
    drives_table[drives_table_idx].type = type;
    drives_table[drives_table_idx].bus = bus_id;
    drives_table[drives_table_idx].unit = unit_id;
    drives_table[drives_table_idx].onerror = onerror;
    drives_table[drives_table_idx].drive_opt_idx = arg - drives_opt;
    strncpy(drives_table[drives_table_idx].serial, serial, sizeof(serial));
    nb_drives++;

    switch(type) {
    case IF_IDE:
    case IF_SCSI:
    case IF_XEN:
        switch(media) {
	case MEDIA_DISK:
            if (cyls != 0) {
                bdrv_set_geometry_hint(bdrv, cyls, heads, secs);
                bdrv_set_translation_hint(bdrv, translation);
            }
	    break;
	case MEDIA_CDROM:
            bdrv_set_type_hint(bdrv, BDRV_TYPE_CDROM);
	    break;
	}
        break;
    case IF_SD:
        /* FIXME: This isn't really a floppy, but it's a reasonable
           approximation.  */
    case IF_FLOPPY:
        bdrv_set_type_hint(bdrv, BDRV_TYPE_FLOPPY);
        break;
    case IF_PFLASH:
    case IF_MTD:
    case IF_VIRTIO:
        break;
    case IF_COUNT:
    case IF_NONE:
        abort();
    }
    if (!file[0])
        return -2;
    bdrv_flags = 0;
    if (snapshot) {
        bdrv_flags |= BDRV_O_SNAPSHOT;
        cache = 2; /* always use write-back with snapshot */
    }
    if (cache == 0) /* no caching */
        bdrv_flags |= BDRV_O_NOCACHE;
    else if (cache == 2) /* write-back */
        bdrv_flags |= BDRV_O_CACHE_WB;
    else if (cache == 3) /* not specified */
        bdrv_flags |= BDRV_O_CACHE_DEF;
    if (bdrv_open2(bdrv, file, bdrv_flags, drv) < 0) {
        fprintf(stderr, "qemu: could not open disk image %s\n",
                        file);
        return -1;
    }
    if (bdrv_key_required(bdrv))
        autostart = 0;
    return drives_table_idx;
}
#endif /* MAX_DRIVES */

static void numa_add(const char *optarg)
{
    char option[128];
    char *endptr;
    unsigned long long value, endvalue;
    int nodenr;

    optarg = get_opt_name(option, 128, optarg, ',') + 1;
    if (!strcmp(option, "node")) {
        if (get_param_value(option, 128, "nodeid", optarg) == 0) {
            nodenr = nb_numa_nodes;
        } else {
            nodenr = strtoull(option, NULL, 10);
        }

        if (get_param_value(option, 128, "mem", optarg) == 0) {
            node_mem[nodenr] = 0;
        } else {
            value = strtoull(option, &endptr, 0);
            switch (*endptr) {
            case 0: case 'M': case 'm':
                value <<= 20;
                break;
            case 'G': case 'g':
                value <<= 30;
                break;
            }
            node_mem[nodenr] = value;
        }
        if (get_param_value(option, 128, "cpus", optarg) == 0) {
            node_cpumask[nodenr] = 0;
        } else {
            value = strtoull(option, &endptr, 10);
            if (value >= 64) {
                value = 63;
                fprintf(stderr, "only 64 CPUs in NUMA mode supported.\n");
            } else {
                if (*endptr == '-') {
                    endvalue = strtoull(endptr+1, &endptr, 10);
                    if (endvalue >= 63) {
                        endvalue = 62;
                        fprintf(stderr,
                            "only 63 CPUs in NUMA mode supported.\n");
                    }
                    value = (1 << (endvalue + 1)) - (1 << value);
                } else {
                    value = 1 << value;
                }
            }
            node_cpumask[nodenr] = value;
        }
        nb_numa_nodes++;
    }
    return;
}

/***********************************************************/
/* USB devices */

static USBPort *used_usb_ports;
static USBPort *free_usb_ports;

/* ??? Maybe change this to register a hub to keep track of the topology.  */
void qemu_register_usb_port(USBPort *port, void *opaque, int index,
                            usb_attachfn attach)
{
    port->opaque = opaque;
    port->index = index;
    port->attach = attach;
    port->next = free_usb_ports;
    free_usb_ports = port;
}

int usb_device_add_dev(USBDevice *dev)
{
    USBPort *port;

    /* Find a USB port to add the device to.  */
    port = free_usb_ports;
    if (!port->next) {
        USBDevice *hub;

        /* Create a new hub and chain it on.  */
        free_usb_ports = NULL;
        port->next = used_usb_ports;
        used_usb_ports = port;

        hub = usb_hub_init(VM_USB_HUB_SIZE);
        usb_attach(port, hub);
        port = free_usb_ports;
    }

    free_usb_ports = port->next;
    port->next = used_usb_ports;
    used_usb_ports = port;
    usb_attach(port, dev);
    return 0;
}

#if 0
static void usb_msd_password_cb(void *opaque, int err)
{
    USBDevice *dev = opaque;

    if (!err)
        usb_device_add_dev(dev);
    else
        dev->handle_destroy(dev);
}
#endif

static int usb_device_add(const char *devname, int is_hotplug)
{
    const char *p;
    USBDevice *dev;

    if (!free_usb_ports)
        return -1;

    if (strstart(devname, "host:", &p)) {
        dev = usb_host_device_open(p);
    } else if (!strcmp(devname, "mouse")) {
        dev = usb_mouse_init();
    } else if (!strcmp(devname, "tablet")) {
        dev = usb_tablet_init();
    } else if (!strcmp(devname, "keyboard")) {
        dev = usb_keyboard_init();
    } else if (strstart(devname, "disk:", &p)) {
#if 0
        BlockDriverState *bs;
#endif
        dev = usb_msd_init(p);
        if (!dev)
            return -1;
#if 0
        bs = usb_msd_get_bdrv(dev);
        if (bdrv_key_required(bs)) {
            autostart = 0;
            if (is_hotplug) {
                monitor_read_bdrv_key_start(cur_mon, bs, usb_msd_password_cb,
                                            dev);
                return 0;
            }
        }
    } else if (!strcmp(devname, "wacom-tablet")) {
        dev = usb_wacom_init();
    } else if (strstart(devname, "serial:", &p)) {
        dev = usb_serial_init(p);
#ifdef CONFIG_BRLAPI
    } else if (!strcmp(devname, "braille")) {
        dev = usb_baum_init();
#endif
    } else if (strstart(devname, "net:", &p)) {
        int nic = nb_nics;

        if (net_client_init("nic", p) < 0)
            return -1;
        nd_table[nic].model = "usb";
        dev = usb_net_init(&nd_table[nic]);
    } else if (!strcmp(devname, "bt") || strstart(devname, "bt:", &p)) {
        dev = usb_bt_init(devname[2] ? hci_init(p) :
                        bt_new_hci(qemu_find_bt_vlan(0)));
#endif
    } else {
        return -1;
    }
    if (!dev)
        return -1;

    return usb_device_add_dev(dev);
}

int usb_device_del_addr(int bus_num, int addr)
{
    USBPort *port;
    USBPort **lastp;
    USBDevice *dev;

    if (!used_usb_ports)
        return -1;

    if (bus_num != 0)
        return -1;

    lastp = &used_usb_ports;
    port = used_usb_ports;
    while (port && port->dev->addr != addr) {
        lastp = &port->next;
        port = port->next;
    }

    if (!port)
        return -1;

    dev = port->dev;
    *lastp = port->next;
    usb_attach(port, NULL);
    dev->handle_destroy(dev);
    port->next = free_usb_ports;
    free_usb_ports = port;
    return 0;
}

static int usb_device_del(const char *devname)
{
    int bus_num, addr;
    const char *p;

    if (strstart(devname, "host:", &p))
        return usb_host_device_close(p);

    if (!used_usb_ports)
        return -1;

    p = strchr(devname, '.');
    if (!p)
        return -1;
    bus_num = strtoul(devname, NULL, 0);
    addr = strtoul(p + 1, NULL, 0);

    return usb_device_del_addr(bus_num, addr);
}

void do_usb_add(Monitor *mon, const char *devname)
{
    usb_device_add(devname, 1);
}

void do_usb_del(Monitor *mon, const char *devname)
{
    usb_device_del(devname);
}

void usb_info(Monitor *mon)
{
    USBDevice *dev;
    USBPort *port;
    const char *speed_str;

    if (!usb_enabled) {
        monitor_printf(mon, "USB support not enabled\n");
        return;
    }

    for (port = used_usb_ports; port; port = port->next) {
        dev = port->dev;
        if (!dev)
            continue;
        switch(dev->speed) {
        case USB_SPEED_LOW:
            speed_str = "1.5";
            break;
        case USB_SPEED_FULL:
            speed_str = "12";
            break;
        case USB_SPEED_HIGH:
            speed_str = "480";
            break;
        default:
            speed_str = "?";
            break;
        }
        monitor_printf(mon, "  Device %d.%d, Speed %s Mb/s, Product %s\n",
                       0, dev->addr, speed_str, dev->devname);
    }
}

/***********************************************************/
/* PCMCIA/Cardbus */

static struct pcmcia_socket_entry_s {
    PCMCIASocket *socket;
    struct pcmcia_socket_entry_s *next;
} *pcmcia_sockets = 0;

void pcmcia_socket_register(PCMCIASocket *socket)
{
    struct pcmcia_socket_entry_s *entry;

    entry = qemu_malloc(sizeof(struct pcmcia_socket_entry_s));
    entry->socket = socket;
    entry->next = pcmcia_sockets;
    pcmcia_sockets = entry;
}

void pcmcia_socket_unregister(PCMCIASocket *socket)
{
    struct pcmcia_socket_entry_s *entry, **ptr;

    ptr = &pcmcia_sockets;
    for (entry = *ptr; entry; ptr = &entry->next, entry = *ptr)
        if (entry->socket == socket) {
            *ptr = entry->next;
            qemu_free(entry);
        }
}

void pcmcia_info(Monitor *mon)
{
    struct pcmcia_socket_entry_s *iter;

    if (!pcmcia_sockets)
        monitor_printf(mon, "No PCMCIA sockets\n");

    for (iter = pcmcia_sockets; iter; iter = iter->next)
        monitor_printf(mon, "%s: %s\n", iter->socket->slot_string,
                       iter->socket->attached ? iter->socket->card_string :
                       "Empty");
}

/***********************************************************/
/* I/O handling */

typedef struct IOHandlerRecord {
    int fd;
    IOCanReadHandler *fd_read_poll;
    IOHandler *fd_read;
    IOHandler *fd_write;
    int deleted;
    void *opaque;
    /* temporary data */
    struct pollfd *ufd;
    struct IOHandlerRecord *next;
} IOHandlerRecord;

static IOHandlerRecord *first_io_handler;

/* XXX: fd_read_poll should be suppressed, but an API change is
   necessary in the character devices to suppress fd_can_read(). */
int qemu_set_fd_handler2(int fd,
                         IOCanReadHandler *fd_read_poll,
                         IOHandler *fd_read,
                         IOHandler *fd_write,
                         void *opaque)
{
    IOHandlerRecord **pioh, *ioh;

    if (!fd_read && !fd_write) {
        pioh = &first_io_handler;
        for(;;) {
            ioh = *pioh;
            if (ioh == NULL)
                break;
            if (ioh->fd == fd) {
                ioh->deleted = 1;
                break;
            }
            pioh = &ioh->next;
        }
    } else {
        for(ioh = first_io_handler; ioh != NULL; ioh = ioh->next) {
            if (ioh->fd == fd)
                goto found;
        }
        ioh = qemu_mallocz(sizeof(IOHandlerRecord));
        ioh->next = first_io_handler;
        first_io_handler = ioh;
    found:
        ioh->fd = fd;
        ioh->fd_read_poll = fd_read_poll;
        ioh->fd_read = fd_read;
        ioh->fd_write = fd_write;
        ioh->opaque = opaque;
        ioh->deleted = 0;
    }
    return 0;
}

int qemu_set_fd_handler(int fd,
                        IOHandler *fd_read,
                        IOHandler *fd_write,
                        void *opaque)
{
    return qemu_set_fd_handler2(fd, NULL, fd_read, fd_write, opaque);
}

#ifdef _WIN32
/***********************************************************/
/* Polling handling */

typedef struct PollingEntry {
    PollingFunc *func;
    void *opaque;
    struct PollingEntry *next;
} PollingEntry;

static PollingEntry *first_polling_entry;

int qemu_add_polling_cb(PollingFunc *func, void *opaque)
{
    PollingEntry **ppe, *pe;
    pe = qemu_mallocz(sizeof(PollingEntry));
    pe->func = func;
    pe->opaque = opaque;
    for(ppe = &first_polling_entry; *ppe != NULL; ppe = &(*ppe)->next);
    *ppe = pe;
    return 0;
}

void qemu_del_polling_cb(PollingFunc *func, void *opaque)
{
    PollingEntry **ppe, *pe;
    for(ppe = &first_polling_entry; *ppe != NULL; ppe = &(*ppe)->next) {
        pe = *ppe;
        if (pe->func == func && pe->opaque == opaque) {
            *ppe = pe->next;
            qemu_free(pe);
            break;
        }
    }
}

/***********************************************************/
/* Wait objects support */
typedef struct WaitObjects {
    int num;
    HANDLE events[MAXIMUM_WAIT_OBJECTS + 1];
    WaitObjectFunc *func[MAXIMUM_WAIT_OBJECTS + 1];
    void *opaque[MAXIMUM_WAIT_OBJECTS + 1];
} WaitObjects;

static WaitObjects wait_objects = {0};

int qemu_add_wait_object(HANDLE handle, WaitObjectFunc *func, void *opaque)
{
    WaitObjects *w = &wait_objects;

    if (w->num >= MAXIMUM_WAIT_OBJECTS)
        return -1;
    w->events[w->num] = handle;
    w->func[w->num] = func;
    w->opaque[w->num] = opaque;
    w->num++;
    return 0;
}

void qemu_del_wait_object(HANDLE handle, WaitObjectFunc *func, void *opaque)
{
    int i, found;
    WaitObjects *w = &wait_objects;

    found = 0;
    for (i = 0; i < w->num; i++) {
        if (w->events[i] == handle)
            found = 1;
        if (found) {
            w->events[i] = w->events[i + 1];
            w->func[i] = w->func[i + 1];
            w->opaque[i] = w->opaque[i + 1];
        }
    }
    if (found)
        w->num--;
}
#endif

/***********************************************************/
/* ram save/restore */

static int ram_get_page(QEMUFile *f, uint8_t *buf, int len)
{
    int v;

    v = qemu_get_byte(f);
    switch(v) {
    case 0:
        if (qemu_get_buffer(f, buf, len) != len)
            return -EIO;
        break;
    case 1:
        v = qemu_get_byte(f);
        memset(buf, v, len);
        break;
    default:
        return -EINVAL;
    }

    if (qemu_file_has_error(f))
        return -EIO;

    return 0;
}

static int ram_load_v1(QEMUFile *f, void *opaque)
{
    int ret;
    ram_addr_t i;

    if (qemu_get_be32(f) != last_ram_offset)
        return -EINVAL;
    for(i = 0; i < last_ram_offset; i+= TARGET_PAGE_SIZE) {
        ret = ram_get_page(f, qemu_get_ram_ptr(i), TARGET_PAGE_SIZE);
        if (ret)
            return ret;
    }
    return 0;
}

#define BDRV_HASH_BLOCK_SIZE 1024
#define IOBUF_SIZE 4096
#define RAM_CBLOCK_MAGIC 0xfabe

typedef struct RamDecompressState {
    z_stream zstream;
    QEMUFile *f;
    uint8_t buf[IOBUF_SIZE];
} RamDecompressState;

static int ram_decompress_open(RamDecompressState *s, QEMUFile *f)
{
    int ret;
    memset(s, 0, sizeof(*s));
    s->f = f;
    ret = inflateInit(&s->zstream);
    if (ret != Z_OK)
        return -1;
    return 0;
}

static int ram_decompress_buf(RamDecompressState *s, uint8_t *buf, int len)
{
    int ret, clen;

    s->zstream.avail_out = len;
    s->zstream.next_out = buf;
    while (s->zstream.avail_out > 0) {
        if (s->zstream.avail_in == 0) {
            if (qemu_get_be16(s->f) != RAM_CBLOCK_MAGIC)
                return -1;
            clen = qemu_get_be16(s->f);
            if (clen > IOBUF_SIZE)
                return -1;
            qemu_get_buffer(s->f, s->buf, clen);
            s->zstream.avail_in = clen;
            s->zstream.next_in = s->buf;
        }
        ret = inflate(&s->zstream, Z_PARTIAL_FLUSH);
        if (ret != Z_OK && ret != Z_STREAM_END) {
            return -1;
        }
    }
    return 0;
}

static void ram_decompress_close(RamDecompressState *s)
{
    inflateEnd(&s->zstream);
}

#define RAM_SAVE_FLAG_FULL	0x01
#define RAM_SAVE_FLAG_COMPRESS	0x02
#define RAM_SAVE_FLAG_MEM_SIZE	0x04
#define RAM_SAVE_FLAG_PAGE	0x08
#define RAM_SAVE_FLAG_EOS	0x10

static int is_dup_page(uint8_t *page, uint8_t ch)
{
    uint32_t val = ch << 24 | ch << 16 | ch << 8 | ch;
    uint32_t *array = (uint32_t *)page;
    int i;

    for (i = 0; i < (TARGET_PAGE_SIZE / 4); i++) {
        if (array[i] != val)
            return 0;
    }

    return 1;
}

static int ram_save_block(QEMUFile *f)
{
    static ram_addr_t current_addr = 0;
    ram_addr_t saved_addr = current_addr;
    ram_addr_t addr = 0;
    int found = 0;

    while (addr < last_ram_offset) {
        if (cpu_physical_memory_get_dirty(current_addr, MIGRATION_DIRTY_FLAG)) {
            uint8_t *p;

            cpu_physical_memory_reset_dirty(current_addr,
                                            current_addr + TARGET_PAGE_SIZE,
                                            MIGRATION_DIRTY_FLAG);

            p = qemu_get_ram_ptr(current_addr);

#ifdef CONFIG_S2E
            uint8_t buf[TARGET_PAGE_SIZE];
            s2e_read_ram_concrete(g_s2e, g_s2e_state, (uint64_t) p,
                                  buf, TARGET_PAGE_SIZE);
            p = buf;
#endif

            if (is_dup_page(p, *p)) {
                qemu_put_be64(f, current_addr | RAM_SAVE_FLAG_COMPRESS);
                qemu_put_byte(f, *p);
            } else {
                qemu_put_be64(f, current_addr | RAM_SAVE_FLAG_PAGE);
                qemu_put_buffer(f, p, TARGET_PAGE_SIZE);
            }

            found = 1;
            break;
        }
        addr += TARGET_PAGE_SIZE;
        current_addr = (saved_addr + addr) % last_ram_offset;
    }

    return found;
}

static uint64_t bytes_transferred = 0;

static ram_addr_t ram_save_remaining(void)
{
    ram_addr_t addr;
    ram_addr_t count = 0;

    for (addr = 0; addr < last_ram_offset; addr += TARGET_PAGE_SIZE) {
        if (cpu_physical_memory_get_dirty(addr, MIGRATION_DIRTY_FLAG))
            count++;
    }

    return count;
}

uint64_t ram_bytes_remaining(void)
{
    return ram_save_remaining() * TARGET_PAGE_SIZE;
}

uint64_t ram_bytes_transferred(void)
{
    return bytes_transferred;
}

uint64_t ram_bytes_total(void)
{
    return last_ram_offset;
}

static int ram_save_live(QEMUFile *f, int stage, void *opaque)
{
    ram_addr_t addr;
    uint64_t bytes_transferred_last;
    double bwidth = 0;
    uint64_t expected_time = 0;

    cpu_physical_sync_dirty_bitmap(0, TARGET_PHYS_ADDR_MAX);

    if (stage == 1) {
        /* Make sure all dirty bits are set */
        for (addr = 0; addr < last_ram_offset; addr += TARGET_PAGE_SIZE) {
            if (!cpu_physical_memory_get_dirty(addr, MIGRATION_DIRTY_FLAG))
                cpu_physical_memory_set_dirty(addr);
        }

        /* Enable dirty memory tracking */
        cpu_physical_memory_set_dirty_tracking(1);

        qemu_put_be64(f, last_ram_offset | RAM_SAVE_FLAG_MEM_SIZE);
    }

    bytes_transferred_last = bytes_transferred;
    bwidth = qemu_get_clock_ns(rt_clock);

    while (!qemu_file_rate_limit(f)) {
        int ret;

        ret = ram_save_block(f);
        bytes_transferred += ret * TARGET_PAGE_SIZE;
        if (ret == 0) /* no more blocks */
            break;
    }

    bwidth = qemu_get_clock_ns(rt_clock) - bwidth;
    bwidth = (bytes_transferred - bytes_transferred_last) / bwidth;

    /* if we haven't transferred anything this round, force expected_time to a
     * a very high value, but without crashing */
    if (bwidth == 0)
        bwidth = 0.000001;

    /* try transferring iterative blocks of memory */

    if (stage == 3) {

        /* flush all remaining blocks regardless of rate limiting */
        while (ram_save_block(f) != 0) {
            bytes_transferred += TARGET_PAGE_SIZE;
        }
        cpu_physical_memory_set_dirty_tracking(0);
    }

    qemu_put_be64(f, RAM_SAVE_FLAG_EOS);

    expected_time = ram_save_remaining() * TARGET_PAGE_SIZE / bwidth;

    return (stage == 2) && (expected_time <= migrate_max_downtime());
}

static int ram_load_dead(QEMUFile *f, void *opaque)
{
    RamDecompressState s1, *s = &s1;
    uint8_t buf[10];
    ram_addr_t i;

    if (ram_decompress_open(s, f) < 0)
        return -EINVAL;
    for(i = 0; i < last_ram_offset; i+= BDRV_HASH_BLOCK_SIZE) {
        if (ram_decompress_buf(s, buf, 1) < 0) {
            fprintf(stderr, "Error while reading ram block header\n");
            goto error;
        }
        if (buf[0] == 0) {
            if (ram_decompress_buf(s, qemu_get_ram_ptr(i),
                                   BDRV_HASH_BLOCK_SIZE) < 0) {
                fprintf(stderr, "Error while reading ram block address=0x%08" PRIx64, (uint64_t)i);
                goto error;
            }
        } else {
        error:
            printf("Error block header\n");
            return -EINVAL;
        }
    }
    ram_decompress_close(s);

    return 0;
}

static int ram_load(QEMUFile *f, void *opaque, int version_id)
{
    ram_addr_t addr;
    int flags;

    if (version_id == 1)
        return ram_load_v1(f, opaque);

    if (version_id == 2) {
        if (qemu_get_be32(f) != last_ram_offset)
            return -EINVAL;
        return ram_load_dead(f, opaque);
    }

    if (version_id != 3)
        return -EINVAL;

    do {
        addr = qemu_get_be64(f);

        flags = addr & ~TARGET_PAGE_MASK;
        addr &= TARGET_PAGE_MASK;

        if (flags & RAM_SAVE_FLAG_MEM_SIZE) {
            if (addr != last_ram_offset)
                return -EINVAL;
        }

        if (flags & RAM_SAVE_FLAG_FULL) {
            if (ram_load_dead(f, opaque) < 0)
                return -EINVAL;
        }

        if (flags & RAM_SAVE_FLAG_COMPRESS) {
            uint8_t ch = qemu_get_byte(f);
#ifndef CONFIG_S2E
            memset(qemu_get_ram_ptr(addr), ch, TARGET_PAGE_SIZE);
#else
            int i;
            uint64_t vaddr = (uint64_t) qemu_get_ram_ptr(addr);
            for(i = 0; i < TARGET_PAGE_SIZE; i++)
                s2e_write_ram_concrete(g_s2e, g_s2e_state, vaddr+i, &ch, 1);
#endif
        } else if (flags & RAM_SAVE_FLAG_PAGE)
#ifndef CONFIG_S2E
            qemu_get_buffer(f, qemu_get_ram_ptr(addr), TARGET_PAGE_SIZE);
#else
            uint8_t buf[TARGET_PAGE_SIZE];
            qemu_get_buffer(f, buf, TARGET_PAGE_SIZE);

            s2e_write_ram_concrete(g_s2e, g_s2e_state,
                                   (uint64_t) qemu_get_ram_ptr(addr),
                                   buf, TARGET_PAGE_SIZE);
#endif
    } while (!(flags & RAM_SAVE_FLAG_EOS));

    return 0;
}

void qemu_service_io(void)
{
    qemu_notify_event();
}

/***********************************************************/
/* machine registration */

static QEMUMachine *first_machine = NULL;
QEMUMachine *current_machine = NULL;

int qemu_register_machine(QEMUMachine *m)
{
    QEMUMachine **pm;
    pm = &first_machine;
    while (*pm != NULL)
        pm = &(*pm)->next;
    m->next = NULL;
    *pm = m;
    return 0;
}

static QEMUMachine *find_machine(const char *name)
{
    QEMUMachine *m;

    for(m = first_machine; m != NULL; m = m->next) {
        if (!strcmp(m->name, name))
            return m;
    }
    return NULL;
}

static QEMUMachine *find_default_machine(void)
{
    QEMUMachine *m;

    for(m = first_machine; m != NULL; m = m->next) {
        if (m->is_default) {
            return m;
        }
    }
    return NULL;
}

/***********************************************************/
/* main execution loop */

static void gui_update(void *opaque)
{
    uint64_t interval = GUI_REFRESH_INTERVAL;
    DisplayState *ds = opaque;
    DisplayChangeListener *dcl = ds->listeners;

    dpy_refresh(ds);

    while (dcl != NULL) {
        if (dcl->gui_timer_interval &&
            dcl->gui_timer_interval < interval)
            interval = dcl->gui_timer_interval;
        dcl = dcl->next;
    }
    qemu_mod_timer(ds->gui_timer, interval + qemu_get_clock(rt_clock));
}

static void nographic_update(void *opaque)
{
    uint64_t interval = GUI_REFRESH_INTERVAL;

    qemu_mod_timer(nographic_timer, interval + qemu_get_clock(rt_clock));
}

struct vm_change_state_entry {
    VMChangeStateHandler *cb;
    void *opaque;
    QLIST_ENTRY (vm_change_state_entry) entries;
};

static QLIST_HEAD(vm_change_state_head, vm_change_state_entry) vm_change_state_head;

VMChangeStateEntry *qemu_add_vm_change_state_handler(VMChangeStateHandler *cb,
                                                     void *opaque)
{
    VMChangeStateEntry *e;

    e = qemu_mallocz(sizeof (*e));

    e->cb = cb;
    e->opaque = opaque;
    QLIST_INSERT_HEAD(&vm_change_state_head, e, entries);
    return e;
}

void qemu_del_vm_change_state_handler(VMChangeStateEntry *e)
{
    QLIST_REMOVE (e, entries);
    qemu_free (e);
}

static void vm_state_notify(int running, int reason)
{
    VMChangeStateEntry *e;

    for (e = vm_change_state_head.lh_first; e; e = e->entries.le_next) {
        e->cb(e->opaque, running, reason);
    }
}

static void resume_all_vcpus(void);
static void pause_all_vcpus(void);

void vm_start(void)
{
    if (!vm_running) {
        cpu_enable_ticks();
        vm_running = 1;
        vm_state_notify(1, 0);
        //qemu_rearm_alarm_timer(alarm_timer);
        resume_all_vcpus();
    }
}

/* reset/shutdown handler */

typedef struct QEMUResetEntry {
    QEMUResetHandler *func;
    void *opaque;
    int order;
    struct QEMUResetEntry *next;
} QEMUResetEntry;

static QEMUResetEntry *first_reset_entry;
static int reset_requested;
static int shutdown_requested;
static int powerdown_requested;
static int debug_requested;
static int vmstop_requested;

int qemu_shutdown_requested(void)
{
    int r = shutdown_requested;
    shutdown_requested = 0;
    return r;
}

int qemu_reset_requested(void)
{
    int r = reset_requested;
    reset_requested = 0;
    return r;
}

int qemu_powerdown_requested(void)
{
    int r = powerdown_requested;
    powerdown_requested = 0;
    return r;
}

static int qemu_debug_requested(void)
{
    int r = debug_requested;
    debug_requested = 0;
    return r;
}

static int qemu_vmstop_requested(void)
{
    int r = vmstop_requested;
    vmstop_requested = 0;
    return r;
}

static void do_vm_stop(int reason)
{
    if (vm_running) {
        cpu_disable_ticks();
        vm_running = 0;
        pause_all_vcpus();
        vm_state_notify(0, reason);
    }
}

void qemu_register_reset(QEMUResetHandler *func, int order, void *opaque)
{
    QEMUResetEntry **pre, *re;

    pre = &first_reset_entry;
    while (*pre != NULL && (*pre)->order >= order) {
        pre = &(*pre)->next;
    }
    re = qemu_mallocz(sizeof(QEMUResetEntry));
    re->func = func;
    re->opaque = opaque;
    re->order = order;
    re->next = NULL;
    *pre = re;
}

void qemu_system_reset(void)
{
    QEMUResetEntry *re;

    /* reset all devices */
    for(re = first_reset_entry; re != NULL; re = re->next) {
        re->func(re->opaque);
    }
}

void qemu_system_reset_request(void)
{
    if (no_reboot) {
        shutdown_requested = 1;
    } else {
        reset_requested = 1;
    }
    qemu_notify_event();
}

void qemu_system_shutdown_request(void)
{
    shutdown_requested = 1;
    qemu_notify_event();
}

void qemu_system_powerdown_request(void)
{
    powerdown_requested = 1;
    qemu_notify_event();
}

#ifdef CONFIG_IOTHREAD
static void qemu_system_vmstop_request(int reason)
{
    vmstop_requested = reason;
    qemu_notify_event();
}
#endif

#ifndef _WIN32
static int io_thread_fd = -1;

#if 0
static void qemu_event_increment(void)
{
    static const char byte = 0;

    if (io_thread_fd == -1)
        return;

    write(io_thread_fd, &byte, sizeof(byte));
}
#endif

static void qemu_event_read(void *opaque)
{
    int fd = (unsigned long)opaque;
    ssize_t len;

    /* Drain the notify pipe */
    do {
        char buffer[512];
        len = read(fd, buffer, sizeof(buffer));
    } while ((len == -1 && errno == EINTR) || len > 0);
}

static int qemu_event_init(void)
{
    int err;
    int fds[2];

    err = pipe(fds);
    if (err == -1)
        return -errno;

    err = fcntl_setfl(fds[0], O_NONBLOCK);
    if (err < 0)
        goto fail;

    err = fcntl_setfl(fds[1], O_NONBLOCK);
    if (err < 0)
        goto fail;

    qemu_set_fd_handler2(fds[0], NULL, qemu_event_read, NULL,
                         (void *)(unsigned long)fds[0]);

    io_thread_fd = fds[1];
    return 0;

fail:
    close(fds[0]);
    close(fds[1]);
    return err;
}
#else
HANDLE qemu_event_handle;

static void dummy_event_handler(void *opaque)
{
}

static int qemu_event_init(void)
{
    qemu_event_handle = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (!qemu_event_handle) {
        perror("Failed CreateEvent");
        return -1;
    }
    qemu_add_wait_object(qemu_event_handle, dummy_event_handler, NULL);
    return 0;
}

#if 0
static void qemu_event_increment(void)
{
    SetEvent(qemu_event_handle);
}
#endif
#endif

static int cpu_can_run(CPUState *env)
{
    if (env->stop)
        return 0;
    if (env->stopped)
        return 0;
    return 1;
}

#ifndef CONFIG_IOTHREAD
static int qemu_init_main_loop(void)
{
    return qemu_event_init();
}

void qemu_init_vcpu(void *_env)
{
    CPUState *env = _env;

    if (kvm_enabled())
        kvm_init_vcpu(env);
    return;
}

int qemu_cpu_self(void *env)
{
    return 1;
}

static void resume_all_vcpus(void)
{
}

static void pause_all_vcpus(void)
{
}

void qemu_cpu_kick(void *env)
{
    return;
}

void qemu_notify_event(void)
{
    CPUState *env = cpu_single_env;

    if (env) {
        cpu_exit(env);
#ifdef USE_KQEMU
        if (env->kqemu_enabled)
            kqemu_cpu_interrupt(env);
#endif
     }
}

#define qemu_mutex_lock_iothread() do { } while (0)
#define qemu_mutex_unlock_iothread() do { } while (0)

void vm_stop(int reason)
{
    do_vm_stop(reason);
}

#else /* CONFIG_IOTHREAD */

#include "qemu-thread.h"

QemuMutex qemu_global_mutex;
static QemuMutex qemu_fair_mutex;

static QemuThread io_thread;

static QemuThread *tcg_cpu_thread;
static QemuCond *tcg_halt_cond;

static int qemu_system_ready;
/* cpu creation */
static QemuCond qemu_cpu_cond;
/* system init */
static QemuCond qemu_system_cond;
static QemuCond qemu_pause_cond;

static void block_io_signals(void);
static void unblock_io_signals(void);
static int tcg_has_work(void);

static int qemu_init_main_loop(void)
{
    int ret;

    ret = qemu_event_init();
    if (ret)
        return ret;

    qemu_cond_init(&qemu_pause_cond);
    qemu_mutex_init(&qemu_fair_mutex);
    qemu_mutex_init(&qemu_global_mutex);
    qemu_mutex_lock(&qemu_global_mutex);

    unblock_io_signals();
    qemu_thread_self(&io_thread);

    return 0;
}

static void qemu_wait_io_event(CPUState *env)
{
    while (!tcg_has_work())
        qemu_cond_timedwait(env->halt_cond, &qemu_global_mutex, 1000);

    qemu_mutex_unlock(&qemu_global_mutex);

    /*
     * Users of qemu_global_mutex can be starved, having no chance
     * to acquire it since this path will get to it first.
     * So use another lock to provide fairness.
     */
    qemu_mutex_lock(&qemu_fair_mutex);
    qemu_mutex_unlock(&qemu_fair_mutex);

    qemu_mutex_lock(&qemu_global_mutex);
    if (env->stop) {
        env->stop = 0;
        env->stopped = 1;
        qemu_cond_signal(&qemu_pause_cond);
    }
}

static int qemu_cpu_exec(CPUState *env);

static void *kvm_cpu_thread_fn(void *arg)
{
    CPUState *env = arg;

    block_io_signals();
    qemu_thread_self(env->thread);

    /* signal CPU creation */
    qemu_mutex_lock(&qemu_global_mutex);
    env->created = 1;
    qemu_cond_signal(&qemu_cpu_cond);

    /* and wait for machine initialization */
    while (!qemu_system_ready)
        qemu_cond_timedwait(&qemu_system_cond, &qemu_global_mutex, 100);

    while (1) {
        if (cpu_can_run(env))
            qemu_cpu_exec(env);
        qemu_wait_io_event(env);
    }

    return NULL;
}

static void tcg_cpu_exec(void);

static void *tcg_cpu_thread_fn(void *arg)
{
    CPUState *env = arg;

    block_io_signals();
    qemu_thread_self(env->thread);

    /* signal CPU creation */
    qemu_mutex_lock(&qemu_global_mutex);
    for (env = first_cpu; env != NULL; env = env->next_cpu)
        env->created = 1;
    qemu_cond_signal(&qemu_cpu_cond);

    /* and wait for machine initialization */
    while (!qemu_system_ready)
        qemu_cond_timedwait(&qemu_system_cond, &qemu_global_mutex, 100);

    while (1) {
        tcg_cpu_exec();
        qemu_wait_io_event(cur_cpu);
    }

    return NULL;
}

void qemu_cpu_kick(void *_env)
{
    CPUState *env = _env;
    qemu_cond_broadcast(env->halt_cond);
    if (kvm_enabled())
        qemu_thread_signal(env->thread, SIGUSR1);
}

int qemu_cpu_self(void *env)
{
    return (cpu_single_env != NULL);
}

static void cpu_signal(int sig)
{
    if (cpu_single_env)
        cpu_exit(cpu_single_env);
}

static void block_io_signals(void)
{
    sigset_t set;
    struct sigaction sigact;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);
    sigaddset(&set, SIGIO);
    sigaddset(&set, SIGALRM);
    pthread_sigmask(SIG_BLOCK, &set, NULL);

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);

    memset(&sigact, 0, sizeof(sigact));
    sigact.sa_handler = cpu_signal;
    sigaction(SIGUSR1, &sigact, NULL);
}

static void unblock_io_signals(void)
{
    sigset_t set;

    sigemptyset(&set);
    sigaddset(&set, SIGUSR2);
    sigaddset(&set, SIGIO);
    sigaddset(&set, SIGALRM);
    pthread_sigmask(SIG_UNBLOCK, &set, NULL);

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    pthread_sigmask(SIG_BLOCK, &set, NULL);
}

static void qemu_signal_lock(unsigned int msecs)
{
    qemu_mutex_lock(&qemu_fair_mutex);

    while (qemu_mutex_trylock(&qemu_global_mutex)) {
        qemu_thread_signal(tcg_cpu_thread, SIGUSR1);
        if (!qemu_mutex_timedlock(&qemu_global_mutex, msecs))
            break;
    }
    qemu_mutex_unlock(&qemu_fair_mutex);
}

void qemu_mutex_lock_iothread(void)
{
    if (kvm_enabled()) {
        qemu_mutex_lock(&qemu_fair_mutex);
        qemu_mutex_lock(&qemu_global_mutex);
        qemu_mutex_unlock(&qemu_fair_mutex);
    } else
        qemu_signal_lock(100);
}

void qemu_mutex_unlock_iothread(void)
{
    qemu_mutex_unlock(&qemu_global_mutex);
}

static int all_vcpus_paused(void)
{
    CPUState *penv = first_cpu;

    while (penv) {
        if (!penv->stopped)
            return 0;
        penv = (CPUState *)penv->next_cpu;
    }

    return 1;
}

static void pause_all_vcpus(void)
{
    CPUState *penv = first_cpu;

    while (penv) {
        penv->stop = 1;
        qemu_thread_signal(penv->thread, SIGUSR1);
        qemu_cpu_kick(penv);
        penv = (CPUState *)penv->next_cpu;
    }

    while (!all_vcpus_paused()) {
        qemu_cond_timedwait(&qemu_pause_cond, &qemu_global_mutex, 100);
        penv = first_cpu;
        while (penv) {
            qemu_thread_signal(penv->thread, SIGUSR1);
            penv = (CPUState *)penv->next_cpu;
        }
    }
}

static void resume_all_vcpus(void)
{
    CPUState *penv = first_cpu;

    while (penv) {
        penv->stop = 0;
        penv->stopped = 0;
        qemu_thread_signal(penv->thread, SIGUSR1);
        qemu_cpu_kick(penv);
        penv = (CPUState *)penv->next_cpu;
    }
}

static void tcg_init_vcpu(void *_env)
{
    CPUState *env = _env;
    /* share a single thread for all cpus with TCG */
    if (!tcg_cpu_thread) {
        env->thread = qemu_mallocz(sizeof(QemuThread));
        env->halt_cond = qemu_mallocz(sizeof(QemuCond));
        qemu_cond_init(env->halt_cond);
        qemu_thread_create(env->thread, tcg_cpu_thread_fn, env);
        while (env->created == 0)
            qemu_cond_timedwait(&qemu_cpu_cond, &qemu_global_mutex, 100);
        tcg_cpu_thread = env->thread;
        tcg_halt_cond = env->halt_cond;
    } else {
        env->thread = tcg_cpu_thread;
        env->halt_cond = tcg_halt_cond;
    }
}

static void kvm_start_vcpu(CPUState *env)
{
#if 0
    kvm_init_vcpu(env);
    env->thread = qemu_mallocz(sizeof(QemuThread));
    env->halt_cond = qemu_mallocz(sizeof(QemuCond));
    qemu_cond_init(env->halt_cond);
    qemu_thread_create(env->thread, kvm_cpu_thread_fn, env);
    while (env->created == 0)
        qemu_cond_timedwait(&qemu_cpu_cond, &qemu_global_mutex, 100);
#endif
}

void qemu_init_vcpu(void *_env)
{
    CPUState *env = _env;

    if (kvm_enabled())
        kvm_start_vcpu(env);
    else
        tcg_init_vcpu(env);
}

void qemu_notify_event(void)
{
    qemu_event_increment();
}

void vm_stop(int reason)
{
    QemuThread me;
    qemu_thread_self(&me);

    if (!qemu_thread_equal(&me, &io_thread)) {
        qemu_system_vmstop_request(reason);
        /*
         * FIXME: should not return to device code in case
         * vm_stop() has been requested.
         */
        if (cpu_single_env) {
            cpu_exit(cpu_single_env);
            cpu_single_env->stop = 1;
        }
        return;
    }
    do_vm_stop(reason);
}

#endif


#ifdef _WIN32
static void host_main_loop_wait(int *timeout)
{
    int ret, ret2, i;
    PollingEntry *pe;


    /* XXX: need to suppress polling by better using win32 events */
    ret = 0;
    for(pe = first_polling_entry; pe != NULL; pe = pe->next) {
        ret |= pe->func(pe->opaque);
    }
    if (ret == 0) {
        int err;
        WaitObjects *w = &wait_objects;

        ret = WaitForMultipleObjects(w->num, w->events, FALSE, *timeout);
        if (WAIT_OBJECT_0 + 0 <= ret && ret <= WAIT_OBJECT_0 + w->num - 1) {
            if (w->func[ret - WAIT_OBJECT_0])
                w->func[ret - WAIT_OBJECT_0](w->opaque[ret - WAIT_OBJECT_0]);

            /* Check for additional signaled events */
            for(i = (ret - WAIT_OBJECT_0 + 1); i < w->num; i++) {

                /* Check if event is signaled */
                ret2 = WaitForSingleObject(w->events[i], 0);
                if(ret2 == WAIT_OBJECT_0) {
                    if (w->func[i])
                        w->func[i](w->opaque[i]);
                } else if (ret2 == WAIT_TIMEOUT) {
                } else {
                    err = GetLastError();
                    fprintf(stderr, "WaitForSingleObject error %d %d\n", i, err);
                }
            }
        } else if (ret == WAIT_TIMEOUT) {
        } else {
            err = GetLastError();
            fprintf(stderr, "WaitForMultipleObjects error %d %d\n", ret, err);
        }
    }

    *timeout = 0;
}
#else
static void host_main_loop_wait(int *timeout)
{
}
#endif

void main_loop_wait(int timeout)
{
    IOHandlerRecord *ioh;
    fd_set rfds, wfds, xfds;
    int ret, nfds;
    struct timeval tv;

    qemu_bh_update_timeout(&timeout);

    host_main_loop_wait(&timeout);

    /* poll any events */
    /* XXX: separate device handlers from system ones */
    nfds = -1;
    FD_ZERO(&rfds);
    FD_ZERO(&wfds);
    FD_ZERO(&xfds);
    for(ioh = first_io_handler; ioh != NULL; ioh = ioh->next) {
        if (ioh->deleted)
            continue;
        if (ioh->fd_read &&
            (!ioh->fd_read_poll ||
             ioh->fd_read_poll(ioh->opaque) != 0)) {
            FD_SET(ioh->fd, &rfds);
            if (ioh->fd > nfds)
                nfds = ioh->fd;
        }
        if (ioh->fd_write) {
            FD_SET(ioh->fd, &wfds);
            if (ioh->fd > nfds)
                nfds = ioh->fd;
        }
    }

    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

#if defined(CONFIG_SLIRP)
    if (slirp_is_inited()) {
        slirp_select_fill(&nfds, &rfds, &wfds, &xfds);
    }
#endif
    qemu_mutex_unlock_iothread();
    ret = select(nfds + 1, &rfds, &wfds, &xfds, &tv);
    qemu_mutex_lock_iothread();
    if (ret > 0) {
        IOHandlerRecord **pioh;

        for(ioh = first_io_handler; ioh != NULL; ioh = ioh->next) {
            if (!ioh->deleted && ioh->fd_read && FD_ISSET(ioh->fd, &rfds)) {
                ioh->fd_read(ioh->opaque);
            }
            if (!ioh->deleted && ioh->fd_write && FD_ISSET(ioh->fd, &wfds)) {
                ioh->fd_write(ioh->opaque);
            }
        }

        /* remove deleted IO handlers */
        pioh = &first_io_handler;
        while (*pioh) {
            ioh = *pioh;
            if (ioh->deleted) {
                *pioh = ioh->next;
                qemu_free(ioh);
            } else
                pioh = &ioh->next;
        }
    }
#if defined(CONFIG_SLIRP)
    if (slirp_is_inited()) {
        if (ret < 0) {
            FD_ZERO(&rfds);
            FD_ZERO(&wfds);
            FD_ZERO(&xfds);
        }
        slirp_select_poll(&rfds, &wfds, &xfds);
    }
#endif
    charpipe_poll();

    qemu_run_all_timers();

    /* Check bottom-halves last in case any of the earlier events triggered
       them.  */
    qemu_bh_poll();

}

static int qemu_cpu_exec(CPUState *env)
{
    int ret;
#ifdef CONFIG_PROFILER
    int64_t ti;
#endif

#ifdef CONFIG_PROFILER
    ti = profile_getclock();
#endif
    if (use_icount) {
#ifdef CONFIG_S2E
        assert(qemu_icount == env->s2e_icount);
#endif
        int64_t count;
        int decr;
        qemu_icount -= (env->icount_decr.u16.low + env->icount_extra);
        env->icount_decr.u16.low = 0;
        env->icount_extra = 0;
        count = qemu_next_deadline();
        count = (count + (1 << icount_time_shift) - 1)
                >> icount_time_shift;
        qemu_icount += count;
        decr = (count > 0xffff) ? 0xffff : count;
        count -= decr;
        env->icount_decr.u16.low = decr;
        env->icount_extra = count;
    }
#ifdef CONFIG_TRACE
    if (tbflush_requested) {
        tbflush_requested = 0;
        tb_flush(env);
        return EXCP_INTERRUPT;
    }
#endif


    ret = cpu_exec(env);
#ifdef CONFIG_PROFILER
    qemu_time += profile_getclock() - ti;
#endif
    if (use_icount) {
        /* Fold pending instructions back into the
           instruction counter, and clear the interrupt flag.  */
        qemu_icount -= (env->icount_decr.u16.low
                        + env->icount_extra);
        env->icount_decr.u32 = 0;
        env->icount_extra = 0;
#ifdef CONFIG_S2E
        assert(qemu_icount == env->s2e_icount);
#endif
    }
    return ret;
}

static void tcg_cpu_exec(void)
{
    int ret = 0;

    if (next_cpu == NULL)
        next_cpu = first_cpu;
    for (; next_cpu != NULL; next_cpu = next_cpu->next_cpu) {
        CPUState *env = cur_cpu = next_cpu;

        if (!vm_running)
            break;
        if (qemu_timer_alarm_pending()) {
            break;
        }
        if (cpu_can_run(env))
            ret = qemu_cpu_exec(env);
        if (ret == EXCP_DEBUG) {
            gdb_set_stop_cpu(env);
            debug_requested = 1;
            break;
        }
    }
}

static int cpu_has_work(CPUState *env)
{
    if (env->stop)
        return 1;
    if (env->stopped)
        return 0;
    if (!env->halted)
        return 1;
    if (qemu_cpu_has_work(env))
        return 1;
    return 0;
}

int tcg_has_work(void)
{
    CPUState *env;

    for (env = first_cpu; env != NULL; env = env->next_cpu)
        if (cpu_has_work(env))
            return 1;
    return 0;
}

static int vm_can_run(void)
{
    if (powerdown_requested)
        return 0;
    if (reset_requested)
        return 0;
    if (shutdown_requested)
        return 0;
    if (debug_requested)
        return 0;
    return 1;
}

static void main_loop(void)
{
    int r;

#ifdef CONFIG_IOTHREAD
    qemu_system_ready = 1;
    qemu_cond_broadcast(&qemu_system_cond);
#endif

    for (;;) {
        do {
#ifdef CONFIG_PROFILER
            int64_t ti;
#endif
            /*#ifdef CONFIG_S2E
                        g_s2e_state = s2e_select_next_state(g_s2e, g_s2e_state);
            #endif*/
#ifndef CONFIG_IOTHREAD
            tcg_cpu_exec();
#endif
#ifdef CONFIG_PROFILER
            ti = profile_getclock();
#endif
            main_loop_wait(qemu_calculate_timeout());
#ifdef CONFIG_PROFILER
            dev_time += profile_getclock() - ti;
#endif

            if (rotate_logs_requested) {
                FILE* new_dns_log_fd = rotate_qemu_log(get_slirp_dns_log_fd(),
                                                        dns_log_filename);
                FILE* new_drop_log_fd = rotate_qemu_log(get_slirp_drop_log_fd(),
                                                         drop_log_filename);
                slirp_dns_log_fd(new_dns_log_fd);
                slirp_drop_log_fd(new_drop_log_fd);
                reset_rotate_qemu_logs_request();
            }

        } while (vm_can_run());

        if (qemu_debug_requested())
            vm_stop(EXCP_DEBUG);
        if (qemu_shutdown_requested()) {
            if (no_shutdown) {
                vm_stop(0);
                no_shutdown = 0;
            } else {
                if (savevm_on_exit != NULL) {
                  do_savevm(cur_mon, savevm_on_exit);
                }
                break;
            }
        }
        if (qemu_reset_requested()) {
            pause_all_vcpus();
            qemu_system_reset();
            resume_all_vcpus();
        }
        if (qemu_powerdown_requested())
            qemu_system_powerdown();
        if ((r = qemu_vmstop_requested()))
            vm_stop(r);
    }
    pause_all_vcpus();
}

void version(void)
{
    printf("QEMU PC emulator version " QEMU_VERSION QEMU_PKGVERSION ", Copyright (c) 2003-2008 Fabrice Bellard\n");
}

void qemu_help(int exitcode)
{
    version();
    printf("usage: %s [options] [disk_image]\n"
           "\n"
           "'disk_image' is a raw hard image image for IDE hard disk 0\n"
           "\n"
#define DEF(option, opt_arg, opt_enum, opt_help)        \
           opt_help
#define DEFHEADING(text) stringify(text) "\n"
#include "qemu-options.h"
#undef DEF
#undef DEFHEADING
#undef GEN_DOCS
           "\n"
           "During emulation, the following keys are useful:\n"
           "ctrl-alt-f      toggle full screen\n"
           "ctrl-alt-n      switch to virtual console 'n'\n"
           "ctrl-alt        toggle mouse and keyboard grab\n"
           "\n"
           "When using -nographic, press 'ctrl-a h' to get some help.\n"
           ,
           "qemu",
           DEFAULT_RAM_SIZE,
#ifndef _WIN32
           DEFAULT_NETWORK_SCRIPT,
           DEFAULT_NETWORK_DOWN_SCRIPT,
#endif
           DEFAULT_GDBSTUB_PORT,
           "/tmp/qemu.log");
    QEMU_EXIT(exitcode);
}

#define HAS_ARG 0x0001

enum {
#define DEF(option, opt_arg, opt_enum, opt_help)        \
    opt_enum,
#define DEFHEADING(text)
#include "qemu-options.h"
#undef DEF
#undef DEFHEADING
#undef GEN_DOCS
};

typedef struct QEMUOption {
    const char *name;
    int flags;
    int index;
} QEMUOption;

static const QEMUOption qemu_options[] = {
    { "h", 0, QEMU_OPTION_h },
#define DEF(option, opt_arg, opt_enum, opt_help)        \
    { option, opt_arg, opt_enum },
#define DEFHEADING(text)
#include "qemu-options.h"
#undef DEF
#undef DEFHEADING
#undef GEN_DOCS
    { NULL, 0, 0 },
};

#ifdef HAS_AUDIO
struct soundhw soundhw[] = {
#ifdef HAS_AUDIO_CHOICE
#if defined(TARGET_I386) || defined(TARGET_MIPS)
    {
        "pcspk",
        "PC speaker",
        0,
        1,
        { .init_isa = pcspk_audio_init }
    },
#endif

#ifdef CONFIG_SB16
    {
        "sb16",
        "Creative Sound Blaster 16",
        0,
        1,
        { .init_isa = SB16_init }
    },
#endif

#ifdef CONFIG_CS4231A
    {
        "cs4231a",
        "CS4231A",
        0,
        1,
        { .init_isa = cs4231a_init }
    },
#endif

#ifdef CONFIG_ADLIB
    {
        "adlib",
#ifdef HAS_YMF262
        "Yamaha YMF262 (OPL3)",
#else
        "Yamaha YM3812 (OPL2)",
#endif
        0,
        1,
        { .init_isa = Adlib_init }
    },
#endif

#ifdef CONFIG_GUS
    {
        "gus",
        "Gravis Ultrasound GF1",
        0,
        1,
        { .init_isa = GUS_init }
    },
#endif

#ifdef CONFIG_AC97
    {
        "ac97",
        "Intel 82801AA AC97 Audio",
        0,
        0,
        { .init_pci = ac97_init }
    },
#endif

#ifdef CONFIG_ES1370
    {
        "es1370",
        "ENSONIQ AudioPCI ES1370",
        0,
        0,
        { .init_pci = es1370_init }
    },
#endif

#endif /* HAS_AUDIO_CHOICE */

    { NULL, NULL, 0, 0, { NULL } }
};

static void select_soundhw (const char *optarg)
{
    struct soundhw *c;

    if (*optarg == '?') {
    show_valid_cards:

        printf ("Valid sound card names (comma separated):\n");
        for (c = soundhw; c->name; ++c) {
            printf ("%-11s %s\n", c->name, c->descr);
        }
        printf ("\n-soundhw all will enable all of the above\n");
        if (*optarg != '?') {
            PANIC("Unknown sound card name: %s", optarg);
        } else {
            QEMU_EXIT(0);
        }
    }
    else {
        size_t l;
        const char *p;
        char *e;
        int bad_card = 0;

        if (!strcmp (optarg, "all")) {
            for (c = soundhw; c->name; ++c) {
                c->enabled = 1;
            }
            return;
        }

        p = optarg;
        while (*p) {
            e = strchr (p, ',');
            l = !e ? strlen (p) : (size_t) (e - p);

            for (c = soundhw; c->name; ++c) {
                if (!strncmp (c->name, p, l)) {
                    c->enabled = 1;
                    break;
                }
            }

            if (!c->name) {
#ifndef CONFIG_ANDROID
                if (l > 80) {
                    fprintf (stderr,
                             "Unknown sound card name (too big to show)\n");
                } else {
                    fprintf (stderr, "Unknown sound card name `%.*s'\n",
                             (int) l, p);
                }
#endif  // !CONFIG_ANDROID
	            bad_card = 1;
            }
            p += l + (e != NULL);
        }

        if (bad_card)
            goto show_valid_cards;
    }
}
#endif

static void select_vgahw (const char *p)
{
    const char *opts;

    cirrus_vga_enabled = 0;
    std_vga_enabled = 0;
    vmsvga_enabled = 0;
    xenfb_enabled = 0;
    if (strstart(p, "std", &opts)) {
        std_vga_enabled = 1;
    } else if (strstart(p, "cirrus", &opts)) {
        cirrus_vga_enabled = 1;
    } else if (strstart(p, "vmware", &opts)) {
        vmsvga_enabled = 1;
    } else if (strstart(p, "xenfb", &opts)) {
        xenfb_enabled = 1;
    } else if (!strstart(p, "none", &opts)) {
    invalid_vga:
        PANIC("Unknown vga type: %s", p);
    }
    while (*opts) {
        const char *nextopt;

        if (strstart(opts, ",retrace=", &nextopt)) {
            opts = nextopt;
            if (strstart(opts, "dumb", &nextopt))
                vga_retrace_method = VGA_RETRACE_DUMB;
            else if (strstart(opts, "precise", &nextopt))
                vga_retrace_method = VGA_RETRACE_PRECISE;
            else goto invalid_vga;
        } else goto invalid_vga;
        opts = nextopt;
    }
}

#ifdef _WIN32
static BOOL WINAPI qemu_ctrl_handler(DWORD type)
{
    exit(STATUS_CONTROL_C_EXIT);
    return TRUE;
}
#endif

int qemu_uuid_parse(const char *str, uint8_t *uuid)
{
    int ret;

    if(strlen(str) != 36)
        return -1;

    ret = sscanf(str, UUID_FMT, &uuid[0], &uuid[1], &uuid[2], &uuid[3],
            &uuid[4], &uuid[5], &uuid[6], &uuid[7], &uuid[8], &uuid[9],
            &uuid[10], &uuid[11], &uuid[12], &uuid[13], &uuid[14], &uuid[15]);

    if(ret != 16)
        return -1;

#ifdef TARGET_I386
    smbios_add_field(1, offsetof(struct smbios_type_1, uuid), 16, uuid);
#endif

    return 0;
}

#define MAX_NET_CLIENTS 32

#ifndef _WIN32

static void termsig_handler(int signal)
{
    qemu_system_shutdown_request();
}

static void sigchld_handler(int signal)
{
    waitpid(-1, NULL, WNOHANG);
}

static void sighandler_setup(void)
{
    struct sigaction act;

    memset(&act, 0, sizeof(act));
    act.sa_handler = termsig_handler;
    sigaction(SIGINT,  &act, NULL);
    sigaction(SIGHUP,  &act, NULL);
    sigaction(SIGTERM, &act, NULL);

    act.sa_handler = sigchld_handler;
    act.sa_flags = SA_NOCLDSTOP;
    sigaction(SIGCHLD, &act, NULL);
}

#endif

#ifdef _WIN32
/* Look for support files in the same directory as the executable.  */
static char *find_datadir(const char *argv0)
{
    char *p;
    char buf[MAX_PATH];
    DWORD len;

    len = GetModuleFileName(NULL, buf, sizeof(buf) - 1);
    if (len == 0) {
        return NULL;
    }

    buf[len] = 0;
    p = buf + len - 1;
    while (p != buf && *p != '\\')
        p--;
    *p = 0;
    if (access(buf, R_OK) == 0) {
        return qemu_strdup(buf);
    }
    return NULL;
}
#else /* !_WIN32 */

/* Find a likely location for support files using the location of the binary.
   For installed binaries this will be "$bindir/../share/qemu".  When
   running from the build tree this will be "$bindir/../pc-bios".
   The emulator running from the SDK will find the support files in $bindir/lib/pc-bios. */
#define SHARE_SUFFIX "/share/qemu"
#define BUILD_SUFFIX "/pc-bios"
#define SDK_SUFFIX "/lib/pc-bios"
static char *find_datadir(const char *argv0)
{
    char *dir;
    char *p = NULL;
    char *res;
#ifdef PATH_MAX
    char buf[PATH_MAX];
#endif
    size_t max_len;

#if defined(__linux__)
    {
        int len;
        len = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = 0;
            p = buf;
        }
    }
#elif defined(__FreeBSD__)
    {
        int len;
        len = readlink("/proc/curproc/file", buf, sizeof(buf) - 1);
        if (len > 0) {
            buf[len] = 0;
            p = buf;
        }
    }
#endif
    /* If we don't have any way of figuring out the actual executable
       location then try argv[0].  */
    if (!p) {
#ifdef PATH_MAX
        p = buf;
#endif
        p = realpath(argv0, p);
        if (!p) {
            return NULL;
        }
    }

#define STRLEN_CONST(str) (sizeof(str)-1)
    dir = dirname(p);
    max_len = strlen(dir) + 1 +
        MAX(STRLEN_CONST(SDK_SUFFIX), MAX(STRLEN_CONST(SHARE_SUFFIX), STRLEN_CONST(BUILD_SUFFIX)));
    res = qemu_mallocz(max_len);

    snprintf(res, max_len, "%s%s", dir, SDK_SUFFIX);
    if (access(res, R_OK)) {
      dir = dirname(dir);

      snprintf(res, max_len, "%s%s", dir, SHARE_SUFFIX);
      if (access(res, R_OK)) {
          snprintf(res, max_len, "%s%s", dir, BUILD_SUFFIX);
          if (access(res, R_OK)) {
              qemu_free(res);
              res = NULL;
          }
      }
    }
#ifndef PATH_MAX
    free(p);
#endif
    return res;
}
#undef SHARE_SUFFIX
#undef BUILD_SUFFIX
#endif

char *qemu_find_file(int type, const char *name)
{
    int len;
    const char *subdir;
    char *buf;

    /* If name contains path separators then try it as a straight path.  */
    if ((strchr(name, '/') || strchr(name, '\\'))
        && access(name, R_OK) == 0) {
        return strdup(name);
    }
    switch (type) {
    case QEMU_FILE_TYPE_LIB:
        /* XXX: Terrible hack. Redo it after deadline! */
        subdir="../i386-s2e-softmmu/";
        break;
    case QEMU_FILE_TYPE_BIOS:
        subdir = "";
        break;
    case QEMU_FILE_TYPE_KEYMAP:
        subdir = "keymaps/";
        break;
    default:
        abort();
    }
    len = strlen(data_dir) + strlen(name) + strlen(subdir) + 2;
    buf = qemu_mallocz(len);
    snprintf(buf, len, "%s/%s%s", data_dir, subdir, name);
    printf("%s\n", buf);
    if (access(buf, R_OK)) {
        qemu_free(buf);
        return NULL;
    }
    return buf;
}

static int
add_dns_server( const char*  server_name )
{
    SockAddress   addr;

    if (sock_address_init_resolve( &addr, server_name, 55, 0 ) < 0) {
        fprintf(stdout,
                "### WARNING: can't resolve DNS server name '%s'\n",
                server_name );
        return -1;
    }

    fprintf(stderr,
            "DNS server name '%s' resolved to %s\n", server_name, sock_address_to_string(&addr) );

    if ( slirp_add_dns_server( &addr ) < 0 ) {
        fprintf(stderr,
                "### WARNING: could not add DNS server '%s' to the network stack\n", server_name);
        return -1;
    }
    return 0;
}

/* Parses an integer
 * Pararm:
 *  str      String containing a number to be parsed.
 *  result   Passes the parsed integer in this argument
 *  returns  0 if ok, -1 if failed
 */
int
parse_int(const char *str, int *result)
{
    char* r;
    *result = strtol(str, &r, 0);
    if (r == NULL || *r != '\0')
      return -1;

    return 0;
}

#ifndef _WIN32
/*
 * Initializes the SIGUSR1 signal handler to clear Qemu logs.
 */
void init_qemu_clear_logs_sig() {
  struct sigaction act;
  sigfillset(&act.sa_mask);
  act.sa_flags = 0;
  act.sa_handler = rotate_qemu_logs_handler;
  if (sigaction(SIGUSR1, &act, NULL) == -1) {
    fprintf(stderr, "Failed to setup SIGUSR1 handler to clear Qemu logs\n");
    exit(-1);
  }
}
#endif



/* parses a null-terminated string specifying a network port (e.g., "80") or
 * port range (e.g., "[6666-7000]"). In case of a single port, lport and hport
 * are the same. Returns 0 on success, -1 on error. */

int parse_port_range(const char *str, unsigned short *lport,
                     unsigned short *hport) {

  unsigned int low = 0, high = 0;
  char *p, *arg = strdup(str);

  if ((*arg == '[') && ((p = strrchr(arg, ']')) != NULL)) {
    p = arg + 1;   /* skip '[' */
    low  = atoi(strtok(p, "-"));
    high = atoi(strtok(NULL, "-"));
    if ((low > 0) && (high > 0) && (low < high) && (high < 65535)) {
      *lport = low;
      *hport = high;
    }
  }
  else {
    low = atoi(arg);
    if ((0 < low) && (low < 65535)) {
      *lport = low;
      *hport = low;
    }
  }
  free(arg);
  if (low != 0)
    return 0;
  return -1;
}

/*
 * Implements the generic port forwarding option
 */
void
net_slirp_forward(const char *optarg)
{
    /*
     * we expect the following format:
     * dst_net:dst_mask:dst_port:redirect_ip:redirect_port OR
     * dst_net:dst_mask:[dp_range_start-dp_range_end]:redirect_ip:redirect_port
     */
    char *argument = strdup(optarg), *p = argument;
    char *dst_net, *dst_mask, *dst_port;
    char *redirect_ip, *redirect_port;
    uint32_t dnet, dmask, rip;
    unsigned short dlport, dhport, rport;


    dst_net = strtok(p, ":");
    dst_mask = strtok(NULL, ":");
    dst_port = strtok(NULL, ":");
    redirect_ip = strtok(NULL, ":");
    redirect_port = strtok(NULL, ":");

    if (dst_net == NULL || dst_mask == NULL || dst_port == NULL ||
        redirect_ip == NULL || redirect_port == NULL) {
        fprintf(stderr,
                "Invalid argument for -net-forward, we expect "
                "dst_net:dst_mask:dst_port:redirect_ip:redirect_port or "
                "dst_net:dst_mask:[dp_range_start-dp_range_end]"
                ":redirect_ip:redirect_port: %s\n",
                optarg);
        exit(1);
    }

    /* inet_strtoip converts dotted address to host byte order */
    if (inet_strtoip(dst_net, &dnet) == -1) {
        fprintf(stderr, "Invalid destination IP net: %s\n", dst_net);
        exit(1);
    }
    if (inet_strtoip(dst_mask, &dmask) == -1) {
        fprintf(stderr, "Invalid destination IP mask: %s\n", dst_mask);
        exit(1);
    }
    if (inet_strtoip(redirect_ip, &rip) == -1) {
        fprintf(stderr, "Invalid redirect IP address: %s\n", redirect_ip);
        exit(1);
    }

    if (parse_port_range(dst_port, &dlport, &dhport) == -1) {
        fprintf(stderr, "Invalid destination port or port range\n");
        exit(1);
    }

    rport = atoi(redirect_port);
    if (!rport) {
        fprintf(stderr, "Invalid redirect port: %s\n", redirect_port);
        exit(1);
    }

    dnet &= dmask;

    slirp_add_net_forward(dnet, dmask, dlport, dhport,
                          rip, rport);

    free(argument);
}


/* Parses an -allow-tcp or -allow-udp argument and inserts a corresponding
 * entry in the allows list */
void
slirp_allow(const char *optarg, u_int8_t proto)
{
  /*
   * we expect the following format:
   * dst_ip:dst_port OR dst_ip:[dst_lport-dst_hport]
   */
  char *argument = strdup(optarg), *p = argument;
  char *dst_ip_str, *dst_port_str;
  uint32_t dst_ip;
  unsigned short dst_lport, dst_hport;

  dst_ip_str = strtok(p, ":");
  dst_port_str = strtok(NULL, ":");

  if (dst_ip_str == NULL || dst_port_str == NULL) {
    fprintf(stderr,
            "Invalid argument %s for -allow. We expect "
            "dst_ip:dst_port or dst_ip:[dst_lport-dst_hport]\n",
            optarg);
    exit(1);
  }

  if (inet_strtoip(dst_ip_str, &dst_ip) == -1) {
    fprintf(stderr, "Invalid destination IP address: %s\n", dst_ip_str);
    exit(1);
  }
  if (parse_port_range(dst_port_str, &dst_lport, &dst_hport) == -1) {
    fprintf(stderr, "Invalid destination port or port range\n");
    exit(1);
  }

  slirp_add_allow(dst_ip, dst_lport, dst_hport, proto);

  free(argument);
}

/* Add a serial device at a given location in the emulated hardware table.
 * On failure, this function aborts the program with an error message.
 */
static void
serial_hds_add_at(int  index, const char* devname)
{
    char label[32];

    if (!devname || !strcmp(devname,"none"))
        return;

    if (index >= MAX_SERIAL_PORTS) {
        PANIC("qemu: invalid serial index for %s (%d >= %d)",
              devname, index, MAX_SERIAL_PORTS);
    }
    if (serial_hds[index] != NULL) {
        PANIC("qemu: invalid serial index for %s (%d: already taken!)",
              devname, index);
    }
    snprintf(label, sizeof(label), "serial%d", index);
    serial_hds[index] = qemu_chr_open(label, devname, NULL);
    if (!serial_hds[index]) {
        PANIC("qemu: could not open serial device '%s'", devname);
    }
}


/* Find a free slot in the emulated serial device table, and register
 * it. Return the allocated table index.
 */
static int
serial_hds_add(const char* devname)
{
    int  index;

    /* Find first free slot */
    for (index = 0; index < MAX_SERIAL_PORTS; index++) {
        if (serial_hds[index] == NULL) {
            serial_hds_add_at(index, devname);
            return index;
        }
    }

    PANIC("qemu: too many serial devices registered (%d)", index);
    return -1;  /* shouldn't happen */
}

int main(int argc, char **argv, char **envp)
{
    const char *gdbstub_dev = NULL;
    uint32_t boot_devices_bitmap = 0;
    int i;
    int snapshot, linux_boot, net_boot;
    const char *icount_option = NULL;
    const char *initrd_filename;
    const char *kernel_filename, *kernel_cmdline;
    const char *boot_devices = "";
    DisplayState *ds;
    DisplayChangeListener *dcl;
    int cyls, heads, secs, translation;
    QemuOpts *hda_opts = NULL;
    QemuOpts *hdb_opts = NULL;
    const char *net_clients[MAX_NET_CLIENTS];
    int nb_net_clients;
    const char *bt_opts[MAX_BT_CMDLINE];
    int nb_bt_opts;
    int optind;
    const char *r, *optarg;
    CharDriverState *monitor_hd = NULL;
    const char *monitor_device;
    const char *serial_devices[MAX_SERIAL_PORTS];
    int serial_device_index;
    const char *parallel_devices[MAX_PARALLEL_PORTS];
    int parallel_device_index;
    const char *virtio_consoles[MAX_VIRTIO_CONSOLES];
    int virtio_console_index;
    const char *loadvm = NULL;
    QEMUMachine *machine;
    const char *cpu_model;

#ifdef CONFIG_S2E
    const char *s2e_config_file = NULL;
    const char *s2e_output_dir = NULL;
    int execute_always_klee = 0;
    int s2e_verbose = 0;
#endif

    const char *usb_devices[MAX_USB_CMDLINE];
    int usb_devices_index;
#ifndef _WIN32
    int fds[2];
#endif
    int tb_size;
    const char *pid_file = NULL;
    const char *incoming = NULL;
#ifndef _WIN32
    int fd = 0;
    struct passwd *pwd = NULL;
    const char *chroot_dir = NULL;
    const char *run_as = NULL;
#endif
    CPUState *env;
    int show_vnc_port = 0;
    IniFile*  hw_ini = NULL;
    STRALLOC_DEFINE(kernel_params);
    STRALLOC_DEFINE(kernel_config);
    int    dns_count = 0;

    /* Initialize sockets before anything else, so we can properly report
     * initialization failures back to the UI. */
#ifdef _WIN32
    socket_init();
#endif

    init_clocks();

    qemu_cache_utils_init(envp);

    QLIST_INIT (&vm_change_state_head);

#ifndef _WIN32
    {
        struct sigaction act;
        sigfillset(&act.sa_mask);
        act.sa_flags = 0;
        act.sa_handler = SIG_IGN;
        sigaction(SIGPIPE, &act, NULL);
    }
#else
    SetConsoleCtrlHandler(qemu_ctrl_handler, TRUE);
    /* Note: cpu_interrupt() is currently not SMP safe, so we force
       QEMU to run on a single CPU */
    {
        HANDLE h;
        DWORD_PTR mask, smask;
        int i;
        h = GetCurrentProcess();
        if (GetProcessAffinityMask(h, (PDWORD_PTR)&mask, (PDWORD_PTR)&smask)) {
            for(i = 0; i < 32; i++) {
                if (mask & (1 << i))
                    break;
            }
            if (i != 32) {
                mask = 1 << i;
                SetProcessAffinityMask(h, mask);
            }
        }
    }
#endif

    module_call_init(MODULE_INIT_MACHINE);
    machine = find_default_machine();
    cpu_model = NULL;
    initrd_filename = NULL;
    ram_size = 0;
    snapshot = 0;
    kernel_filename = NULL;
    kernel_cmdline = "";

    cyls = heads = secs = 0;
    translation = BIOS_ATA_TRANSLATION_AUTO;
    monitor_device = "vc:80Cx24C";

    serial_devices[0] = "vc:80Cx24C";
    for(i = 1; i < MAX_SERIAL_PORTS; i++)
        serial_devices[i] = NULL;
    serial_device_index = 0;

    parallel_devices[0] = "vc:80Cx24C";
    for(i = 1; i < MAX_PARALLEL_PORTS; i++)
        parallel_devices[i] = NULL;
    parallel_device_index = 0;

    for(i = 0; i < MAX_VIRTIO_CONSOLES; i++)
        virtio_consoles[i] = NULL;
    virtio_console_index = 0;

    for (i = 0; i < MAX_NODES; i++) {
        node_mem[i] = 0;
        node_cpumask[i] = 0;
    }

    usb_devices_index = 0;

    nb_net_clients = 0;
    nb_bt_opts = 0;
#ifdef MAX_DRIVES
    nb_drives = 0;
    nb_drives_opt = 0;
#endif
    nb_numa_nodes = 0;

    nb_nics = 0;

    tb_size = 0;
    autostart= 1;

    register_watchdogs();

    /* Initialize boot properties. */
    boot_property_init_service();

    optind = 1;
    for(;;) {
        if (optind >= argc)
            break;
        r = argv[optind];
        if (r[0] != '-') {
            hda_opts = drive_add(argv[optind++], HD_ALIAS, 0);
        } else {
            const QEMUOption *popt;

            optind++;
            /* Treat --foo the same as -foo.  */
            if (r[1] == '-')
                r++;
            popt = qemu_options;
            for(;;) {
                if (!popt->name) {
                    PANIC("%s: invalid option -- '%s'",
                                      argv[0], r);
                }
                if (!strcmp(popt->name, r + 1))
                    break;
                popt++;
            }
            if (popt->flags & HAS_ARG) {
                if (optind >= argc) {
                    PANIC("%s: option '%s' requires an argument",
                                      argv[0], r);
                }
                optarg = argv[optind++];
            } else {
                optarg = NULL;
            }

            switch(popt->index) {
            case QEMU_OPTION_M:
                machine = find_machine(optarg);
                if (!machine) {
                    QEMUMachine *m;
                    printf("Supported machines are:\n");
                    for(m = first_machine; m != NULL; m = m->next) {
                        printf("%-10s %s%s\n",
                               m->name, m->desc,
                               m->is_default ? " (default)" : "");
                    }
                    if (*optarg != '?') {
                        PANIC("Invalid machine parameter: %s",
                                          optarg);
                    } else {
                        QEMU_EXIT(0);
                    }
                }
                break;
            case QEMU_OPTION_cpu:
                /* hw initialization will check this */
                if (*optarg == '?') {
/* XXX: implement xxx_cpu_list for targets that still miss it */
#if defined(cpu_list)
                    cpu_list(stdout, &fprintf);
#endif
                    QEMU_EXIT(0);
                } else {
                    cpu_model = optarg;
                }
                break;
#ifdef CONFIG_S2E
            case QEMU_OPTION_s2e_config_file:
              s2e_config_file = optarg;
              break;
            case QEMU_OPTION_s2e_output_dir:
              s2e_output_dir = optarg;
              break;
#else
            case QEMU_OPTION_fake_pci_name:
              g_fake_pci.fake_pci_name = optarg;
              break;
            case QEMU_OPTION_fake_pci_vendor_id:
              g_fake_pci.fake_pci_vendor_id = strtol(optarg, NULL, 0);
              break;
            case QEMU_OPTION_fake_pci_device_id:
              g_fake_pci.fake_pci_device_id = strtol(optarg, NULL, 0);
              break;
            case QEMU_OPTION_fake_pci_revision_id:
              g_fake_pci.fake_pci_revision_id = strtol(optarg, NULL, 0);
              break;
            case QEMU_OPTION_fake_pci_class_code:
              g_fake_pci.fake_pci_class_code = strtol(optarg, NULL, 0);
              break;
            case QEMU_OPTION_fake_pci_ss_vendor_id:
              g_fake_pci.fake_pci_ss_vendor_id = strtol(optarg, NULL, 0);
              break;
            case QEMU_OPTION_fake_pci_ss_id:
              g_fake_pci.fake_pci_ss_id = strtol(optarg, NULL, 0);
              break;
            case QEMU_OPTION_fake_pci_resource_io:
              {
                PCIIORegion region =
                  { -1, strtol(optarg, NULL, 0), 0, PCI_BASE_ADDRESS_SPACE_IO, NULL };
                if (g_fake_pci.fake_pci_num_resources < PCI_NUM_REGIONS)
                  g_fake_pci.fake_pci_resources[g_fake_pci.fake_pci_num_resources++] = region;
              }
              break;
            case QEMU_OPTION_fake_pci_resource_mem:
              {
                PCIIORegion region =
                  { -1, strtol(optarg, NULL, 0), 0, PCI_BASE_ADDRESS_SPACE_MEMORY, NULL };
                if (g_fake_pci.fake_pci_num_resources < PCI_NUM_REGIONS)
                  g_fake_pci.fake_pci_resources[g_fake_pci.fake_pci_num_resources++] = region;
              }
              break;
            case QEMU_OPTION_fake_pci_resource_mem_prefetch:
              {
                PCIIORegion region =
                  { -1, strtol(optarg, NULL, 0), 0, PCI_BASE_ADDRESS_MEM_PREFETCH, NULL };
                if (g_fake_pci.fake_pci_num_resources < PCI_NUM_REGIONS)
                  g_fake_pci.fake_pci_resources[g_fake_pci.fake_pci_num_resources++] = region;
              }
              break;
            case QEMU_OPTION_fake_pci_resource_rom:
              {
                PCIIORegion region =
                  { -1, strtol(optarg, NULL, 0), 0, PCI_ROM_ADDRESS, NULL };
                g_fake_pci.fake_pci_num_resources = PCI_NUM_REGIONS;
                g_fake_pci.fake_pci_resources[PCI_ROM_SLOT] = region;
              }
              break;

#endif
            case QEMU_OPTION_initrd:
                initrd_filename = optarg;
                break;
            case QEMU_OPTION_hda:
                if (cyls == 0)
                    hda_opts = drive_add(optarg, HD_ALIAS, 0);
                else
                    hda_opts = drive_add(optarg, HD_ALIAS
			     ",cyls=%d,heads=%d,secs=%d%s",
                             0, cyls, heads, secs,
                             translation == BIOS_ATA_TRANSLATION_LBA ?
                                 ",trans=lba" :
                             translation == BIOS_ATA_TRANSLATION_NONE ?
                                 ",trans=none" : "");
                 break;
            case QEMU_OPTION_hdb:
                hdb_opts = drive_add(optarg, HD_ALIAS, 1);
                break;

            case QEMU_OPTION_hdc:
            case QEMU_OPTION_hdd:
                drive_add(optarg, HD_ALIAS, popt->index - QEMU_OPTION_hda);
                break;
            case QEMU_OPTION_drive:
                drive_add(NULL, "%s", optarg);
	        break;
            case QEMU_OPTION_mtdblock:
                drive_add(optarg, MTD_ALIAS);
                break;
            case QEMU_OPTION_sd:
                drive_add(optarg, SD_ALIAS);
                break;
            case QEMU_OPTION_pflash:
                drive_add(optarg, PFLASH_ALIAS);
                break;
            case QEMU_OPTION_snapshot:
                snapshot = 1;
                break;
            case QEMU_OPTION_hdachs:
                {
                    const char *p;
                    p = optarg;
                    cyls = strtol(p, (char **)&p, 0);
                    if (cyls < 1 || cyls > 16383)
                        goto chs_fail;
                    if (*p != ',')
                        goto chs_fail;
                    p++;
                    heads = strtol(p, (char **)&p, 0);
                    if (heads < 1 || heads > 16)
                        goto chs_fail;
                    if (*p != ',')
                        goto chs_fail;
                    p++;
                    secs = strtol(p, (char **)&p, 0);
                    if (secs < 1 || secs > 63)
                        goto chs_fail;
                    if (*p == ',') {
                        p++;
                        if (!strcmp(p, "none"))
                            translation = BIOS_ATA_TRANSLATION_NONE;
                        else if (!strcmp(p, "lba"))
                            translation = BIOS_ATA_TRANSLATION_LBA;
                        else if (!strcmp(p, "auto"))
                            translation = BIOS_ATA_TRANSLATION_AUTO;
                        else
                            goto chs_fail;
                    } else if (*p != '\0') {
                    chs_fail:
                        PANIC("qemu: invalid physical CHS format");
                    }
		    if (hda_opts != NULL) {
                        char num[16];
                        snprintf(num, sizeof(num), "%d", cyls);
                        qemu_opt_set(hda_opts, "cyls", num);
                        snprintf(num, sizeof(num), "%d", heads);
                        qemu_opt_set(hda_opts, "heads", num);
                        snprintf(num, sizeof(num), "%d", secs);
                        qemu_opt_set(hda_opts, "secs", num);
                        if (translation == BIOS_ATA_TRANSLATION_LBA)
                            qemu_opt_set(hda_opts, "trans", "lba");
                        if (translation == BIOS_ATA_TRANSLATION_NONE)
                            qemu_opt_set(hda_opts, "trans", "none");
                    }
                }
                break;
            case QEMU_OPTION_numa:
                if (nb_numa_nodes >= MAX_NODES) {
                    PANIC("qemu: too many NUMA nodes");
                }
                numa_add(optarg);
                break;
            case QEMU_OPTION_nographic:
                display_type = DT_NOGRAPHIC;
                break;
#ifdef CONFIG_CURSES
            case QEMU_OPTION_curses:
                display_type = DT_CURSES;
                break;
#endif
            case QEMU_OPTION_portrait:
                graphic_rotate = 1;
                break;
            case QEMU_OPTION_kernel:
                kernel_filename = optarg;
                break;
            case QEMU_OPTION_append:
                kernel_cmdline = optarg;
                break;
            case QEMU_OPTION_cdrom:
                drive_add(optarg, CDROM_ALIAS);
                break;
            case QEMU_OPTION_boot:
                boot_devices = optarg;
                /* We just do some generic consistency checks */
                {
                    /* Could easily be extended to 64 devices if needed */
                    const char *p;

                    boot_devices_bitmap = 0;
                    for (p = boot_devices; *p != '\0'; p++) {
                        /* Allowed boot devices are:
                         * a b     : floppy disk drives
                         * c ... f : IDE disk drives
                         * g ... m : machine implementation dependant drives
                         * n ... p : network devices
                         * It's up to each machine implementation to check
                         * if the given boot devices match the actual hardware
                         * implementation and firmware features.
                         */
                        if (*p < 'a' || *p > 'q') {
                            PANIC("Invalid boot device '%c'", *p);
                        }
                        if (boot_devices_bitmap & (1 << (*p - 'a'))) {
                            PANIC(
                                    "Boot device '%c' was given twice",*p);
                        }
                        boot_devices_bitmap |= 1 << (*p - 'a');
                    }
                }
                break;
            case QEMU_OPTION_fda:
            case QEMU_OPTION_fdb:
                drive_add(optarg, FD_ALIAS, popt->index - QEMU_OPTION_fda);
                break;
#ifdef TARGET_I386
            case QEMU_OPTION_no_fd_bootchk:
                fd_bootchk = 0;
                break;
#endif
            case QEMU_OPTION_net:
                if (nb_net_clients >= MAX_NET_CLIENTS) {
                    PANIC("qemu: too many network clients");
                }
                net_clients[nb_net_clients] = optarg;
                nb_net_clients++;
                break;
#ifdef CONFIG_SLIRP
            case QEMU_OPTION_tftp:
		tftp_prefix = optarg;
                break;
            case QEMU_OPTION_bootp:
                bootp_filename = optarg;
                break;
#if 0  /* ANDROID disabled */
#ifndef _WIN32
            case QEMU_OPTION_smb:
		net_slirp_smb(optarg);
                break;
#endif
#endif /* ANDROID */
            case QEMU_OPTION_redir:
                net_slirp_redir(NULL, optarg, NULL);
                break;
#endif
            case QEMU_OPTION_bt:
                if (nb_bt_opts >= MAX_BT_CMDLINE) {
                    PANIC("qemu: too many bluetooth options");
                }
                bt_opts[nb_bt_opts++] = optarg;
                break;
#ifdef HAS_AUDIO
            case QEMU_OPTION_audio_help:
                AUD_help ();
                QEMU_EXIT(0);
                break;
            case QEMU_OPTION_soundhw:
                select_soundhw (optarg);
                break;
#endif
            case QEMU_OPTION_h:
                qemu_help(0);
                break;
            case QEMU_OPTION_version:
                version();
                QEMU_EXIT(0);
                break;
            case QEMU_OPTION_m: {
                uint64_t value;
                char *ptr;

                value = strtoul(optarg, &ptr, 10);
                switch (*ptr) {
                case 0: case 'M': case 'm':
                    value <<= 20;
                    break;
                case 'G': case 'g':
                    value <<= 30;
                    break;
                default:
                    PANIC("qemu: invalid ram size: %s", optarg);
                }

                /* On 32-bit hosts, QEMU is limited by virtual address space */
                if (value > (2047 << 20)
#ifndef CONFIG_KQEMU
                    && HOST_LONG_BITS == 32
#endif
                    ) {
                    PANIC("qemu: at most 2047 MB RAM can be simulated");
                }
                if (value != (uint64_t)(ram_addr_t)value) {
                    PANIC("qemu: ram size too large");
                }
                ram_size = value;
                break;
            }
            case QEMU_OPTION_d:
                {
                    int mask;
                    const CPULogItem *item;

                    mask = cpu_str_to_log_mask(optarg);
                    if (!mask) {
                        printf("Log items (comma separated):\n");
                        for(item = cpu_log_items; item->mask != 0; item++) {
                            printf("%-10s %s\n", item->name, item->help);
                        }
                        PANIC("Invalid parameter -d=%s", optarg);
                    }
                    cpu_set_log(mask);
                }
                break;
            case QEMU_OPTION_s:
                gdbstub_dev = "tcp::" DEFAULT_GDBSTUB_PORT;
                break;
            case QEMU_OPTION_gdb:
                gdbstub_dev = optarg;
                break;
            case QEMU_OPTION_L:
                data_dir = optarg;
                break;
            case QEMU_OPTION_bios:
                bios_name = optarg;
                break;
            case QEMU_OPTION_singlestep:
                singlestep = 1;
                break;
            case QEMU_OPTION_S:
#if 0  /* ANDROID */
                PANIC("Sorry, stopped launch is not supported in the Android emulator" );
#endif
                autostart = 0;
                break;
#ifndef _WIN32
	    case QEMU_OPTION_k:
		keyboard_layout = optarg;
		break;
#endif
            case QEMU_OPTION_localtime:
                rtc_utc = 0;
                break;
            case QEMU_OPTION_vga:
                select_vgahw (optarg);
                break;
#if defined(TARGET_PPC) || defined(TARGET_SPARC)
            case QEMU_OPTION_g:
                {
                    const char *p;
                    int w, h, depth;
                    p = optarg;
                    w = strtol(p, (char **)&p, 10);
                    if (w <= 0) {
                    graphic_error:
                        PANIC("qemu: invalid resolution or depth");
                    }
                    if (*p != 'x')
                        goto graphic_error;
                    p++;
                    h = strtol(p, (char **)&p, 10);
                    if (h <= 0)
                        goto graphic_error;
                    if (*p == 'x') {
                        p++;
                        depth = strtol(p, (char **)&p, 10);
                        if (depth != 8 && depth != 15 && depth != 16 &&
                            depth != 24 && depth != 32)
                            goto graphic_error;
                    } else if (*p == '\0') {
                        depth = graphic_depth;
                    } else {
                        goto graphic_error;
                    }

                    graphic_width = w;
                    graphic_height = h;
                    graphic_depth = depth;
                }
                break;
#endif
            case QEMU_OPTION_echr:
                {
                    char *r;
                    term_escape_char = strtol(optarg, &r, 0);
                    if (r == optarg)
                        printf("Bad argument to echr\n");
                    break;
                }
            case QEMU_OPTION_monitor:
                monitor_device = optarg;
                break;
            case QEMU_OPTION_serial:
                if (serial_device_index >= MAX_SERIAL_PORTS) {
                    PANIC("qemu: too many serial ports");
                }
                serial_devices[serial_device_index] = optarg;
                serial_device_index++;
                break;
            case QEMU_OPTION_watchdog:
                i = select_watchdog(optarg);
                if (i > 0) {
                    if (i == 1) {
                        PANIC("Invalid watchdog parameter: %s",
                                          optarg);
                    } else {
                        QEMU_EXIT(0);
                    }
                }
                break;
            case QEMU_OPTION_watchdog_action:
                if (select_watchdog_action(optarg) == -1) {
                    PANIC("Unknown -watchdog-action parameter");
                }
                break;
            case QEMU_OPTION_virtiocon:
                if (virtio_console_index >= MAX_VIRTIO_CONSOLES) {
                    PANIC("qemu: too many virtio consoles");
                }
                virtio_consoles[virtio_console_index] = optarg;
                virtio_console_index++;
                break;
            case QEMU_OPTION_parallel:
                if (parallel_device_index >= MAX_PARALLEL_PORTS) {
                    PANIC("qemu: too many parallel ports");
                }
                parallel_devices[parallel_device_index] = optarg;
                parallel_device_index++;
                break;
            case QEMU_OPTION_loadvm:
                loadvm = optarg;
                break;
            case QEMU_OPTION_savevm_on_exit:
                savevm_on_exit = optarg;
                break;
            case QEMU_OPTION_full_screen:
                full_screen = 1;
                break;
#ifdef CONFIG_SDL
            case QEMU_OPTION_no_frame:
                no_frame = 1;
                break;
            case QEMU_OPTION_alt_grab:
                alt_grab = 1;
                break;
            case QEMU_OPTION_no_quit:
                no_quit = 1;
                break;
            case QEMU_OPTION_sdl:
                display_type = DT_SDL;
                break;
#endif
            case QEMU_OPTION_pidfile:
                pid_file = optarg;
                break;
#ifdef TARGET_I386
            case QEMU_OPTION_win2k_hack:
                win2k_install_hack = 1;
                break;
            case QEMU_OPTION_rtc_td_hack:
                rtc_td_hack = 1;
                break;
#ifndef CONFIG_ANDROID
            case QEMU_OPTION_acpitable:
                if(acpi_table_add(optarg) < 0) {
                    PANIC("Wrong acpi table provided");
                }
                break;
#endif
            case QEMU_OPTION_smbios:
                if(smbios_entry_add(optarg) < 0) {
                    PANIC("Wrong smbios provided");
                }
                break;
#endif
#ifdef CONFIG_KQEMU
            case QEMU_OPTION_no_kqemu:
                kqemu_allowed = 0;
                break;
            case QEMU_OPTION_kernel_kqemu:
                kqemu_allowed = 2;
                break;
#endif
#ifdef TARGET_I386
#ifdef CONFIG_KVM
            case QEMU_OPTION_enable_kvm:
                kvm_allowed = 1;
#ifdef CONFIG_KQEMU
                kqemu_allowed = 0;
#endif
                break;
#endif
#endif  /* TARGET_I386 */
            case QEMU_OPTION_usb:
                usb_enabled = 1;
                break;
            case QEMU_OPTION_usbdevice:
                usb_enabled = 1;
                if (usb_devices_index >= MAX_USB_CMDLINE) {
                    PANIC("Too many USB devices");
                }
                usb_devices[usb_devices_index] = optarg;
                usb_devices_index++;
                break;
            case QEMU_OPTION_smp:
                smp_cpus = atoi(optarg);
                if (smp_cpus < 1) {
                    PANIC("Invalid number of CPUs");
                }
                break;
	    case QEMU_OPTION_vnc:
                display_type = DT_VNC;
		vnc_display = optarg;
		break;
#ifdef TARGET_I386
            case QEMU_OPTION_no_acpi:
                acpi_enabled = 0;
                break;
            case QEMU_OPTION_no_hpet:
                no_hpet = 0;
                break;
            case QEMU_OPTION_no_virtio_balloon:
                no_virtio_balloon = 1;
                break;
#endif
            case QEMU_OPTION_no_reboot:
                no_reboot = 1;
                break;
            case QEMU_OPTION_no_shutdown:
                no_shutdown = 1;
                break;
            case QEMU_OPTION_show_cursor:
                cursor_hide = 0;
                break;
            case QEMU_OPTION_uuid:
                if(qemu_uuid_parse(optarg, qemu_uuid) < 0) {
                    PANIC("Fail to parse UUID string. Wrong format.");
                }
                break;
#ifndef _WIN32
	    case QEMU_OPTION_daemonize:
		daemonize = 1;
		break;
#endif
	    case QEMU_OPTION_option_rom:
		if (nb_option_roms >= MAX_OPTION_ROMS) {
		    PANIC("Too many option ROMs");
		}
		option_rom[nb_option_roms] = optarg;
		nb_option_roms++;
		break;
#if defined(TARGET_ARM) || defined(TARGET_M68K)
            case QEMU_OPTION_semihosting:
                semihosting_enabled = 1;
                break;
#endif
            case QEMU_OPTION_name:
                qemu_name = optarg;
                break;
#if defined(TARGET_SPARC) || defined(TARGET_PPC)
            case QEMU_OPTION_prom_env:
                if (nb_prom_envs >= MAX_PROM_ENVS) {
                    PANIC("Too many prom variables");
                }
                prom_envs[nb_prom_envs] = optarg;
                nb_prom_envs++;
                break;
#endif
#ifdef TARGET_ARM
            case QEMU_OPTION_old_param:
                old_param = 1;
                break;
#endif
            case QEMU_OPTION_clock:
                configure_alarms(optarg);
                break;
            case QEMU_OPTION_startdate:
                {
                    struct tm tm;
                    time_t rtc_start_date = 0;
                    if (!strcmp(optarg, "now")) {
                        rtc_date_offset = -1;
                    } else {
                        if (sscanf(optarg, "%d-%d-%dT%d:%d:%d",
                               &tm.tm_year,
                               &tm.tm_mon,
                               &tm.tm_mday,
                               &tm.tm_hour,
                               &tm.tm_min,
                               &tm.tm_sec) == 6) {
                            /* OK */
                        } else if (sscanf(optarg, "%d-%d-%d",
                                          &tm.tm_year,
                                          &tm.tm_mon,
                                          &tm.tm_mday) == 3) {
                            tm.tm_hour = 0;
                            tm.tm_min = 0;
                            tm.tm_sec = 0;
                        } else {
                            goto date_fail;
                        }
                        tm.tm_year -= 1900;
                        tm.tm_mon--;
                        rtc_start_date = mktimegm(&tm);
                        if (rtc_start_date == -1) {
                        date_fail:
                            PANIC("Invalid date format. Valid format are:\n"
                                    "'now' or '2006-06-17T16:01:21' or '2006-06-17'");
                        }
                        rtc_date_offset = time(NULL) - rtc_start_date;
                    }
                }
                break;

            /* -------------------------------------------------------*/
            /* User mode network stack restrictions */
            case QEMU_OPTION_drop_udp:
                slirp_drop_udp();
                break;
            case QEMU_OPTION_drop_tcp:
                slirp_drop_tcp();
                break;
            case QEMU_OPTION_allow_tcp:
                slirp_allow(optarg, IPPROTO_TCP);
                break;
            case QEMU_OPTION_allow_udp:
                slirp_allow(optarg, IPPROTO_UDP);
                break;
             case QEMU_OPTION_drop_log:
                {
                    FILE* drop_log_fd;
                    drop_log_filename = optarg;
                    drop_log_fd = fopen(optarg, "w+");

                    if (!drop_log_fd) {
                        fprintf(stderr, "Cannot open drop log: %s\n", optarg);
                        exit(1);
                    }

                    slirp_drop_log_fd(drop_log_fd);
                }
                break;

            case QEMU_OPTION_dns_log:
                {
                    FILE* dns_log_fd;
                    dns_log_filename = optarg;
                    dns_log_fd = fopen(optarg, "wb+");

                    if (dns_log_fd == NULL) {
                        fprintf(stderr, "Cannot open dns log: %s\n", optarg);
                        exit(1);
                    }

                    slirp_dns_log_fd(dns_log_fd);
                }
                break;


            case QEMU_OPTION_max_dns_conns:
                {
                    int max_dns_conns = 0;
                    if (parse_int(optarg, &max_dns_conns)) {
                      fprintf(stderr,
                              "qemu: syntax: -max-dns-conns max_connections\n");
                      exit(1);
                    }
                    if (max_dns_conns <= 0 ||  max_dns_conns == LONG_MAX) {
                      fprintf(stderr,
                              "Invalid arg for max dns connections: %s\n",
                              optarg);
                      exit(1);
                    }
                    slirp_set_max_dns_conns(max_dns_conns);
                }
                break;

            case QEMU_OPTION_net_forward:
                net_slirp_forward(optarg);
                break;
            case QEMU_OPTION_net_forward_tcp2sink:
                {
                    SockAddress saddr;

                    if (parse_host_port(&saddr, optarg)) {
                        fprintf(stderr,
                                "Invalid ip/port %s for "
                                "-forward-dropped-tcp2sink. "
                                "We expect 'sink_ip:sink_port'\n",
                                optarg);
                        exit(1);
                    }
                    slirp_forward_dropped_tcp2sink(saddr.u.inet.address,
                                                   saddr.u.inet.port);
                }
                break;
            /* -------------------------------------------------------*/

            case QEMU_OPTION_tb_size:
                tb_size = strtol(optarg, NULL, 0);
                if (tb_size < 0)
                    tb_size = 0;
                break;
            case QEMU_OPTION_icount:
                icount_option = optarg;
                break;
            case QEMU_OPTION_incoming:
                incoming = optarg;
                break;
#ifndef _WIN32
            case QEMU_OPTION_chroot:
                chroot_dir = optarg;
                break;
            case QEMU_OPTION_runas:
                run_as = optarg;
                break;
#endif
#ifdef CONFIG_XEN
            case QEMU_OPTION_xen_domid:
                xen_domid = atoi(optarg);
                break;
            case QEMU_OPTION_xen_create:
                xen_mode = XEN_CREATE;
                break;
            case QEMU_OPTION_xen_attach:
                xen_mode = XEN_ATTACH;
                break;
#endif

#if defined(CONFIG_LLVM) && !defined(CONFIG_S2E)
            case QEMU_OPTION_execute_llvm:
                generate_llvm = 1;
                execute_llvm = 1;
                break;
            case QEMU_OPTION_generate_llvm:
                generate_llvm = 1;
                break;
#endif
#ifdef CONFIG_S2E
            case QEMU_OPTION_always_klee:
                execute_always_klee = 1;
                break;
            case QEMU_OPTION_s2e_verbose:
                s2e_verbose = 1;
                break;
#endif


            case QEMU_OPTION_mic:
                audio_input_source = (char*)optarg;
                break;
#ifdef CONFIG_TRACE
            case QEMU_OPTION_trace:
                trace_filename = optarg;
                tracing = 1;
                break;
#if 0
            case QEMU_OPTION_trace_miss:
                trace_cache_miss = 1;
                break;
            case QEMU_OPTION_trace_addr:
                trace_all_addr = 1;
                break;
#endif
            case QEMU_OPTION_tracing:
                if (strcmp(optarg, "off") == 0)
                    tracing = 0;
                else if (strcmp(optarg, "on") == 0 && trace_filename)
                    tracing = 1;
                else {
                    PANIC("Unexpected option to -tracing ('%s')",
                            optarg);
                }
                break;
#if 0
            case QEMU_OPTION_dcache_load_miss:
                dcache_load_miss_penalty = atoi(optarg);
                break;
            case QEMU_OPTION_dcache_store_miss:
                dcache_store_miss_penalty = atoi(optarg);
                break;
#endif
#endif
#ifdef CONFIG_NAND
            case QEMU_OPTION_nand:
                nand_add_dev(optarg);
                break;
#endif
            case QEMU_OPTION_android_ports:
                android_op_ports = (char*)optarg;
                break;

            case QEMU_OPTION_android_port:
                android_op_port = (char*)optarg;
                break;

            case QEMU_OPTION_android_report_console:
                android_op_report_console = (char*)optarg;
                break;

            case QEMU_OPTION_http_proxy:
                op_http_proxy = (char*)optarg;
                break;

            case QEMU_OPTION_charmap:
                op_charmap_file = (char*)optarg;
                break;

            case QEMU_OPTION_android_hw:
                android_op_hwini = (char*)optarg;
                break;

            case QEMU_OPTION_dns_server:
                android_op_dns_server = (char*)optarg;
                break;

            case QEMU_OPTION_radio:
                android_op_radio = (char*)optarg;
                break;

            case QEMU_OPTION_gps:
                android_op_gps = (char*)optarg;
                break;

            case QEMU_OPTION_audio:
                android_op_audio = (char*)optarg;
                break;

            case QEMU_OPTION_cpu_delay:
                android_op_cpu_delay = (char*)optarg;
                break;

            case QEMU_OPTION_show_kernel:
                android_kmsg_init(ANDROID_KMSG_PRINT_MESSAGES);
                break;

#ifdef CONFIG_NAND_LIMITS
            case QEMU_OPTION_nand_limits:
                android_op_nand_limits = (char*)optarg;
                break;
#endif  // CONFIG_NAND_LIMITS

            case QEMU_OPTION_netspeed:
                android_op_netspeed = (char*)optarg;
                break;

            case QEMU_OPTION_netdelay:
                android_op_netdelay = (char*)optarg;
                break;

            case QEMU_OPTION_netfast:
                android_op_netfast = 1;
                break;

            case QEMU_OPTION_tcpdump:
                android_op_tcpdump = (char*)optarg;
                break;

            case QEMU_OPTION_boot_property:
                boot_property_parse_option((char*)optarg);
                break;

            case QEMU_OPTION_lcd_density:
                android_op_lcd_density = (char*)optarg;
                break;

            case QEMU_OPTION_ui_port:
                android_op_ui_port = (char*)optarg;
                break;

            case QEMU_OPTION_ui_settings:
                android_op_ui_settings = (char*)optarg;
                break;

            case QEMU_OPTION_audio_test_out:
                android_audio_test_start_out();
                break;

            case QEMU_OPTION_android_avdname:
                android_op_avd_name = (char*)optarg;
                break;

            case QEMU_OPTION_timezone:
                if (timezone_set((char*)optarg)) {
                    fprintf(stderr, "emulator: it seems the timezone '%s' is not in zoneinfo format\n",
                            (char*)optarg);
                }
                break;

#ifdef CONFIG_MEMCHECK
            case QEMU_OPTION_android_memcheck:
                android_op_memcheck = (char*)optarg;
                /* This will set ro.kernel.memcheck system property
                 * to memcheck's tracing flags. */
                stralloc_add_format(kernel_config, " memcheck=%s", android_op_memcheck);
                break;
#endif // CONFIG_MEMCHECK

            case QEMU_OPTION_snapshot_no_time_update:
                android_snapshot_update_time = 0;
                break;
            }
        }
    }

    /* Initialize character map. */
    if (android_charmap_setup(op_charmap_file)) {
        if (op_charmap_file) {
            PANIC(
                    "Unable to initialize character map from file %s.",
                    op_charmap_file);
        } else {
            PANIC(
                    "Unable to initialize default character map.");
        }
    }

    /* If no data_dir is specified then try to find it relative to the
       executable path.  */
    if (!data_dir) {
        data_dir = find_datadir(argv[0]);
    }
    /* If all else fails use the install patch specified when building.  */
    if (!data_dir) {
        data_dir = CONFIG_QEMU_SHAREDIR;
    }

#ifdef CONFIG_LLVM
    tcg_llvm_ctx = tcg_llvm_initialize();
#endif

#ifdef CONFIG_S2E
    if (!s2e_config_file) {
      fprintf(stderr, "Warning: S2E configuration file was not specified, "
                        "using the default (empty) file\n");
    }
    g_s2e = s2e_initialize(argc, argv, tcg_llvm_ctx,
                           s2e_config_file, s2e_output_dir,
                           s2e_verbose);

    g_s2e_state = s2e_create_initial_state(g_s2e);

    atexit(s2e_cleanup);
    //atexit(tcg_llvm_cleanup);
#endif

    if (!android_op_hwini) {
        PANIC("Missing -android-hw <file> option!");
    }
    hw_ini = iniFile_newFromFile(android_op_hwini);
    if (hw_ini == NULL) {
        PANIC("Could not find %s file.", android_op_hwini);
    }

    androidHwConfig_init(android_hw, 0);
    androidHwConfig_read(android_hw, hw_ini);

    iniFile_free(hw_ini);

    {
        int width  = android_hw->hw_lcd_width;
        int height = android_hw->hw_lcd_height;
        int depth  = android_hw->hw_lcd_depth;

        /* A bit of sanity checking */
        if (width <= 0 || height <= 0    ||
            (depth != 16 && depth != 32) ||
            (((width|height) & 3) != 0)  )
        {
            PANIC("Invalid display configuration (%d,%d,%d)",
                  width, height, depth);
        }
        android_display_width  = width;
        android_display_height = height;
        android_display_bpp    = depth;
    }

#ifdef CONFIG_NAND_LIMITS
    /* Init nand stuff. */
    if (android_op_nand_limits) {
        parse_nand_limits(android_op_nand_limits);
    }
#endif  // CONFIG_NAND_LIMITS

    /* Initialize AVD name from hardware configuration if needed */
    if (!android_op_avd_name) {
        if (android_hw->avd_name && *android_hw->avd_name) {
            android_op_avd_name = android_hw->avd_name;
            VERBOSE_PRINT(init,"AVD Name: %s", android_op_avd_name);
        }
    }

    /* Initialize system partition image */
    {
        char        tmp[PATH_MAX+32];
        const char* sysImage = android_hw->disk_systemPartition_path;
        const char* initImage = android_hw->disk_systemPartition_initPath;
        uint64_t    sysBytes = android_hw->disk_systemPartition_size;

        if (sysBytes == 0) {
            PANIC("Invalid system partition size: %" PRUd64, sysBytes);
        }

        snprintf(tmp,sizeof(tmp),"system,size=0x%" PRUx64, sysBytes);

        if (sysImage && *sysImage) {
            if (filelock_create(sysImage) == NULL) {
                fprintf(stderr,"WARNING: System image already in use, changes will not persist!\n");
                /* If there is no file= parameters, nand_add_dev will create
                 * a temporary file to back the partition image. */
            } else {
                pstrcat(tmp,sizeof(tmp),",file=");
                pstrcat(tmp,sizeof(tmp),sysImage);
            }
        }
        if (initImage && *initImage) {
            if (!path_exists(initImage)) {
                PANIC("Invalid initial system image path: %s", initImage);
            }
            pstrcat(tmp,sizeof(tmp),",initfile=");
            pstrcat(tmp,sizeof(tmp),initImage);
        } else {
            PANIC("Missing initial system image path!");
        }
        nand_add_dev(tmp);
    }

    /* Initialize data partition image */
    {
        char        tmp[PATH_MAX+32];
        const char* dataImage = android_hw->disk_dataPartition_path;
        const char* initImage = android_hw->disk_dataPartition_initPath;
        uint64_t    dataBytes = android_hw->disk_dataPartition_size;

        if (dataBytes == 0) {
            PANIC("Invalid data partition size: %" PRUd64, dataBytes);
        }

        snprintf(tmp,sizeof(tmp),"userdata,size=0x%" PRUx64, dataBytes);

        if (dataImage && *dataImage) {
            if (filelock_create(dataImage) == NULL) {
                fprintf(stderr, "WARNING: Data partition already in use. Changes will not persist!\n");
                /* Note: if there is no file= parameters, nand_add_dev() will
                 *       create a temporary file to back the partition image. */
            } else {
                /* Create the file if needed */
                if (!path_exists(dataImage)) {
                    if (path_empty_file(dataImage) < 0) {
                        PANIC("Could not create data image file %s: %s", dataImage, strerror(errno));
                    }
                }
                pstrcat(tmp, sizeof(tmp), ",file=");
                pstrcat(tmp, sizeof(tmp), dataImage);
            }
        }
        if (initImage && *initImage) {
            pstrcat(tmp, sizeof(tmp), ",initfile=");
            pstrcat(tmp, sizeof(tmp), initImage);
        }
        nand_add_dev(tmp);
    }

    /* Init SD-Card stuff. For Android, it is always hda */
    /* If the -hda option was used, ignore the Android-provided one */
    if (hda_opts == NULL) {
        const char* sdPath = android_hw->hw_sdCard_path;
        if (sdPath && *sdPath) {
            if (!path_exists(sdPath)) {
                fprintf(stderr, "WARNING: SD Card image is missing: %s\n", sdPath);
            } else if (filelock_create(sdPath) == NULL) {
                fprintf(stderr, "WARNING: SD Card image already in use: %s\n", sdPath);
            } else {
                /* Successful locking */
                hda_opts = drive_add(sdPath, HD_ALIAS, 0);
            }
        }
    }

    if (hdb_opts == NULL) {
        const char* spath = android_hw->disk_snapStorage_path;
        if (spath && *spath) {
            if (!path_exists(spath)) {
                PANIC("Snapshot storage file does not exist: %s", spath);
            }
            if (filelock_create(spath) == NULL) {
                PANIC("Snapshot storag already in use: %s", spath);
            }
            hdb_opts = drive_add(spath, HD_ALIAS, 1);
        }
    }

    /* Set the VM's max heap size, passed as a boot property */
    if (android_hw->vm_heapSize > 0) {
        char  tmp[64];
        snprintf(tmp, sizeof(tmp), "%dm", android_hw->vm_heapSize);
        boot_property_add("dalvik.vm.heapsize",tmp);
    }

    /* Initialize net speed and delays stuff. */
    if (android_parse_network_speed(android_op_netspeed) < 0 ) {
        PANIC("invalid -netspeed parameter '%s'",
                android_op_netspeed);
    }

    if ( android_parse_network_latency(android_op_netdelay) < 0 ) {
        PANIC("invalid -netdelay parameter '%s'",
                android_op_netdelay);
    }

    if (android_op_netfast) {
        qemu_net_download_speed = 0;
        qemu_net_upload_speed = 0;
        qemu_net_min_latency = 0;
        qemu_net_max_latency = 0;
    }

    /* Initialize LCD density */
    if (android_hw->hw_lcd_density) {
        long density = android_hw->hw_lcd_density;
        if (density <= 0) {
            PANIC("Invalid hw.lcd.density value: %ld", density);
        }
        hwLcd_setBootProperty(density);
    }

    /* Initialize TCP dump */
    if (android_op_tcpdump) {
        if (qemu_tcpdump_start(android_op_tcpdump) < 0) {
            fprintf(stdout, "could not start packet capture: %s\n", strerror(errno));
        }
    }

    /* Initialize modem */
    if (android_op_radio) {
        CharDriverState*  cs = qemu_chr_open("radio", android_op_radio, NULL);
        if (cs == NULL) {
            PANIC("unsupported character device specification: %s\n"
                        "used -help-char-devices for list of available formats",
                    android_op_radio);
        }
        android_qemud_set_channel( ANDROID_QEMUD_GSM, cs);
    } else if (android_hw->hw_gsmModem != 0 ) {
        if ( android_qemud_get_channel( ANDROID_QEMUD_GSM, &android_modem_cs ) < 0 ) {
            PANIC("could not initialize qemud 'gsm' channel");
        }
    }

    /* Initialize GPS */
    if (android_op_gps) {
        CharDriverState*  cs = qemu_chr_open("gps", android_op_gps, NULL);
        if (cs == NULL) {
            PANIC("unsupported character device specification: %s\n"
                        "used -help-char-devices for list of available formats",
                    android_op_gps);
        }
        android_qemud_set_channel( ANDROID_QEMUD_GPS, cs);
    } else if (android_hw->hw_gps != 0) {
        if ( android_qemud_get_channel( "gps", &android_gps_cs ) < 0 ) {
            PANIC("could not initialize qemud 'gps' channel");
        }
    }

    /* Initialize audio. */
    if (android_op_audio) {
        if ( !audio_check_backend_name( 0, android_op_audio ) ) {
            PANIC("'%s' is not a valid audio output backend. see -help-audio-out",
                    android_op_audio);
        }
    }

    if (android_op_cpu_delay) {
        char*   end;
        long    delay = strtol(android_op_cpu_delay, &end, 0);
        if (end == NULL || *end || delay < 0 || delay > 1000 ) {
            PANIC("option -cpu-delay must be an integer between 0 and 1000" );
        }
        if (delay > 0)
            delay = (1000-delay);

        qemu_cpu_delay = (int) delay;
    }

    if (android_op_dns_server) {
        char*  x = strchr(android_op_dns_server, ',');
        dns_count = 0;
        if (x == NULL)
        {
            if ( add_dns_server( android_op_dns_server ) == 0 )
                dns_count = 1;
        }
        else
        {
            x = android_op_dns_server;
            while (*x) {
                char*  y = strchr(x, ',');

                if (y != NULL) {
                    *y = 0;
                    y++;
                } else {
                    y = x + strlen(x);
                }

                if (y > x && add_dns_server( x ) == 0) {
                    dns_count += 1;
                }
                x = y;
            }
        }
        if (dns_count == 0)
            fprintf( stdout, "### WARNING: will use system default DNS server\n" );
    }

    if (dns_count == 0)
        dns_count = slirp_get_system_dns_servers();
    if (dns_count) {
        stralloc_add_format(kernel_config, " ndns=%d", dns_count);
    }

#ifdef CONFIG_MEMCHECK
    if (android_op_memcheck) {
        memcheck_init(android_op_memcheck);
    }
#endif  // CONFIG_MEMCHECK

    /* Initialize cache partition, if any */
    if (android_hw->disk_cachePartition != 0) {
        char        tmp[PATH_MAX+32];
        const char* partPath = android_hw->disk_cachePartition_path;
        uint64_t    partSize = android_hw->disk_cachePartition_size;

        snprintf(tmp,sizeof(tmp),"cache,size=0x%" PRUx64, partSize);

        if (partPath && *partPath && strcmp(partPath, "<temp>") != 0) {
            if (filelock_create(partPath) == NULL) {
                fprintf(stderr, "WARNING: Cache partition already in use. Changes will not persist!\n");
                /* Note: if there is no file= parameters, nand_add_dev() will
                 *       create a temporary file to back the partition image. */
            } else {
                /* Create the file if needed */
                if (!path_exists(partPath)) {
                    if (path_empty_file(partPath) < 0) {
                        PANIC("Could not create cache image file %s: %s", partPath, strerror(errno));
                    }
                }
                pstrcat(tmp, sizeof(tmp), ",file=");
                pstrcat(tmp, sizeof(tmp), partPath);
            }
        }
        nand_add_dev(tmp);
    }

    /* We always force qemu=1 when running inside QEMU */
    stralloc_add_str(kernel_params, " qemu=1");

    /* We always initialize the first serial port for the android-kmsg
     * character device (used to send kernel messages) */
    serial_hds_add_at(0, "android-kmsg");
    stralloc_add_str(kernel_params, " console=ttyS0");

    /* We always initialize the second serial port for the android-qemud
     * character device as well */
    serial_hds_add_at(1, "android-qemud");
    stralloc_add_str(kernel_params, " android.qemud=ttyS1");

#if defined(CONFIG_KVM) && defined(CONFIG_KQEMU)
    if (kvm_allowed && kqemu_allowed) {
        PANIC(
                "You can not enable both KVM and kqemu at the same time");
    }
#endif

    machine->max_cpus = machine->max_cpus ?: 1; /* Default to UP */
    if (smp_cpus > machine->max_cpus) {
        PANIC("Number of SMP cpus requested (%d), exceeds max cpus "
                "supported by machine `%s' (%d)", smp_cpus,  machine->name,
                machine->max_cpus);
    }

    if (display_type == DT_NOGRAPHIC) {
       if (serial_device_index == 0)
           serial_devices[0] = "stdio";
       if (parallel_device_index == 0)
           parallel_devices[0] = "null";
       if (strncmp(monitor_device, "vc", 2) == 0)
           monitor_device = "stdio";
    }

#ifndef _WIN32
    if (daemonize) {
	pid_t pid;

	if (pipe(fds) == -1) {
	    PANIC("Unable to aquire pidfile");
	}

	pid = fork();
	if (pid > 0) {
	    uint8_t status;
	    ssize_t len;

	    close(fds[1]);

	again:
            len = read(fds[0], &status, 1);
            if (len == -1 && (errno == EINTR))
                goto again;

            if (len != 1) {
                PANIC("Error when aquiring pidfile");
            }
            else if (status == 1) {
                PANIC("Could not acquire pidfile");
            } else {
                QEMU_EXIT(0);
            }
	} else if (pid < 0) {
        PANIC("Unable to daemonize");
	}

	setsid();

	pid = fork();
	if (pid > 0) {
	    QEMU_EXIT(0);
	} else if (pid < 0) {
         PANIC("Could not acquire pid file");
	}

	umask(027);

        signal(SIGTSTP, SIG_IGN);
        signal(SIGTTOU, SIG_IGN);
        signal(SIGTTIN, SIG_IGN);
    }

    if (pid_file && qemu_create_pidfile(pid_file) != 0) {
        if (daemonize) {
            uint8_t status = 1;
            int ret;
            do {
                ret = write(fds[1], &status, 1);
            } while (ret < 0 && errno == EINTR);
            PANIC("Could not acquire pid file");
        } else {
            PANIC("Could not acquire pid file");
        }
    }
#endif

#ifdef CONFIG_KQEMU
    if (smp_cpus > 1)
        kqemu_allowed = 0;
#endif
    if (qemu_init_main_loop()) {
        PANIC("qemu_init_main_loop failed");
    }

    if (kernel_filename == NULL) {
        kernel_filename = android_hw->kernel_path;
    }
    if (initrd_filename == NULL) {
        initrd_filename = android_hw->disk_ramdisk_path;
    }

    linux_boot = (kernel_filename != NULL);
    net_boot = (boot_devices_bitmap >> ('n' - 'a')) & 0xF;

    if (!linux_boot && *kernel_cmdline != '\0') {
        PANIC("-append only allowed with -kernel option");
    }

    if (!linux_boot && initrd_filename != NULL) {
        PANIC("-initrd only allowed with -kernel option");
    }

    /* boot to floppy or the default cd if no hard disk defined yet */
    if (!boot_devices[0]) {
        boot_devices = "cad";
    }
    setvbuf(stdout, NULL, _IOLBF, 0);

    if (init_timer_alarm() < 0) {
        PANIC("could not initialize alarm timer");
    }
    configure_icount(icount_option);

    /* init network clients */
    if (nb_net_clients == 0) {
        /* if no clients, we use a default config */
        net_clients[nb_net_clients++] = "nic";
#ifdef CONFIG_SLIRP
        net_clients[nb_net_clients++] = "user";
#endif
    }

    for(i = 0;i < nb_net_clients; i++) {
        if (net_client_parse(net_clients[i]) < 0) {
            PANIC("Unable to parse net clients");
        }
    }
    net_client_check();

#ifdef TARGET_I386
    /* XXX: this should be moved in the PC machine instantiation code */
    if (net_boot != 0) {
        int netroms = 0;
	for (i = 0; i < nb_nics && i < 4; i++) {
	    const char *model = nd_table[i].model;
	    char buf[1024];
            char *filename;
            if (net_boot & (1 << i)) {
                if (model == NULL)
                    model = "ne2k_pci";
                snprintf(buf, sizeof(buf), "pxe-%s.bin", model);
                filename = qemu_find_file(QEMU_FILE_TYPE_BIOS, buf);
                if (filename && get_image_size(filename) > 0) {
                    if (nb_option_roms >= MAX_OPTION_ROMS) {
                        PANIC("Too many option ROMs");
                    }
                    option_rom[nb_option_roms] = qemu_strdup(buf);
                    nb_option_roms++;
                    netroms++;
                }
                if (filename) {
                    qemu_free(filename);
                }
            }
	}
	if (netroms == 0) {
	    PANIC("No valid PXE rom found for network device");
	}
    }
#endif

    /* init the bluetooth world */
    for (i = 0; i < nb_bt_opts; i++)
        if (bt_parse(bt_opts[i])) {
            PANIC("Unable to parse bluetooth options");
        }

    /* init the memory */
    if (ram_size == 0) {
        ram_size = android_hw->hw_ramSize * 1024LL * 1024;
        if (ram_size == 0) {
            ram_size = DEFAULT_RAM_SIZE * 1024 * 1024;
        }
    }

#ifdef CONFIG_KQEMU
    /* FIXME: This is a nasty hack because kqemu can't cope with dynamic
       guest ram allocation.  It needs to go away.  */
    if (kqemu_allowed) {
        kqemu_phys_ram_size = ram_size + 8 * 1024 * 1024 + 4 * 1024 * 1024;
        kqemu_phys_ram_base = qemu_vmalloc(kqemu_phys_ram_size);
        if (!kqemu_phys_ram_base) {
            PANIC("Could not allocate physical memory");
        }
    }
#endif

#ifndef _WIN32
    init_qemu_clear_logs_sig();
#endif

    /* init the dynamic translator */
    cpu_exec_init_all(tb_size * 1024 * 1024);

    bdrv_init();

    /* we always create the cdrom drive, even if no disk is there */
#if 0
    if (nb_drives_opt < MAX_DRIVES)
        drive_add(NULL, CDROM_ALIAS);

    /* we always create at least one floppy */

    if (nb_drives_opt < MAX_DRIVES)
        drive_add(NULL, FD_ALIAS, 0);
    /* we always create one sd slot, even if no card is in it */

    if (1) {
        drive_add(NULL, SD_ALIAS);
    }
#endif

    /* open the virtual block devices */
    if (snapshot)
        qemu_opts_foreach(qemu_find_opts("drive"), drive_enable_snapshot, NULL, 0);
    if (qemu_opts_foreach(qemu_find_opts("drive"), drive_init_func, &machine->use_scsi, 1) != 0)
        exit(1);

    //register_savevm("timer", 0, 2, timer_save, timer_load, &timers_state);
    register_savevm_live("ram", 0, 3, ram_save_live, NULL, ram_load, NULL);

#ifndef _WIN32
    /* must be after terminal init, SDL library changes signal handlers */
    sighandler_setup();
#endif

    /* Maintain compatibility with multiple stdio monitors */
    if (!strcmp(monitor_device,"stdio")) {
        for (i = 0; i < MAX_SERIAL_PORTS; i++) {
            const char *devname = serial_devices[i];
            if (devname && !strcmp(devname,"mon:stdio")) {
                monitor_device = NULL;
                break;
            } else if (devname && !strcmp(devname,"stdio")) {
                monitor_device = NULL;
                serial_devices[i] = "mon:stdio";
                break;
            }
        }
    }

    if (nb_numa_nodes > 0) {
        int i;

        if (nb_numa_nodes > smp_cpus) {
            nb_numa_nodes = smp_cpus;
        }

        /* If no memory size if given for any node, assume the default case
         * and distribute the available memory equally across all nodes
         */
        for (i = 0; i < nb_numa_nodes; i++) {
            if (node_mem[i] != 0)
                break;
        }
        if (i == nb_numa_nodes) {
            uint64_t usedmem = 0;

            /* On Linux, the each node's border has to be 8MB aligned,
             * the final node gets the rest.
             */
            for (i = 0; i < nb_numa_nodes - 1; i++) {
                node_mem[i] = (ram_size / nb_numa_nodes) & ~((1 << 23UL) - 1);
                usedmem += node_mem[i];
            }
            node_mem[i] = ram_size - usedmem;
        }

        for (i = 0; i < nb_numa_nodes; i++) {
            if (node_cpumask[i] != 0)
                break;
        }
        /* assigning the VCPUs round-robin is easier to implement, guest OSes
         * must cope with this anyway, because there are BIOSes out there in
         * real machines which also use this scheme.
         */
        if (i == nb_numa_nodes) {
            for (i = 0; i < smp_cpus; i++) {
                node_cpumask[i % nb_numa_nodes] |= 1 << i;
            }
        }
    }

    if (kvm_enabled()) {
        int ret;

        ret = kvm_init(smp_cpus);
        if (ret < 0) {
            PANIC("failed to initialize KVM");
        }
    }

    if (monitor_device) {
        monitor_hd = qemu_chr_open("monitor", monitor_device, NULL);
        if (!monitor_hd) {
            PANIC("qemu: could not open monitor device '%s'",
                              monitor_device);
        }
    }

    for(i = 0; i < MAX_SERIAL_PORTS; i++) {
        serial_hds_add(serial_devices[i]);
    }

    for(i = 0; i < MAX_PARALLEL_PORTS; i++) {
        const char *devname = parallel_devices[i];
        if (devname && strcmp(devname, "none")) {
            char label[32];
            snprintf(label, sizeof(label), "parallel%d", i);
            parallel_hds[i] = qemu_chr_open(label, devname, NULL);
            if (!parallel_hds[i]) {
                PANIC("qemu: could not open parallel device '%s'",
                        devname);
            }
        }
    }

    for(i = 0; i < MAX_VIRTIO_CONSOLES; i++) {
        const char *devname = virtio_consoles[i];
        if (devname && strcmp(devname, "none")) {
            char label[32];
            snprintf(label, sizeof(label), "virtcon%d", i);
            virtcon_hds[i] = qemu_chr_open(label, devname, NULL);
            if (!virtcon_hds[i]) {
                PANIC("qemu: could not open virtio console '%s'",
                        devname);
            }
        }
    }


#ifdef CONFIG_S2E
    s2e_on_device_registration(g_s2e);
#else
//    void fake_register_devices(fake_pci_t *fake);
//    fake_register_devices(&g_fake_pci);
#endif

    module_call_init(MODULE_INIT_DEVICE);


#ifdef CONFIG_TRACE
    if (trace_filename) {
        trace_init(trace_filename);
#if 0
        // We don't need the dcache code until we can get load and store tracing
        // working again.
        dcache_init(dcache_size, dcache_ways, dcache_line_size,
                    dcache_replace_policy, dcache_load_miss_penalty,
                    dcache_store_miss_penalty);
#endif
        fprintf(stderr, "-- When done tracing, exit the emulator. --\n");
    }
#endif

    /* Combine kernel command line passed from the UI with parameters
     * collected during initialization.
     *
     * The order is the following:
     * - parameters from the hw configuration (kernel.parameters)
     * - additionnal parameters from options (e.g. -memcheck)
     * - the -append parameters.
     */
    {
        const char* kernel_parameters;

        if (android_hw->kernel_parameters) {
            stralloc_add_c(kernel_params, ' ');
            stralloc_add_str(kernel_params, android_hw->kernel_parameters);
        }

        /* If not empty, kernel_config always contains a leading space */
        stralloc_append(kernel_params, kernel_config);

        if (*kernel_cmdline) {
            stralloc_add_c(kernel_params, ' ');
            stralloc_add_str(kernel_params, kernel_cmdline);
        }

        /* Remove any leading/trailing spaces */
        stralloc_strip(kernel_params);

        kernel_parameters = stralloc_cstr(kernel_params);
        VERBOSE_PRINT(init, "Kernel parameters: %s", kernel_parameters);

        machine->init(ram_size,
                      boot_devices,
                      kernel_filename,
                      kernel_parameters,
                      initrd_filename,
                      cpu_model);

        stralloc_reset(kernel_params);
        stralloc_reset(kernel_config);
    }


    for (env = first_cpu; env != NULL; env = env->next_cpu) {
        for (i = 0; i < nb_numa_nodes; i++) {
            if (node_cpumask[i] & (1 << env->cpu_index)) {
                env->numa_node = i;
            }
        }
    }

    current_machine = machine;

    /* Set KVM's vcpu state to qemu's initial CPUState. */
    if (kvm_enabled()) {
        int ret;

        ret = kvm_sync_vcpus();
        if (ret < 0) {
            PANIC("failed to initialize vcpus");
        }
    }

    /* init USB devices */
    if (usb_enabled) {
        for(i = 0; i < usb_devices_index; i++) {
            if (usb_device_add(usb_devices[i], 0) < 0) {
                fprintf(stderr, "Warning: could not add USB device %s\n",
                        usb_devices[i]);
            }
        }
    }

    /* just use the first displaystate for the moment */
    ds = get_displaystate();

    /* Initialize display from the command line parameters. */
    android_display_reset(ds,
                          android_display_width,
                          android_display_height,
                          android_display_bpp);

    if (display_type == DT_DEFAULT) {
#if defined(CONFIG_SDL) || defined(CONFIG_COCOA)
        display_type = DT_SDL;
#else
        display_type = DT_VNC;
        vnc_display = "localhost:0,to=99";
        show_vnc_port = 1;
#endif
    }


    switch (display_type) {
    case DT_NOGRAPHIC:
        break;
#if defined(CONFIG_CURSES)
    case DT_CURSES:
        curses_display_init(ds, full_screen);
        break;
#endif
#if defined(CONFIG_SDL) && !defined(CONFIG_STANDALONE_CORE)
    case DT_SDL:
        sdl_display_init(ds, full_screen, no_frame);
        break;
#elif defined(CONFIG_COCOA)
    case DT_SDL:
        cocoa_display_init(ds, full_screen);
        break;
#elif defined(CONFIG_STANDALONE_CORE)
    case DT_SDL:
        coredisplay_init(ds);
        break;
#endif
    case DT_VNC:
        vnc_display_init(ds);
        if (vnc_display_open(ds, vnc_display) < 0) {
            PANIC("Unable to initialize VNC display");
        }

        if (show_vnc_port) {
            printf("VNC server running on `%s'\n", vnc_display_local_addr(ds));
        }
        break;
    default:
        break;
    }
    dpy_resize(ds);

    dcl = ds->listeners;
    while (dcl != NULL) {
        if (dcl->dpy_refresh != NULL) {
            ds->gui_timer = qemu_new_timer(rt_clock, gui_update, ds);
            qemu_mod_timer(ds->gui_timer, qemu_get_clock(rt_clock));
        }
        dcl = dcl->next;
    }

    if (display_type == DT_NOGRAPHIC || display_type == DT_VNC) {
        nographic_timer = qemu_new_timer(rt_clock, nographic_update, NULL);
        qemu_mod_timer(nographic_timer, qemu_get_clock(rt_clock));
    }

    text_consoles_set_display(ds);
    qemu_chr_initial_reset();

    if (monitor_device && monitor_hd)
        monitor_init(monitor_hd, MONITOR_USE_READLINE | MONITOR_IS_DEFAULT);

    for(i = 0; i < MAX_SERIAL_PORTS; i++) {
        const char *devname = serial_devices[i];
        if (devname && strcmp(devname, "none")) {
            if (strstart(devname, "vc", 0))
                qemu_chr_printf(serial_hds[i], "serial%d console\r\n", i);
        }
    }

    for(i = 0; i < MAX_PARALLEL_PORTS; i++) {
        const char *devname = parallel_devices[i];
        if (devname && strcmp(devname, "none")) {
            if (strstart(devname, "vc", 0))
                qemu_chr_printf(parallel_hds[i], "parallel%d console\r\n", i);
        }
    }

    for(i = 0; i < MAX_VIRTIO_CONSOLES; i++) {
        const char *devname = virtio_consoles[i];
        if (virtcon_hds[i] && devname) {
            if (strstart(devname, "vc", 0))
                qemu_chr_printf(virtcon_hds[i], "virtio console%d\r\n", i);
        }
    }

    if (gdbstub_dev && gdbserver_start(gdbstub_dev) < 0) {
        PANIC("qemu: could not open gdbserver on device '%s'",
                gdbstub_dev);
    }

    /* call android-specific setup function */
    android_emulation_setup();

#if !defined(CONFIG_STANDALONE_CORE)
    // For the standalone emulator (UI+core in one executable) we need to
    // set the window title here.
    android_emulator_set_base_port(android_base_port);
#endif

    if (loadvm)
        do_loadvm(cur_mon, loadvm);

#ifdef CONFIG_S2E
   s2e_init_device_state(g_s2e_state);
   s2e_init_timers(g_s2e);
#endif

    if (incoming) {
        autostart = 0; /* fixme how to deal with -daemonize */
        qemu_start_incoming_migration(incoming);
    }

    if (autostart)
        vm_start();

#ifndef _WIN32
    if (daemonize) {
	uint8_t status = 0;
	ssize_t len;

    again1:
	len = write(fds[1], &status, 1);
	if (len == -1 && (errno == EINTR))
	    goto again1;

	if (len != 1) {
	    PANIC("Unable to daemonize");
	}

        if (chdir("/")) {
            perror("not able to chdir to /");
            PANIC("not able to chdir to /");
        }
	TFR(fd = open("/dev/null", O_RDWR));
	if (fd == -1)
	    PANIC("open(\"/dev/null\") failed: %s", errno_str);
    }

    if (run_as) {
        pwd = getpwnam(run_as);
        if (!pwd) {
            PANIC("User \"%s\" doesn't exist", run_as);
        }
    }

    if (chroot_dir) {
        if (chroot(chroot_dir) < 0) {
            PANIC("chroot failed");
        }
        if (chdir("/")) {
            perror("not able to chdir to /");
            PANIC("not able to chdir to /");
        }
    }

    if (run_as) {
        if (setgid(pwd->pw_gid) < 0) {
            PANIC("Failed to setgid(%d)", pwd->pw_gid);
        }
        if (setuid(pwd->pw_uid) < 0) {
            PANIC("Failed to setuid(%d)", pwd->pw_uid);
        }
        if (setuid(0) != -1) {
            PANIC("Dropping privileges failed");
        }
    }

    if (daemonize) {
        dup2(fd, 0);
        dup2(fd, 1);
        dup2(fd, 2);

        close(fd);
    }
#endif

#ifdef CONFIG_S2E
    /** Normal memory was registered by pc.c. Here we want to
        register all other memory as a shared concrete memory */
    /*
    ram_addr_t addr;
    for(addr = 0; addr < last_ram_offset; addr += TARGET_PAGE_SIZE) {
        uint64_t host_address = (uint64_t) qemu_get_ram_ptr(addr);
        if(!s2e_is_ram_registered(g_s2e, g_s2e_state, host_address)) {
            s2e_register_ram(g_s2e, g_s2e_state, addr, TARGET_PAGE_SIZE,
                             host_address, 1);
        }
    }
    */
    s2e_initialize_execution(g_s2e, g_s2e_state, execute_always_klee);
    s2e_register_dirty_mask(g_s2e, g_s2e_state, (uint64_t)phys_ram_dirty, last_ram_offset >> TARGET_PAGE_BITS);
#endif

#ifdef CONFIG_ANDROID
    // This will notify the UI that the core is successfuly initialized
    android_core_init_completed();
#endif  // CONFIG_ANDROID

    main_loop();
    quit_timers();
    net_cleanup();
    android_emulation_teardown();

#ifdef CONFIG_S2E
    s2e_cleanup();
#endif

#ifdef CONFIG_LLVM
    tcg_llvm_cleanup();
#endif

    return 0;
}

void
android_emulation_teardown(void)
{
    android_charmap_done();
}
