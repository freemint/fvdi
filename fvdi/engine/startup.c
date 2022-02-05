/*
 * fVDI startup
 *
 * Copyright 1999-2003, Johan Klockars
 * This software is licensed under the GNU General Public License.
 * Please, see LICENSE.TXT for further information.
 */

#include "fvdi.h"
#include <stddef.h>
#include "function.h"
#include "relocate.h"
#include "utility.h"
#include "os.h"
#include "globals.h"
#include "calamus.h"
#include "stdio.h"

#define SYSNAME "fvdi.sys"

#define VERSION	0x0970
#define BETA	3
#define VERmaj	(VERSION >> 12)
#define VERmin	(((VERSION & 0x0f00) >> 8) * 100 + ((VERSION & 0x00f0) >> 4) * 10 + (VERSION & 0x000f))

#define fvdi_magic	1969
#define ACTIVE		1		/* fVDI installed */
#define BOOTED		2		/* fVDI can't be removed */

#define MAX_NVDI_SEARCH		100	/* Words of forward search from the initial NVDI dispatcher */
#define MAX_NVDI_DISTANCE	10000	/* Allowed distance between the two dispatchers */


/*
 * Global variables
 */

long basepage;
static char fake_bp[256];

short old_gdos;
void *linea_vars;

static long CDECL remove_fvdi(void);
static long CDECL setup_fvdi(unsigned long, long);

static int nvdi_patch(void);

/* called from startup assembler code */
long CDECL startup(void);
void CDECL recheck_mtask(void);
void CDECL vdi_debug(VDIpars * pars, long pc);
void CDECL trap2_debug(long type, VDIpars *pars, unsigned short *stack);
void CDECL lineA_debug(long opcode, long pc);



struct fVDI_cookie {
    short version;
    short flags;
    long CDECL(*remove)(void);
    long CDECL(*setup)(unsigned long type, long value);
    struct fVDI_log *log;
};	/* cookie = {VERSION, 0, remove_fvdi, setup_fvdi, &fvdi_log}; */

struct FSMC_cookie {
    long type;
    short versions;
    short quality;
};	/* fsmc_cookie = {"_FSM", 0x0100, -1}; */

struct NVDI_cookie {
    short version;  /* 0x0502 for version 5.02   */
    long  date;     /* 0x18061990 for 1990-06-18 */
    short flags;    /* 9*reserved, alert, reserved, linea, mouse, gemdos, error, gdos */
};

static struct Readable_data {
    struct fVDI_cookie cookie;
    struct FSMC_cookie fsmc_cookie;
    struct NVDI_cookie nvdi_cookie;
    struct DCSD_cookie dcsd_cookie;
} *readable = 0;

struct Super_data *super = 0;

static long old_eddi = 0;
static long old_fsmc = 0;
static long old_nvdi = 0;


/* Stack should probably be allocated dynamically */
static char vdi_stack[8192];   /* Used to be 2048, but Standa wants 8192 for FreeType */
char *vdi_stack_top = &vdi_stack[sizeof(vdi_stack)];
long vdi_stack_size = sizeof(vdi_stack);


static long bconout_hook(void)
{
    void **bconout_vec = (void **) 0x586;
    bconout_address = *bconout_vec;
    *bconout_vec = bconout_stub;

    return 0;
}

static long bconout_unhook(void)
{
    void **bconout_vec = (void **) 0x586;
    *bconout_vec = bconout_address;
    bconout_address = 0;

    return 0;
}


/*
 * Top level fVDI initialization
 */
