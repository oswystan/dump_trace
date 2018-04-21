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
#include <limits.h>

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
    unsigned int cnt = 0;
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
        if (sscanf(buf, "%lx-%lx %*4s %*x %*x:%*x %*d %n",
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

static const proc_map_t* get_mapinfo(void* p) {
    int i = 0;
    while(g_maps[i]) {
        if (g_maps[i]->start <= p && g_maps[i]->end >= p) {
            return g_maps[i];
        }
        ++i;
    }
    return NULL;
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

static void dump_regs(unw_cursor_t* cursor) {
    struct reg_desc_t {
        unw_word_t val;
        char name[8];
    };

    unw_regnum_t reg = 0;
    struct reg_desc_t regs[17];
    while (reg < (unw_regnum_t)(sizeof(regs)/sizeof(regs[0]))) {
        unw_get_reg(cursor, reg, &regs[reg].val);
        sprintf(regs[reg].name, "%s", unw_regname(reg));
        reg++;
    }
    char line[256];
    uint32_t i=0;
    memset(line, 0x00, sizeof(line));
    while(i<(sizeof(regs)/sizeof(regs[0]))) {
        if (line[0] == '\0') {
            sprintf(line, "%3s:0x%012lx", regs[i].name, regs[i].val);
        } else {
            sprintf(line, "%s  %3s:0x%012lx", line, regs[i].name, regs[i].val);
        }
        i++;
        if (i%3 == 0 && i != 0) {
            logd("%s", line);
            memset(line, 0x00, sizeof(line));
        }
    }
    if (line[0]) {
        logd("%s", line);
    }
}

static const char* get_executable_name() {
    static char fn[PATH_MAX];
    readlink("/proc/self/exe", fn, sizeof(fn));
    return fn;
}

#pragma GCC diagnostic ignored "-Wformat"
void dump_backtrace(unw_context_t* ctx)
{
    unw_cursor_t    cursor;
    unw_init_local(&cursor, ctx);
    dump_regs(&cursor);
    int level = 0;
    int ret = 1;
    loge("");
    loge("backtrace:");
    while (ret > 0) {
        unw_word_t  offset, pc;
        char        fname[256];
        unw_get_reg(&cursor, UNW_REG_IP, &pc);
        fname[0] = '\0';
        (void) unw_get_proc_name(&cursor, fname, sizeof(fname), &offset);
        if (pc == 0) {
            break;
        }
        const proc_map_t* map = get_mapinfo((void*)pc);
        char* rel_pc = (char*)pc;
        if (map && strcmp(map->name, get_executable_name()) != 0) {
            rel_pc = (char*)(rel_pc-(char*)map->start);
        }
        logd("#%02d pc %012p %s (%s+0x%lx)", level++, rel_pc, map->name,
                fname, (unsigned long)offset);
        if (strcmp("main", fname) == 0) {
            break;
        }
        ret = unw_step(&cursor);
    }
}

static void resend_signal(siginfo_t* info) {
    signal(info->si_signo, SIG_DFL);
    syscall(SYS_rt_tgsigqueueinfo, getpid(), gettid(), info->si_signo, info);
}
#pragma GCC diagnostic warning "-Wformat"
static void sig_handler(int sig, siginfo_t *info, void *data ) {
    parse_proc_maps(getpid());
    logd("*** *** *** *** *** *** *** *** *** *** *** ***");
    format_sig_summary(sig, info);
    dump_backtrace((unw_context_t*) data);
    logd("*** *** *** *** *** *** *** *** *** *** *** ***");
    resend_signal(info);
    //exit(0);
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

