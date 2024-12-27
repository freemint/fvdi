/*
 * Bitplane blit routines
 *
 * Copyright 2005, Johan Klockars
 * Copyright 2003 The EmuTOS development team
 * Copyright 2002 Joachim Hoenig (blitter)
 *
 * This software is licensed under the GNU General Public License.
 * Please, see LICENSE.TXT for further information.
 */

#include "fvdi.h"
#include "relocate.h"
#include "driver.h"
#include "bitplane.h"

#define DBG_BLIT 0                      /* See, what happens (a bit) */

#define TRUE 1
#define FALSE 0


/* flag:1 SOURCE and PATTERN   flag:0 SOURCE only */
#define PAT_FLAG        16


static long dbg = 0;

/* Passes parameters to bitblt */
struct blit_frame
{
    short b_wd;                         /* Width of block in pixels */
    short b_ht;                         /* Height of block in pixels */
    short plane_ct;                     /* Number of consequitive planes to blt */
    unsigned short fg_col;              /* Foreground color (logic op table index:hi bit) */
    unsigned short bg_col;              /* Background color (logic op table index:lo bit) */
    unsigned char op_tab[4];            /* Logic ops for all fore and background combos */
    short s_xmin;                       /* Minimum X: source */
    short s_ymin;                       /* Minimum Y: source */
    unsigned short *s_form;             /* Source form base address */
    short s_nxwd;                       /* Offset to next word in line  (in bytes) */
    short s_nxln;                       /* Offset to next line in plane (in bytes) */
    short s_nxpl;                       /* Offset to next plane from start of current plane */
    short d_xmin;                       /* Minimum X: destination */
    short d_ymin;                       /* Minimum Y: destination */
    unsigned short *d_form;             /* Destination form base address */
    short d_nxwd;                       /* Offset to next word in line  (in bytes) */
    short d_nxln;                       /* Offset to next line in plane (in bytes) */
    short d_nxpl;                       /* Offset to next plane from start of current plane */
    unsigned short *p_addr;             /* Address of pattern buffer   (0:no pattern) */
    short p_nxln;                       /* Offset to next line in pattern  (in bytes) */
    short p_nxpl;                       /* Offset to next plane in pattern (in bytes) */
    short p_mask;                       /* Pattern index mask */

    /* These frame parameters are internally set */
    short p_indx;                       /* Initial pattern index */
    unsigned short *s_addr;             /* Initial source address */
    short s_xmax;                       /* Maximum X: source */
    short s_ymax;                       /* Maximum Y: source */
    unsigned short *d_addr;             /* Initial destination address */
    short d_xmax;                       /* Maximum X: destination */
    short d_ymax;                       /* Maximum Y: destination */
    short inner_ct;                     /* Blt inner loop initial count */
    short dst_wr;                       /* Destination form wrap (in bytes) */
    short src_wr;                       /* Source form wrap (in bytes) */
};


#define FXSR    0x80
#define NFSR    0x40
#define SKEW    0x0f



/* Blitter registers */
struct blit
{
    short src_x_inc;
    short src_y_inc;
    unsigned short *src_addr;
    short end_1, end_2, end_3;
    short dst_x_inc, dst_y_inc;
    unsigned short *dst_addr;
    unsigned short x_cnt, y_cnt;
    char hop;
    unsigned char op;
    unsigned char status;
    char skew;
};