long startup(void)
{
    Virtual *base_vwk, *first_vwk;
    List *element;
    Driver *driver;

    kputs("\n");

    if (!init_utility())
    {
        /* Make the utility routines ready for use */
        error("Error while initializing utility routines.", NULL);
        return 0;
    }

    if ((super = fmalloc(sizeof(struct Super_data), 0x4033)) == NULL)
    {
        error("Could not allocate space for supervisor accessible data.", 0);
        return 0;
    }
    super->fvdi_log.active = 0;
    super->fvdi_log.start = 0;
    super->fvdi_log.current = 0;
    super->fvdi_log.end = 0;

    if ((readable = fmalloc(sizeof(struct Readable_data), 0x4043)) == NULL)
    {
        error("Could not allocate space for world-readable data.", NULL);
        return 0;
    }

    if ((base_vwk = initialize_vdi()) == NULL)
    {
        /* Setup initial real and virtual workstations */
        error("Error while initializing VDI.", NULL);
        return 0;
    }
    screen_wk = base_vwk->real_address;

    if (!load_prefs(base_vwk, SYSNAME))
    {
        /* Load preferences (and load all fonts and device drivers specified) */
        error("Aborted while loading preferences.", NULL);
        return 0;
    }

    readable->cookie.version = VERSION;
    readable->cookie.flags = 0;
    readable->cookie.remove = remove_fvdi;
    readable->cookie.setup = setup_fvdi;
    readable->cookie.log = &super->fvdi_log;
    if (!speedo_cookie)
    {
        readable->fsmc_cookie.type = str2long("_FNT");  /* Was _FSM */
        readable->fsmc_cookie.versions = 0x0100;
    } else
    {
        readable->fsmc_cookie.type = str2long("_SPD");
        readable->fsmc_cookie.versions = speedo_cookie;
        readable->fsmc_cookie.quality = -1;
    }
    if (nvdi_cookie)
    {
        readable->nvdi_cookie.version = nvdi_cookie;
        readable->nvdi_cookie.date = 0x13052005L;
        readable->nvdi_cookie.flags = 0x0001;  /* GDOS support */
    }
    if (calamus_cookie)
    {
        calamus_initialize_cookie(&readable->dcsd_cookie, calamus_cookie);
    }

    if (debug)
    {
        /* Set up log table if asked for */
        if ((super->fvdi_log.start = malloc(log_size * sizeof(long))) != NULL)
        {
            super->fvdi_log.active = 1;
            super->fvdi_log.current = super->fvdi_log.start;
            super->fvdi_log.end = &super->fvdi_log.start[log_size - 8];
        }
    }

    if (!initialize_pool(block_size, blocks))
    {
        /* Initialize the internal memory pool */
        error("Error while initializing memory pool.", 0);
        return 0;
    }

    if (remove_xbra(34 * 4, "fVDI") && debug)		/* fVDI might already be installed */
    {
        PUTS("Removing previous XBRA.\n");
    }

    sub_call = get_sub_call();

    old_gdos = vq_gdos();

    if (nvdifix && nvdi_patch() && debug)
    {
        PUTS("Patching NVDI dispatcher\n");
    }

#if 0 /* currently does not work */
    if (booted && !singlebend)
    {
        trap2_address = (void (*)(void)) Setexc(34, trap2_temp);	/* Install a temporary trap handler if real boot (really necessary?) */
    } else
#endif
    {
        vdi_address = (void (*)(void)) Setexc(34, vdi_dispatch);	/*   otherwise the dispatcher directly */
    }

    trap14_address = (void (*)(void)) Setexc(46, trap14);	/* Install an XBIOS handler */

    lineA_address = (void (*)(void)) Setexc(10, lineA);		/* Install a LineA handler */

    if (bconout)
    {
        Supexec(bconout_hook); /* cannot use Setexc() as the 0x586 is not divisible by 4 */
    }

    kprintf("fVDI v%d.%d", VERmaj, VERmin);
    if (BETA)
    {
        kprintf("beta%d", BETA);
    }
    access->funcs.puts(sizeof(int) == sizeof(short) ? "-16bit" : "-32bit");
#ifdef __GNUC__
    access->funcs.puts("-gcc");
#else
#ifdef __PUREC__
    access->funcs.puts("-purec");
#endif
#endif
#ifdef FT2
    access->funcs.puts("-FT-");
    access->funcs.puts(ft2_version);
#endif
#ifdef GIT_COMMIT
    access->funcs.puts(" (" GIT_COMMIT ")");
#endif
    access->funcs.puts(" now installed.\n");

    if (debug)
    {
        PRINTF(("fVDI engine Text: $%08lx   Data: $%08lx   Bss: $%08lx\n", (long) init, (long) data_start, (long) bss_start));
        if (super->fvdi_log.active)
        {
            PRINTF(("Logging at $%08lx\n", (long) &super->fvdi_log));
        }
    }


    /*
     * During the load process, all installed drivers were linked onto a list.
     * Go through the list and call all post install driver initialization routines.
     */

    if (debug)
    {
        PUTS("Post install initialization of drivers\n");
    }

    first_vwk = 0;
    element = driver_list;
    while (element)
    {
        driver = (Driver *)element->value;
        if (driver->module.flags & MOD_RESIDENT)
        {
            if (debug)
            {
                PRINTF((" %s at $%08lx\n", driver->module.name, (long) driver->module.initialize));
            }
            if (driver->module.initialize(driver->default_vwk) == 0)
            {
                /* Driver that fails initialization should be removed! */
                error("Failed driver initialization of ", driver->module.name);
            } else
            {
                if (!first_vwk)
                    first_vwk = driver->default_vwk;
            }
        }
        element = element->next;
    }

    if (first_vwk == 0)
    {
        error("no suitable screen driver found", 0);
        remove_xbra(34 * 4, "fVDI");        /* Trap #2 handler */
        remove_xbra(46 * 4, "fVDI");        /* Trap #14 handler */
        remove_xbra(10 * 4, "fVDI");        /* LineA handler */
        if (bconout_address)
        {
            Supexec(bconout_unhook);
        }
        return 0;
    }

    /*
     * Open and initialize copies of previous workstations for fVDI,
     * unless this is a boot (in which case a fall-back is set up instead).
     */

    if (!booted)
    {
        if (debug)
        {
            PUTS("Copying available virtual workstations\n");
        }
        copy_workstations(first_vwk, 1);    /* f.vwk - default vwk to set up for, fall-through if fakeboot */
    } else
    {
#if 0
        short intout[45];
        short ptsout[12];

        if (!stand_alone)
        {
            if (debug)
            {
                PUTS("About to set up VDI fallback. Press any key.\n");
                KEY_WAIT(10);           /* It's too late to wait for a key afterwards */
            }
            /*
             * When fVDI is being booted, this will open a
             * screen workstation using the old VDI.
             * This is then used mainly for mouse/keyboard input.
             */
            old_wk_handle = call_v_opnwk(1, intout, ptsout);
        }
#endif
        readable->cookie.flags |= BOOTED;
    }

    {
        /* dangerous! This is self-modifying code ! Need to cache flush below to be safe */
        * (short *) ((long) vdi_dispatch + 2) = 0x0073;	/* Finally make fVDI take normal VDI calls */
        Supexec((long (*)(void))cache_flush);
        readable->cookie.flags |= ACTIVE;
    }


    /*
     * Set up cookies
     */

    old_eddi = get_cookie("EdDI", 0);
    if (old_eddi == -1)
        old_eddi = 0;
    set_cookie("EdDI", (long) eddi_dispatch);

    old_fsmc = get_cookie("FSMC", 0);			/* Experimental */
    if (old_fsmc == -1)
        old_fsmc = 0;
    set_cookie("FSMC", (long)&readable->fsmc_cookie);

    if (nvdi_cookie)
    {
        old_nvdi = get_cookie("NVDI", 0);
        if (old_nvdi == -1)
            old_nvdi = 0;
        set_cookie("NVDI", (long)&readable->nvdi_cookie);
    }

    if (calamus_cookie && get_cookie("DCSD", 0) == -1)
    {
        set_cookie("DCSD", (long)&readable->dcsd_cookie);
        PUTS("Calamus cookie installed\n");
    }

    if (set_cookie("fVDI", (long)&readable->cookie) && debug)
    {
        PUTS("Replacing previous cookie\n");
    }

    /*
     * Some trickery to make it possible for a TSR
     * to allocate and release memory under TOS.
     */

    if ((pid = (long *) pid_addr) != 0)
    {
        copymem((void *)basepage, fake_bp, 256);
        *pid = (long)fake_bp;
    }

    if (debug && !booted)
    {
        KEY_WAIT(10);
    }

    return 1;
}


