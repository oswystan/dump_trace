/*
 **************************************************************************************
 *       Filename:  debug_trace.c
 *    Description:   source file
 *
 *        Version:  1.0
 *        Created:  2017-10-04 21:04:13
 *
 *       Revision:  initial draft;
 **************************************************************************************
 */
#define LOG_TAG "dumptrace"
#include "log.h"

#include <string.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/prctl.h>
#include <libunwind.h>
#include <errno.h>

#define gettid() syscall(SYS_gettid)

typedef struct _proc_map_t {
    void* start;
    void* end;
    char* name;
} proc_map_t;

static proc_map_t* g_maps[1024];

typedef struct _sig_name_t {
    int sig;
    const char* name;
} sig_name_t;
#define FMT_SIG(x) {sig: x, name: #x}

static sig_name_t g_sigs[] = {
    FMT_SIG(SIGSEGV),
    FMT_SIG(SIGABRT),
    FMT_SIG(SIGBUS),
    FMT_SIG(SIGFPE),
    FMT_SIG(SIGILL),
    FMT_SIG(SIGSYS),
    FMT_SIG(SIGTRAP),
};

static void parse_proc_maps(int pid) {
    memset(g_maps, 0x00, sizeof(g_maps));
    int cnt = 0;
    char fn[64];
    char buf[1024];
    sprintf(fn, "/proc/%d/maps", pid);
    FILE* fp = fopen(fn, "r");
    if (!fp) {
        return;
    }
    uint64_t start;
    uint64_t end;
    int name_pos;
    while (fgets(buf, sizeof(buf), fp)) {
        buf[strlen(buf)-1] = '\0';
        if (sscanf(buf, "%" SCNx64 "-%" SCNx64 " %*4s %*x %*x:%*x %*d %n",
                    &start, &end, &name_pos) != 2) {
            loge("error");
            continue;
        }
        proc_map_t* p = (proc_map_t*)malloc(sizeof(proc_map_t));
        if (!p) {
            loge("fail to alloc buf");
            break;
        }
        p->start = (void*)start;
        p->end = (void*)end;
        p->name = strdup(buf+name_pos);
        g_maps[cnt] = p;
        ++cnt;
        if (cnt >= sizeof(g_maps)/sizeof(g_maps[0])-1) {
            break;
        }
    }
    fclose(fp);
}

static const char* get_map_name(void* p) {
    int i = 0;
    while(g_maps[i]) {
        if (g_maps[i]->start <= p && g_maps[i]->end >= p) {
            return g_maps[i]->name;
        }
        ++i;
    }
    return "unknown map";
}

static const char* get_signame(int sig) {
    const char* name = "<unkown signal>";
    unsigned int i = 0;
    for (i = 0; i < sizeof(g_sigs)/sizeof(g_sigs[0]); i++) {
        if (sig == g_sigs[i].sig) {
            name = g_sigs[i].name;
        }
    }
    return name;
}

static void format_sig_summary(int sig, siginfo_t* info) {
    char tname[128];
    if(0 != prctl(PR_GET_NAME, (unsigned long)tname, 0, 0, 0)) {
        sprintf(tname, "<unknown thread name>");
    } else {
        tname[sizeof(tname)-1] = '\0';
    }
    logd("fatal signal: %d (%s) received, fault addr: %p", sig, get_signame(sig), info->si_addr);
    logd("pid: %d, tid: %ld, name: %s", getpid(), gettid(), tname);
}

static void dump_backtrace(unw_context_t* ctx)
{
    unw_cursor_t    cursor;
    unw_init_local(&cursor, ctx);
    int level = 0;
    int ret = 1;
    while (ret > 0) {
        unw_word_t  offset, pc;
        char        fname[64];
        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        fname[0] = '\0';
        (void) unw_get_proc_name(&cursor, fname, sizeof(fname), &offset);
        logd("#%02d pc 0x%012llx: %s (%s+0x%lx)", level++, (unsigned long long)pc, get_map_name((void*)pc),
                fname, (unsigned long)offset);
        if (0 == pc || strcmp("main", fname) == 0) {
            break;
        }
        ret = unw_step(&cursor);
    }
}
static void sig_handler(int sig, siginfo_t *info, void *data ) {
    parse_proc_maps(getpid());
    logd("-----------------------------------------------");
    format_sig_summary(sig, info);
    dump_backtrace((unw_context_t*) data);
    logd("-----------------------------------------------");
    exit(-1);
}

#define __ctors __attribute__((constructor))
void __ctors debug_trace_init() {
    struct sigaction sa;
    memset(&sa, 0x00, sizeof(sa));
    sa.sa_sigaction = sig_handler;
    sa.sa_flags = SA_SIGINFO | SA_RESTART;

    unsigned int i = 0;
    for (i = 0; i < sizeof(g_sigs)/sizeof(g_sigs[0]); i++) {
        if (sigaction(g_sigs[i].sig, &sa, NULL) != 0) {
            return;
        }
    }
}

/********************************** END **********************************************/