static void do_blit_short_32(struct blit *blt)
{
    unsigned long blt_src_in;
    unsigned short blt_src_out, blt_dst_in;
    int yc;
    char skew;
    unsigned short *dst_addr;
    unsigned short *src_addr;
    long src_x_inc, src_y_inc, dst_y_inc;
    short end_1;

    skew = blt->skew;
    dst_addr = blt->dst_addr;
    src_addr = blt->src_addr;
    src_y_inc = blt->src_y_inc >> 1;
    dst_y_inc = blt->dst_y_inc >> 1;
    src_x_inc = blt->src_x_inc >> 1;
    end_1 = blt->end_1;

    yc = blt->y_cnt;
#if 1
    if (yc == 0)
        yc = 65535;
#endif

    if (blt->op < 2)
        end_1 = ~end_1;

#if 1
    do
    {
        blt_src_in = *src_addr;
        blt_src_in <<= 16;
        src_addr += src_x_inc;
        blt_src_in |= *src_addr;
        if (src_x_inc < 0)
            blt_src_in = (blt_src_in << 16) | (blt_src_in >> 16);
#else
    if (src_x_inc < 0)
    {
        src_addr += src_x_inc;
        src_x_inc = -src_x_inc;
    }
    src_y_inc += src_x_inc;
    do
    {
        blt_src_in = *src_addr;
        blt_src_in <<= 16;
        blt_src_in |= src_addr[src_x_inc];
#endif

        /* Shift blt->skew times into blt_src_out */
        blt_src_out = blt_src_in >> skew;

        switch (blt->op)
        {
        case 0:
            *dst_addr &= end_1;
            break;
        case 1:
            *dst_addr &= blt_src_out | end_1;
            break;
        case 2:
            blt_src_out ^= end_1;
            /* fall through */
        case 8:
            blt_dst_in = *dst_addr;
            blt_src_out = ((blt_src_out & end_1) | blt_dst_in) ^ end_1;
            *dst_addr = blt_src_out;
            break;
        case 3:
            blt_dst_in = *dst_addr;
            blt_src_out = ((blt_src_out ^ blt_dst_in) & end_1) ^ blt_dst_in;
            *dst_addr = blt_src_out;
            break;
        case 4:
            *dst_addr &= ~(blt_src_out & end_1);
            break;
        case 5:
            break;
        case 9:
            blt_src_out ^= end_1;
            /* fall through */
        case 6:
            *dst_addr ^= (blt_src_out & end_1);
            break;
        case 13:
            blt_src_out ^= end_1;
            /* fall through */
        case 7:
            *dst_addr |= (blt_src_out & end_1);
            break;
        case 10:
            *dst_addr ^= end_1;
            break;
        case 14:
            blt_src_out ^= end_1;
            /* fall through */
        case 11:
            blt_dst_in = *dst_addr ^ end_1;
            blt_src_out = (blt_src_out & end_1) | blt_dst_in;
            *dst_addr = blt_src_out;
            break;
        case 12:
            blt_dst_in = *dst_addr | end_1;
            blt_src_out = (blt_src_out & end_1) ^ blt_dst_in;
            *dst_addr = blt_src_out;
            break;
        case 15:
            *dst_addr |= end_1;
            break;
        default:
            unreachable();
        }

        src_addr += src_y_inc;
        dst_addr += dst_y_inc;
    } while (--yc > 0);
}



static void do_blit_short(struct blit *blt)
{
    unsigned long blt_src_in;
    unsigned short blt_src_out, blt_dst_in;
    int yc;
    char skew;
    unsigned short *dst_addr;
    unsigned short *src_addr;
    long src_y_inc, dst_y_inc;
    short end_1;

    if (blt->hop & FXSR)
    {
        do_blit_short_32(blt);
        return;
    }

    skew = blt->skew;
    dst_addr = blt->dst_addr;
    src_addr = blt->src_addr;
    src_y_inc = blt->src_y_inc;
    dst_y_inc = blt->dst_y_inc;
    end_1 = blt->end_1;

    yc = blt->y_cnt;
#if 1
    if (yc == 0)
        yc = 65535;
#endif

    if (blt->op < 2)
        end_1 = ~end_1;

    do
    {
        blt_src_in = *src_addr;
#if 0
        if (src_x_inc < 0)
            blt_src_in = (blt_src_in << 16) | (blt_src_in >> 16);
#else
        blt_src_in = (blt_src_in << 16) | blt_src_in;
#endif

        /* Shift blt->skew times into blt_src_out */
        blt_src_out = blt_src_in >> skew;

        switch (blt->op)
        {
        case 0:
            *dst_addr &= end_1;
            break;
        case 1:
            *dst_addr &= blt_src_out | end_1;
            break;
        case 2:
            blt_src_out ^= end_1;
            /* fall through */
        case 8:
            blt_dst_in = *dst_addr;
            blt_src_out = ((blt_src_out & end_1) | blt_dst_in) ^ end_1;
            *dst_addr = blt_src_out;
            break;
        case 3:
            blt_dst_in = *dst_addr;
            blt_src_out = ((blt_src_out ^ blt_dst_in) & end_1) ^ blt_dst_in;
            *dst_addr = blt_src_out;
            break;
        case 4:
            *dst_addr &= ~(blt_src_out & end_1);
            break;
        case 5:
            break;
        case 9:
            blt_src_out ^= end_1;
            /* fall through */
        case 6:
            *dst_addr ^= (blt_src_out & end_1);
            break;
        case 13:
            blt_src_out ^= end_1;
            /* fall through */
        case 7:
            *dst_addr |= (blt_src_out & end_1);
            break;
        case 10:
            *dst_addr ^= end_1;
            break;
        case 14:
            blt_src_out ^= end_1;
            /* fall through */
        case 11:
            blt_dst_in = *dst_addr ^ end_1;
            blt_src_out = (blt_src_out & end_1) | blt_dst_in;
            *dst_addr = blt_src_out;
            break;
        case 12:
            blt_dst_in = *dst_addr | end_1;
            blt_src_out = (blt_src_out & end_1) ^ blt_dst_in;
            *dst_addr = blt_src_out;
            break;
        case 15:
            *dst_addr |= end_1;
            break;
        default:
            unreachable();
        }

        src_addr += src_y_inc >> 1;
        dst_addr += dst_y_inc >> 1;
    } while (--yc > 0);
}


static void do_blit(struct blit *blt)
{
    unsigned long blt_src_in;
    unsigned short blt_src_out, blt_dst_in, blt_dst_out;
    int xc, yc;
    char skew;
    unsigned short *dst_addr;
    unsigned short *src_addr;
    long src_x_inc, dst_x_inc, src_y_inc, dst_y_inc;
    unsigned long end_1;

#if DBG_BLIT
    kprintf("bitblt: Start\n");
    kprintf("X COUNT 0x%04x\n", (unsigned short) blt->x_cnt);
    kprintf("Y COUNT 0x%04x\n", (unsigned short) blt->y_cnt);
    kprintf("X S INC 0x%04x\n", (unsigned short) src_x_inc);
    kprintf("Y S INC 0x%04x\n", (unsigned short) blt->src_y_inc);
    kprintf("X D INC 0x%04x\n", (unsigned short) blt->dst_x_inc);
    kprintf("Y D INC 0x%04x\n", (unsigned short) blt->dst_y_inc);
    kprintf("ENDMASK 0x%04x-%04x-%04x\n", (unsigned short) end_1, (unsigned short) blt->end_2, (unsigned short) blt->end_3);
    kprintf("S_ADDR  0x%08lx\n", src_addr);
    kprintf("D_ADDR  0x%08lx\n", dst_addr);
    kprintf("HOP=%01d, OP=%02d\n", blt->hop & 0x3, blt->op);
    kprintf("HOPline=%02d\n", blt->status & 0xf);
    kprintf("NFSR=%d, FXSR=%d, SKEW=%02d\n", (blt->skew & NFSR) != 0, (blt->skew & FXSR) != 0, (blt->skew & SKEW));
#endif

    if (blt->x_cnt == 1)
    {
        do_blit_short(blt);
        return;
    }

    skew = blt->skew;
    dst_addr = blt->dst_addr;
    src_addr = blt->src_addr;
    src_y_inc = blt->src_y_inc;
    dst_y_inc = blt->dst_y_inc;
    src_x_inc = blt->src_x_inc;
    dst_x_inc = blt->dst_x_inc;
    end_1 = ((unsigned long) blt->end_3 << 16) | (unsigned short) blt->end_1;

    if (blt->op < 2)
        end_1 = ~end_1;

    yc = blt->y_cnt;
#if 1
    if (blt->x_cnt == 0)
        blt->x_cnt = 65535;
    if (yc == 0)
        yc = 65535;
#endif

    if (dbg)
    {
        PRINTF(("$%08lx ($%x)\n", end_1, blt->op));
    }

    do
    {
        xc = blt->x_cnt;
        blt_src_in = 0;
        /* Next line to get rid of obnoxious compiler warnings */
        blt_src_out = blt_dst_out = 0;

        xc--;
        /* Read source into blt_src_in */
        if (blt->hop & FXSR)
        {
            blt_src_in = *src_addr;
            blt_src_in <<= 16;
            src_addr += src_x_inc >> 1;
        }
        blt_src_in |= *src_addr;
        src_addr += src_x_inc >> 1;
        if (src_x_inc < 0)
            blt_src_in = (blt_src_in << 16) | (blt_src_in >> 16);

        /* Shift blt->skew times into blt_src_out */
        blt_src_out = blt_src_in >> skew;

        switch (blt->op)
        {
        case 0:
            *dst_addr &= end_1;
            break;
        case 1:
            *dst_addr &= blt_src_out | end_1;
            break;
        case 2:
            blt_src_out ^= end_1;
            /* fall through */
        case 8:
            blt_dst_in = *dst_addr;
            blt_src_out = ((blt_src_out & end_1) | blt_dst_in) ^ end_1;
            *dst_addr = blt_src_out;
            break;
        case 3:
            blt_dst_in = *dst_addr;
            blt_src_out = ((blt_src_out ^ blt_dst_in) & end_1) ^ blt_dst_in;
            *dst_addr = blt_src_out;
            break;
        case 4:
            *dst_addr &= ~(blt_src_out & end_1);
            break;
        case 5:
            break;
        case 9:
            blt_src_out ^= end_1;
            /* fall through */
        case 6:
            *dst_addr ^= (blt_src_out & end_1);
            break;
        case 13:
            blt_src_out ^= end_1;
            /* fall through */
        case 7:
            *dst_addr |= (blt_src_out & end_1);
            break;
        case 10:
            *dst_addr ^= end_1;
            break;
        case 14:
            blt_src_out ^= end_1;
            /* fall through */
        case 11:
            blt_dst_in = *dst_addr ^ end_1;
            blt_src_out = (blt_src_out & end_1) | blt_dst_in;
            *dst_addr = blt_src_out;
            break;
        case 12:
            blt_dst_in = *dst_addr | end_1;
            blt_src_out = (blt_src_out & end_1) ^ blt_dst_in;
            *dst_addr = blt_src_out;
            break;
        case 15:
            *dst_addr |= end_1;
            break;
        default:
            unreachable();
        }

        if (xc)
        {
            dst_addr += dst_x_inc >> 1;
        }

        while (--xc > 0)
        {
            if (src_x_inc >= 0)
            {
                blt_src_in <<= 16;
                blt_src_in |= *src_addr;
            } else
            {
                blt_src_in >>= 16;
                blt_src_in |= (unsigned long)(*src_addr) << 16;
            }
            src_addr += src_x_inc >> 1;

            /* Shift blt->skew times into blt_src_out */
            blt_src_out = blt_src_in >> skew;

            switch (blt->op)
            {
            case 0:
                blt_src_out = 0;
                break;
            case 4:
                blt_src_out = ~blt_src_out;
                /* fall through */
            case 1:
                blt_dst_in = *dst_addr;
                blt_src_out = blt_src_out & blt_dst_in;
                break;
            case 2:
                blt_dst_in = *dst_addr;
                blt_src_out = blt_src_out & ~blt_dst_in;
                break;
            case 3:
                break;
            case 5:
                blt_dst_in = *dst_addr;
                blt_src_out = blt_dst_in;
                break;
            case 9:
                blt_src_out = ~blt_src_out;
                /* fall through */
            case 6:
                blt_dst_in = *dst_addr;
                blt_src_out = blt_src_out ^ blt_dst_in;
                break;
            case 13:
                blt_src_out = ~blt_src_out;
                /* fall through */
            case 7:
                blt_dst_in = *dst_addr;
                blt_src_out = blt_src_out | blt_dst_in;
                break;
            case 8:
                blt_dst_in = *dst_addr;
                blt_src_out = ~(blt_src_out | blt_dst_in);
                break;
            case 10:
                blt_dst_in = *dst_addr;
                blt_src_out = ~blt_dst_in;
                break;
            case 14:
                blt_src_out = ~blt_src_out;
                /* fall through */
            case 11:
                blt_dst_in = *dst_addr;
                blt_src_out = blt_src_out | ~blt_dst_in;
                break;
            case 12:
                blt_src_out = ~blt_src_out;
                break;
            case 15:
                blt_src_out = 0xffff;
                break;
            default:
                unreachable();
            }

            *dst_addr = blt_src_out;
            dst_addr += dst_x_inc >> 1;
        }

        /* Read source into blt_src_in */
        if (src_x_inc >= 0)
        {
            blt_src_in <<= 16;
            if (blt->hop & NFSR)
            {
                src_addr -= src_x_inc >> 1;
            } else
            {
                blt_src_in |= *src_addr;
            }
        } else
        {
            blt_src_in >>= 16;
            if (blt->hop & NFSR)
            {
                src_addr -= src_x_inc >> 1;
            } else
            {
                blt_src_in |= (unsigned long)(*src_addr) << 16;
            }
        }

        /* Shift blt->skew times into blt_src_out */
        blt_src_out = blt_src_in >> skew;

        end_1 = (end_1 >> 16) | (end_1 << 16);
        
        switch (blt->op)
        {
        case 0:
            *dst_addr &= end_1;
            break;
        case 1:
            *dst_addr &= blt_src_out | end_1;
            break;
        case 2:
            blt_src_out ^= end_1;
            /* fall through */
        case 8:
            blt_dst_in = *dst_addr;
            blt_src_out = ((blt_src_out & end_1) | blt_dst_in) ^ end_1;
            *dst_addr = blt_src_out;
            break;
        case 3:
            blt_dst_in = *dst_addr;
            blt_src_out = ((blt_src_out ^ blt_dst_in) & end_1) ^ blt_dst_in;
            *dst_addr = blt_src_out;
            break;
        case 4:
            *dst_addr &= ~(blt_src_out & end_1);
            break;
        case 5:
            break;
        case 9:
            blt_src_out ^= end_1;
            /* fall through */
        case 6:
            *dst_addr ^= (blt_src_out & end_1);
            break;
        case 13:
            blt_src_out ^= end_1;
            /* fall through */
        case 7:
            *dst_addr |= (blt_src_out & end_1);
            break;
        case 10:
            *dst_addr ^= end_1;
            break;
        case 14:
            blt_src_out ^= end_1;
            /* fall through */
        case 11:
            blt_dst_in = *dst_addr ^ end_1;
            blt_src_out = (blt_src_out & end_1) | blt_dst_in;
            *dst_addr = blt_src_out;
            break;
        case 12:
            blt_dst_in = *dst_addr | end_1;
            blt_src_out = (blt_src_out & end_1) ^ blt_dst_in;
            *dst_addr = blt_src_out;
            break;
        case 15:
            *dst_addr |= end_1;
            break;
        default:
            unreachable();
        }
        end_1 = (end_1 >> 16) | (end_1 << 16);

        src_addr += src_y_inc >> 1;
        dst_addr += dst_y_inc >> 1;
    } while (--yc > 0);
}


/*
 * endmask data
 *
 * a bit means:
 *
 *   0: Destination
 *   1: Source <<< Invert right end mask data >>>
 */

/* TiTLE: BLiT_iT */

/* PuRPoSE: */
/* Transfer a rectangular block of pixels located at an */
/* arbitrary X,Y position in the source memory form to */
/* another arbitrary X,Y position in the destination memory */
/* form using replace mode (boolean operator 3). */
/* The source and destination rectangles should not overlap. */

static void bit_blt(struct blit_frame *info)
{
    short plane;
    unsigned short s_xmin, s_xmax;
    unsigned short d_xmin, d_xmax;
    unsigned short lendmask, rendmask;
    short skew, skew_idx;
    short s_span, s_xmin_off, s_xmax_off;
    short d_span, d_xmin_off, d_xmax_off;
    unsigned long s_addr, d_addr;
    struct blit blitter;
    struct blit *blt = &blitter;

    /* Setting of skew flags */

    /* QUALIFIERS   ACTIONS   BITBLT DIRECTION: LEFT -> RIGHT */

    /* equal Sx&F> */
    /* spans Dx&F FXSR NFSR */

    /* 0     0     0    1 |..ssssssssssssss|ssssssssssssss..|   */
    /*   |......dddddddddd|dddddddddddddddd|dd..............|   */

    /* 0     1      1  0 */
    /*   |......ssssssssss|ssssssssssssssss|ss..............|   */
    /*   |..dddddddddddddd|dddddddddddddd..|   */

    /* 1     0     0    0 |..ssssssssssssss|ssssssssssssss..|   */
    /*   |...ddddddddddddd|ddddddddddddddd.|   */

    /* 1     1     1    1 |...sssssssssssss|sssssssssssssss.|   */
    /*   |..dddddddddddddd|dddddddddddddd..|   */

#define mSkewFXSR    0x80
#define mSkewNFSR    0x40

    const unsigned char skew_flags[8] = {
        mSkewNFSR,                      /* Source span < Destination span */
        mSkewFXSR,                      /* Source span > Destination span */
        0,                              /* Spans equal Shift Source right */
        mSkewNFSR + mSkewFXSR,          /* Spans equal Shift Source left */

        /* When Destination span is but a single word ... */
        0,                              /* Implies a Source span of no words */
        mSkewFXSR,                      /* Source span of two words */
        0,                              /* Skew flags aren't set if Source and */
        0                               /* destination spans are both one word */
    };

    /* Calculate Xmax coordinates from Xmin coordinates and width */
    s_xmin = info->s_xmin;              /* d0<- src Xmin */
    s_xmax = s_xmin + info->b_wd - 1;   /* d1<- src Xmax=src Xmin+width-1 */
    d_xmin = info->d_xmin;              /* d2<- dst Xmin */
    d_xmax = d_xmin + info->b_wd - 1;   /* d3<- dst Xmax=dstXmin+width-1 */

    /* Skew value is (destination Xmin mod 16 - source Xmin mod 16) */
    /* && 0x000F.  Three discriminators are used to determine the */
    /* states of FXSR and NFSR flags: */

    /* bit 0     0: Source Xmin mod 16 =< Destination Xmin mod 16 */
    /*           1: Source Xmin mod 16 >  Destination Xmin mod 16 */

    /* bit 1     0: SrcXmax/16-SrcXmin/16 <> DstXmax/16-DstXmin/16 */
    /*                       Source span      Destination span */
    /*           1: SrcXmax/16-SrcXmin/16 == DstXmax/16-DstXmin/16 */

    /* bit 2     0: multiple word Destination span */
    /*           1: single word Destination span */

    /* These flags form an offset into a skew flag table yielding */
    /* correct FXSR and NFSR flag states for the given source and */
    /* destination alignments */

    skew_idx = 0x0000;                  /* Default */

    s_xmin_off = s_xmin >> 4;           /* d0<- word offset to src Xmin */
    s_xmax_off = s_xmax >> 4;           /* d1<- word offset to src Xmax */
    s_span = s_xmax_off - s_xmin_off;   /* d1<- Src span - 1 */

    d_xmin_off = d_xmin >> 4;           /* d2<- word offset to dst Xmin */
    d_xmax_off = d_xmax >> 4;           /* d3<- word offset to dst Xmax */
    d_span = d_xmax_off - d_xmin_off;   /* d3<- dst span - 1 */

    /* The last discriminator is the */
    if (d_span == s_span)
    {                                   /* equality of src and dst spans */
        skew_idx |= 0x0002;             /* Equal spans */
    }

    /* Number of words in dst line */
    blt->x_cnt = d_span + 1;            /* Set value in BLiTTER */

    /* Endmasks derived from source Xmin mod 16 and source Xmax mod 16 */
    lendmask = 0xffff >> (d_xmin % 16);
    rendmask = ~(0x7fff >> (d_xmax % 16));

    /* Does destination just span a single word? */
    if (!d_span)
    {
        /* Merge both end masks into Endmask1. */
        lendmask &= rendmask;           /* Single word end mask */
#if 0
        rendmask = lendmask;
#endif
        skew_idx |= 0x0004;             /* Single word dst */
        /* The other end masks will be ignored by the BLiTTER */
    }

    /* Calculate starting addresses */
    s_addr = (unsigned long)info->s_form +
             (unsigned long)info->s_ymin * (unsigned long)info->s_nxln +
             (unsigned long)s_xmin_off * (unsigned long)info->s_nxwd;
    d_addr = (unsigned long)info->d_form +
             (unsigned long)info->d_ymin * (unsigned long)info->d_nxln +
             (unsigned long)d_xmin_off * (unsigned long)info->d_nxwd;

    if (info->b_ht != 1)
    {
        PRINTF(("$%08lx->$%08lx : %d , %d ; %d , %d * %d\n",
            (long) s_addr, (long) d_addr,
            info->s_nxwd, info->d_nxwd,
            info->s_nxln - info->s_nxwd * s_span,
            info->d_nxln - info->d_nxwd * d_span,
            info->b_ht));
    }

    if (s_addr < d_addr)
    {
        /* Start from lower right corner, so add width+length */
        s_addr = (unsigned long)info->s_form +
                 (unsigned long)info->s_ymax * (unsigned long)info->s_nxln +
                 (unsigned long)s_xmax_off * (unsigned long)info->s_nxwd;
        d_addr = (unsigned long)info->d_form +
                 (unsigned long)info->d_ymax * (unsigned long)info->d_nxln +
                 (unsigned long)d_xmax_off * (unsigned long)info->d_nxwd;

        /* Offset between consecutive words in planes */
        blt->src_x_inc = -info->s_nxwd;
        blt->dst_x_inc = -info->d_nxwd;

        /* Offset from last word of a line to first word of next one */
        blt->src_y_inc = -(info->s_nxln - info->s_nxwd * s_span);
        blt->dst_y_inc = -(info->d_nxln - info->d_nxwd * d_span);

        blt->end_1 = rendmask;          /* Left end mask */
        blt->end_2 = 0xFFFF;            /* Center mask */
        blt->end_3 = lendmask;          /* Right end mask */

        /* We start at maximum, d7<- Dst Xmax mod16 - Src Xmax mod16 */
        skew = (d_xmax & 0x0f) - (s_xmax & 0x0f);
        if (skew >= 0)
            skew_idx |= 0x0001;         /* d6[bit0]<- Alignment flag */
    } else
    {
        if (info->b_ht != 1)
        {
            PUTS("******* Top to bottom **********\n");
        }

        /* Offset between consecutive words in planes */
        blt->src_x_inc = info->s_nxwd;
        blt->dst_x_inc = info->d_nxwd;

        /* Offset from last word of a line to first word of next one */
        blt->src_y_inc = info->s_nxln - info->s_nxwd * s_span;
        blt->dst_y_inc = info->d_nxln - info->d_nxwd * d_span;

        blt->end_1 = lendmask;          /* Left end mask */
        blt->end_2 = 0xFFFF;            /* Center mask */
        blt->end_3 = rendmask;          /* Right end mask */

        /* d7<- Dst Xmin mod16 - Src Xmin mod16 */
        skew = (d_xmin & 0x0f) - (s_xmin & 0x0f);
        if (skew < 0)
            skew_idx |= 0x0001;         /* Alignment flag */

        PRINTF(("$%08lx->$%08lx : $%08lx , $%08lx\n", (long) s_addr, (long) d_addr, (long) lendmask, (long) rendmask));
    }

    if (info->b_ht != 1)
    {
        PRINTF(("$%08lx->$%08lx\n", (long) s_addr, (long) d_addr));
    }

    /*
     * The low nibble of the difference in Source and Destination alignment
     * is the skew value.  Use the skew flag index to reference FXSR and
     * NFSR states in skew flag table.
     */
    blt->skew = skew & 0x0f;
    blt->hop = skew_flags[skew_idx];

    for (plane = info->plane_ct - 1; plane >= 0; plane--)
    {
        int op_tabidx;

        blt->src_addr = (unsigned short *)s_addr;         /* Load Source pointer to this plane */
        blt->dst_addr = (unsigned short *)d_addr;         /* Load Dest ptr to this plane */
        blt->y_cnt = info->b_ht;        /* Load the line count */

        /* Calculate operation for actual plane */
        op_tabidx = ((info->fg_col >> plane) & 0x0001) << 1;
        op_tabidx |= (info->bg_col >> plane) & 0x0001;
        blt->op = info->op_tab[op_tabidx] & 0x000f;

        do_blit(blt);

        s_addr += info->s_nxpl;         /* a0-> start of next src plane */
        d_addr += info->d_nxpl;         /* a1-> start of next dst plane */
    }
}


/*
 * Fill the info structure with MFDB values
 */
static int setup_info(Workstation *wk, struct blit_frame *info, MFDB *src, MFDB *dst, int src_x, int src_y, int dst_x, int dst_y, int w, int h)
{
    /* Setup plane info for source MFDB */
    if (src && src->address && (src->address != wk->screen.mfdb.address || wk->screen.type == 0))
    {
        /* For a positive source address */
        info->s_form = (unsigned short *) src->address;
        info->s_nxwd = src->bitplanes * 2;
        info->s_nxln = src->wdwidth * info->s_nxwd;
    } else
    {
        /* Source form is screen */
        info->s_form = (unsigned short *) wk->screen.mfdb.address;
        info->s_nxwd = wk->screen.mfdb.bitplanes * 2;
        info->s_nxln = wk->screen.wrap;
    }

    /* Setup plane info for destination MFDB */
    if (dst && dst->address && (dst->address != wk->screen.mfdb.address || wk->screen.type == 0))
    {
        /* For a positive address */
        info->d_form = (unsigned short *) dst->address;
        info->plane_ct = dst->bitplanes;
        info->d_nxwd = dst->bitplanes * 2;
        info->d_nxln = dst->wdwidth * info->d_nxwd;
    } else
    {
        /* Destination form is screen */
        info->d_form = (unsigned short *) wk->screen.mfdb.address;
        info->plane_ct = wk->screen.mfdb.bitplanes;
        info->d_nxwd = wk->screen.mfdb.bitplanes * 2;
        info->d_nxln = wk->screen.wrap;
    }

    /* Source */
    info->s_xmin = src_x;
    info->s_ymin = src_y;
    info->s_xmax = src_x + w - 1;
    info->s_ymax = src_y + h - 1;

    /* Width and height of block in pixels */
    info->b_wd = w;
    info->b_ht = h;

    /* Destination */
    info->d_xmin = dst_x;
    info->d_ymin = dst_y;
    info->d_xmax = dst_x + w - 1;
    info->d_ymax = dst_y + h - 1;

    info->s_nxpl = 2;                   /* Next plane offset (source) */
    info->d_nxpl = 2;                   /* Next plane offset (destination) */

    /* Only 8, 4, 2, and 1 planes are valid (destination) */
    return info->plane_ct & ~0x000f;
}


/*
 * copy raster opaque
 *
 * This function copies a rectangular raster area from source form to
 * destination form using the logic operation specified by the application.
 */
long CDECL c_blit_area(Virtual *vwk, MFDB *src, long src_x, long src_y, MFDB *dst,
            long dst_x, long dst_y, long w, long h, long operation)
{
    struct blit_frame info;             /* Holds some internal info for bit_blt */

    operation &= 0x0f;

    /* Check the pattern flag (bit 5) and revert to log op # */
    info.p_addr = 0;                    /* Clear pattern pointer */

    /* If true, the plane count is invalid or clipping took all! */
    if (setup_info(vwk->real_address, &info, src, dst, src_x, src_y, dst_x, dst_y, w, h))
        return 1;

    /* Planes of source and destination must be equal in number */
    if (info.s_nxwd != info.d_nxwd)
        return 1;

    info.op_tab[0] = operation;         /* fg:0 bg:0 */
    info.bg_col = 0;                    /* bg:0 & fg:0 => only first OP_TAB */
    info.fg_col = 0;                    /* entry will be referenced */

    bit_blt(&info);

    return 1;
}


/*
 * vdi_vrt_cpyfm - copy raster transparent
 *
 * This function copies a monochrome raster area from source form to a
 * color area. A writing mode and color indices for both 0's and 1's
 * are specified in the INTIN array.
 */

long CDECL c_expand_area(Virtual *vwk, MFDB *src, long src_x, long src_y,
              MFDB *dst, long dst_x, long dst_y, long w, long h,
              long operation, long colour)
{
    struct blit_frame info;             /* Holds some internal info for bit_blt */
    unsigned long foreground;
    unsigned long background;

    c_get_colours((Virtual *) ((long)vwk & ~1), colour, &foreground, &background);

    operation = ((operation - 1) & 0x03) + 1;

    /* Check the pattern flag (bit 5) and revert to log op # */
    info.p_addr = 0;                    /* Get pattern pointer */

    if (w != 1 || h != 1)
    {
        PRINTF(("%ld,%ld %ld,%ld %ld,%ld\n", src_x, src_y, dst_x, dst_y, w, h));
    }

    /* If true, the plane count is invalid or clipping took all! */
    if (setup_info(vwk->real_address, &info, src, dst, src_x, src_y, dst_x, dst_y, w, h))
        return 1;

    /*
     * COPY RASTER TRANSPARENT - copies a monochrome raster area
     * from source form to a color area. A writing mode and color
     * indices for both 0's and 1's are specified in the INTIN array.
     */

    /* Is source area one plane? */
    if (info.s_nxwd != 2)
        return 1;                       /* Source must be mono plane */

    info.s_nxpl = 0;                    /* Use only one plane of source */

    switch (operation)
    {
    case 2:
        info.op_tab[0] = 04;            /* fg:0 bg:0  D' <- [not S] and D */
        info.op_tab[2] = 07;            /* fg:1 bg:0  D' <- S or D */
        info.fg_col = foreground;       /* We're only interested in one color */
        info.bg_col = 0;                /* Save the color of interest */
        break;

    case 1:
        /* CHECK: bug, that colors are reversed? */
        info.op_tab[0] = 00;            /* fg:0 bg:0  D' <- 0 */
        info.op_tab[1] = 12;            /* fg:0 bg:1  D' <- not S */
        info.op_tab[2] = 03;            /* fg:1 bg:0  D' <- S */
        info.op_tab[3] = 15;            /* fg:1 bg:1  D' <- 1 */
        info.bg_col = background;       /* Save fore and background colors */
        info.fg_col = foreground;
        break;

    case 3:
        info.op_tab[0] = 06;            /* fg:0 bg:0  D' <- S xor D */
        info.bg_col = 0;
        info.fg_col = 0;
        break;

    case 4:
        info.op_tab[0] = 01;            /* fg:0 bg:0  D' <- S and D */
        info.op_tab[1] = 13;            /* fg:0 bg:1  D' <- [not S] or D */
        info.fg_col = 0;                /* We're only interested in one color */
        info.bg_col = background;       /* Save the color of interest */
        break;

    default:
        return 1;                       /* Unsupported mode */
    }

    bit_blt(&info);

    return 1;
}


long CDECL c_read_pixel(Virtual *vwk, MFDB *mfdb, long x, long y)
{
    MFDB dst;
    short pixel[8], *ptr;
    int colour, i;

    dst.address = pixel;
    dst.width = 16;
    dst.height = 1;
    dst.wdwidth = 1;
    dst.standard = 0;
    dst.bitplanes = mfdb->bitplanes;

    /* Fetch one pixel in D=S mode */
    c_blit_area(vwk, mfdb, x, y, &dst, 15, 0, 1, 1, 3);

    colour = 0;
    ptr = pixel;
    for (i = mfdb->bitplanes - 1; i >= 0; --i)
    {
        colour *= 2;
        colour += *ptr++ & 1;
    }

    return colour;
}


long CDECL c_write_pixel(Virtual *vwk, MFDB *mfdb, long x, long y, long colour)
{
    MFDB src;
    short pixel;

    /* Don't understand any table operations yet. */
    if ((long) vwk & 1)
        return -1;

    pixel = 0x8000;
    src.address = &pixel;
    src.width = 16;
    src.height = 1;
    src.wdwidth = 1;
    src.standard = 0;
    src.bitplanes = 1;

    /* Blit a transparent (could as well have been replace mode) pixel */
    c_expand_area(vwk, &src, 0L, 0L, mfdb, x, y, 1L, 1L, 2L, colour);

    return 1;
}


static short set_mouse_colours(Workstation *wk)
{
    int foreground, background;
    short i, colours;

    foreground = x_get_colour(wk, wk->mouse.colour.foreground);
    background = x_get_colour(wk, wk->mouse.colour.background);

    switch (wk->screen.mfdb.bitplanes)
    {
    case 1:
        foreground = (foreground << 1) | foreground;
        background = (background << 1) | background;
        /* fall through */
    case 2:
        foreground = (foreground << 2) | foreground;
        background = (background << 2) | background;
        /* fall through */
    case 4:
        foreground = (foreground << 4) | foreground;
        background = (background << 4) | background;
        break;
    }
    foreground <<= 7;
    background <<= 7;
    colours = 0;
    for (i = 7; i >= 0; i--)
    {
        colours <<= 1;
        foreground <<= 1;
        if (foreground & 0x8000)
            colours |= 1;
        colours <<= 1;
        background <<= 1;
        if (background & 0x8000)
            colours |= 1;
    }

    return colours;
}


static void set_mouse_shape(Mouse *mouse, unsigned short *masks)
{
    int i;

    for (i = 0; i < 16; i++)
    {
        *masks++ = mouse->mask[i];
        *masks++ = mouse->data[i];
    }
}


#define PLANES 1
#include "mouse.c"
#undef PLANES

#define PLANES 2
#include "mouse.c"
#undef PLANES

#define PLANES 4
#include "mouse.c"
#undef PLANES

#define PLANES 8
#include "mouse.c"
#undef PLANES

long CDECL c_mouse_draw(Workstation *wk, long x, long y, Mouse *mouse)
{
    static long CDECL (*mouse_draw[])(Workstation *, long, long, Mouse *) =
    {
        0, c_mouse_draw_1, c_mouse_draw_2, 0, c_mouse_draw_4,
        0, 0, 0, c_mouse_draw_8
    };

    return mouse_draw[wk->screen.mfdb.bitplanes](wk, x, y, mouse);
}