/*
 * Shutdown support
 * Unlinks fVDI and releases all allocated memory.
 */
static long CDECL remove_fvdi(void)
{
    long ret;

    ret = 0;
    if ((readable->cookie.flags & ACTIVE) && !(readable->cookie.flags & BOOTED))
    {
        if (old_eddi)
            set_cookie("EdDI", old_eddi);
        if (old_fsmc)
            set_cookie("FSMC", old_fsmc);
        if (old_nvdi)
            set_cookie("NVDI", old_nvdi);
        remove_xbra(34 * 4, "fVDI");		/* Trap #2 handler */
        remove_xbra(46 * 4, "fVDI");		/* Trap #14 handler */
        remove_xbra(10 * 4, "fVDI");		/* LineA handler */
        if (bconout_address)
        {
            Supexec(bconout_unhook);
        }
        ret = free_all();
        shut_down();
        readable->cookie.flags = 0;
    }

    return ret;
}


/*
 * If (n > 0) return driver n
 * else return next driver after -n.
 */
static Driver *find_driver(long n)
{
    List *element;
    Driver *driver;

    element = driver_list;
    while (element)
    {
        driver = (Driver *)element->value;
        if (((n > 0) && (driver->module.id == n)) || ((n <= 0) && (driver->module.id > -n)))
            return driver;
        element = element->next;
    }

    return 0;
}


/*
 * Post-install setup
 */
static long CDECL setup_fvdi(unsigned long type, long value)
{
    Driver *driver;
    long ret;

    ret = -1;
    if (type >> 16)
    {
        driver = find_driver(type >> 16);
        if (driver)
            ret = ((long (*)(unsigned long, long))driver->module.setup)(type & 0xffff, value);
    } else
    {
        switch ((int) type)
        {
        case Q_NEXT_DRIVER:
            if ((driver = find_driver(-value)) != NULL)
                ret = driver->module.id;
            break;
        case Q_FILE:
            if ((driver = find_driver(value)) != NULL)
                ret = (long)driver->module.file_name;
            break;
        case S_DEBUG:
            if (value != -1)
                debug = value;
            ret = debug;
            break;
        case S_OPTION:
            ret = tokenize((char *)value);
            break;
        }
    }

    return ret;
}


/*
 * Modify a loaded NVDI so that it will never try
 * to move itself forward in the Trap #2 chain.
 */
static int nvdi_patch(void)
{
    long xbra_v, nvdi_v;
    long *addr, link, *nvdi, *test;
    int i, found;

    xbra_v = str2long("XBRA");
    nvdi_v = str2long("NVDI");

    /*
     * Search Trap #2 XBRA chain for NVDI
     */

    link = 0x88;
    addr = (long *) get_protected_l(link);
    while ((addr[-3] == xbra_v) && (addr[-2] != nvdi_v))
    {
        link = (long)&addr[-1];
        addr = *(long **)link;
    }

    if (addr[-2] != nvdi_v)
        return 0;				/* No NVDI found! */

    /*
     * Somewhere in the initial NVDI dispatcher there is, hopefully,
     * a pointer to the 'real' one. Find this by checking all potential
     * pointers for another XBRA id.
     */

    nvdi = addr;
    found = 0;
    for (i = 0; i < MAX_NVDI_SEARCH; i++)
    {
        addr = (long *)((long)addr + 2);
        test = (long *)*addr;
        if ((ABS(test - nvdi) < MAX_NVDI_DISTANCE / 4) && (test[-3] == xbra_v))
        {
            found = 1;
            break;
        }
    }

    /*
     * If the 'real' NVDI dispatch was found, bypass the initial one.
     */

    if (found)
    {
        set_protected_l(link, (long)test);
        test[-1] = nvdi[-1];
    }

    return found;
}


void recheck_mtask(void)
{
    long addr;

    if ((addr = get_cookie("MiNT", 1)) != -1)
        mint = addr;
    if ((addr = get_cookie("MagX", 1)) != -1)
        magic = addr;
    if (mint | magic)
        pid = 0;
}


#ifdef FVDI_DEBUG

long key_wait(void)
{
    long key;

    PUTS("Press a key");
    key = Crawcin();
    PUTS("\r\n");
    return key;
}


void CDECL vdi_debug(VDIpars *pars, long pc)
{
    static long count = 1;
    static int entered = 0;
    static int current = 0;
    static short set[] = { 9100, 109, 110, 121 };
    char buf[10];
    int i;
    short func;
    short display;
    char key;
    MFDB *mfdb;
    long old_count;
    const char *special;

    if (entered)
        return;
    entered = 1;

    {
        static int check_count = 0;
        if ((check_mem > 0) && (--check_count <= 0))
        {
            check_memory();
            check_count = check_mem;
        }
    }

    if (((long) pars) & 1)
    {
        special = "special ";
        pars = (VDIpars *)((long)pars & ~1);
    } else
    {
        special = "";
    }
    func = pars->control->function;
    if (func >= 0 && func < 256 && silent[func >> 3] & (1 << (func & 7)))
    {
        entered = 0;
        return;
    }
    display = 0;
    if ((func == set[current]) || (func == 1984) || (func == 2001) || (func == 1969))
        display = 1;

    old_count = count;
    if (display || ((debug > 2) && (--count == 0)))
    {
        kprintf("$%08lx: %sVDI(%d) %d", pc, special, pars->control->handle, func);
        if (pars->control->subfunction)
        {
            kprintf(",%d", pars->control->subfunction);
        }
        kputs("\n");

        if (pars->control->l_intin)
        {
            kprintf("  Int%d =", pars->control->l_intin);
            if ((func == 8) || (func == 241) || (func == 116) || (func == 117))
            {
                access->funcs.puts(" \"");
                for (i = 0; i < MIN(pars->control->l_intin, 72); i++)
                {
                    buf[0] = '.';
                    buf[1] = 0;
                    if ((pars->intin[i] >= 32) && (pars->intin[i] < 256))
                        buf[0] = (char)pars->intin[i];
                    kputs(buf);
                }
                kputs("\"");
            } else
            {
                for (i = 0; i < MIN(pars->control->l_intin, 12); i++)
                {
                    kprintf(" %d", pars->intin[i]);
                }
            }
            kputs("\n");
        }

        if (pars->control->l_ptsin)
        {
            kprintf("  Pts%d =", pars->control->l_ptsin);
            for (i = 0; i < MIN(pars->control->l_ptsin * 2, 12); i += 2)
            {
                kprintf(" %d,%d", pars->ptsin[i], pars->ptsin[i + 1]);
            }
            kputs("\n");
        }

        if ((func == 109) || (func == 110) || (func == 121))
        {
            mfdb = (MFDB *)pars->control->addr1;
            for (i = 0; i < 2; i++)
            {
                if (i == 0)
                    kputs("  MFDB src = ");
                else
                    kputs("  MFDB dst = ");
                if (!mfdb->address)
                    kputs("screen\n");
                else
                {
                    kprintf("$%08lx %d(%d) %d %s %d\n",
                        (long)mfdb->address,
                        mfdb->width, mfdb->wdwidth,
                        mfdb->height,
                        mfdb->standard ? "standard" : "specific",
                        mfdb->bitplanes);
                }
                mfdb = (MFDB *)pars->control->addr2;
            }
        }

        if (debug > 3)
        {
            PRINTF(("Trap #2: %08lx\n", *(long *) 0x88));
        }

        count = old_count;
        if (interactive)
        {
            key = KEY_WAIT(10);
            switch (key)
            {
            case 'q':
                count = -1;
                current = 0;
                break;
            case 'w':
                count = -1;
                current = 1;
                break;
            case 'e':
                count = -1;
                current = 2;
                break;
            case 'r':
                count = -1;
                current = 3;
                break;
            case 'd':
                debug++;
                break;
            case 'D':
                debug--;
                break;
            case 'i':
                interactive = 0;
                break;
            case '1':
                count = 5;
                break;
            case '2':
                count = 10;
                break;
            case '3':
                count = 50;
                break;
            case '4':
                count = 250;
                break;
            case '5':
                count = 1000;
                break;
            case '6':
                count = 5000;
                break;
            case '7':
                count = 25000;
                break;
            case '8':
                count = 100000L;
                break;
            case '9':
                count = 1000000L;
                break;
            case 27:
                count = -1;
                break;
            default:
                count = 1;
                break;
            }
        }
    }
    entered = 0;
}


void display_output(VDIpars *pars)
{
    int i;

    if (pars->control->l_intout)
    {
        PRINTF(("  Intout[%d] =", pars->control->l_intout));
        for (i = 0; i < MIN(pars->control->l_intout, 12); i++)
        {
            PRINTF((" %d", pars->intout[i]));
        }
        PUTS("\n");
    }

    if (pars->control->l_ptsout)
    {
        PRINTF(("  Ptsout[%d] =", pars->control->l_ptsout));
        for (i = 0; i < MIN(pars->control->l_ptsout * 2, 12); i += 2)
        {
            PRINTF((" %d,%d", pars->ptsout[i], pars->ptsout[i + 1]));
        }
        PUTS("\n");
    }
}


void CDECL trap2_debug(long type, VDIpars *pars, unsigned short *stack)
{
    int i;

    PRINTF(("Stack: $%08lx\n", (long)stack)) ;
    for (i = 0; i < 16; i++)
    {
        PRINTF(("$%04x ", *stack++));
    }
    PUTS(")\n");

    type &= 0xffff;
    if (type == 0x73)
    {
        vdi_debug(pars, 0);
    } else
    {
        PRINTF(("Trap #2: $%lx ($%08lx)\n", type, *(long *) 0x88));

        KEY_WAIT(10);
    }
}


void lineA_debug(long opcode, long pc)
{
    static int entered = 0;

    if (entered)
        return;
    entered = 1;

    PRINTF(("LineA call ($a00%x) at $%08lx.\n", (int)opcode & 0x0f, pc));

    entered = 0;
}

#endif /* FVDI_DEBUG */
