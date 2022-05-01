#ifndef FUNCTION_H
#define FUNCTION_H

#include "fvdi.h"

/*
 * fVDI function declarations
 *
 * Copyright 2003, Johan Klockars
 * This software is licensed under the GNU General Public License.
 * Please, see LICENSE.TXT for further information.
 */

void linea_setup(Workstation *);

long vector_call(void *vector, long data);

void do_nothing(void);
void user_cur(void);

void bad_or_non_fvdi_handle(void);

int load_prefs(Virtual *vwk, const char *sysname);
Virtual *initialize_vdi(void);
void copy_workstations(Virtual *def, long really_copy);
void shut_down(void);
long tokenize(const char *buffer);
void CDECL set_palette(Virtual *vwk, DrvPalette *palette_pars);

void v_bez_accel(long vwk, short *points, long num_points, long totmoves, short *xmov, long pattern, Fgbg colour, long mode);
void lib_v_pline(Virtual *, struct v_bez_pars *);
void c_pline(Virtual *vwk, long num_pts, Fgbg colour, short *points);
void filled_poly(Virtual *vwk, short p[][2], long n, Fgbg colour, short *pattern, short *points, long mode, long interior_style);
void filled_poly_m(Virtual *vwk, short p[][2], long n, Fgbg colour, short *pattern, short *points, short index[], long moves, long mode, long interior_style);
void fill_poly(Virtual *vwk, short *p, long n, Fgbg colour, short *pattern, short *points, long mode, long interior_style);
void fill_area(Virtual *vwk, long x1, long y1, long x2, long y2, Fgbg colour);
void get_extent(Virtual *vwk, long length, short *text, short points[]);
void draw_text(Virtual *vwk, long x, long y, short *text, long length, Fgbg colour);
void hline(Virtual *vwk, long x1, long y1, long y2, Fgbg colour, short *pattern, long mode, long interior_style);
void fill_spans(void *, short *, long n, Fgbg colour, short *pattern, long mode, long interior_style);



void *CDECL set_stack_call_pvlpl(void *stk, long size, void *func, Virtual *,   long, void *,   long);
long CDECL set_stack_call_lppll(void *stk, long size, void *func,    void *, void *,   long,   long);
long CDECL set_stack_call_lpppll(void *stk, long size, void *func,   void *, void *, void *,   long,   long);
long CDECL set_stack_call_lplll(void *stk, long size, void *func,    void *,   long,   long,   long);
long CDECL set_stack_call_lvplp(void *stk, long size, void *func, Virtual *, void *,   long, void *);
long CDECL set_stack_call_lvppl(void *stk, long size, void *func, Virtual *, void *, void *,   long);
#ifdef __PUREC__
void CDECL lib_vdi_s(void *func, Virtual *vwk, short);
void CDECL lib_vdi_sp(void *func, Virtual *vwk, short, void *);
void CDECL lib_vdi_spppp(void *func, Virtual *vwk, short, void *, void *, void *, void *);
void CDECL lib_vdi_pp(void *func, Virtual *vwk, void *, void *);
#else
#ifdef __GNUC__
void CDECL lib_vdi_s(void *func, Virtual *vwk, long);
void CDECL lib_vdi_sp(void *func, Virtual *vwk, long, void *);
void CDECL lib_vdi_spppp(void *func, Virtual *vwk, long, void *, void *, void *, void *);
void CDECL lib_vdi_pp(void *func, Virtual *vwk, void *, void *);
#else
void CDECL lib_vdi_s(void *func, Virtual *vwk, short);
void CDECL lib_vdi_sp(void *func, Virtual *vwk, short, void *);
void CDECL lib_vdi_spppp(void *func, Virtual *vwk, short, void *, void *, void *, void *);
void CDECL lib_vdi_pp(void *func, Virtual *vwk, void *, void *);
#define LIB_CALL {"224f4e92";}			/* move.l a7,a1   jsr (a2) */
#pragma inline lib_vdi_s(a2, a0, (short)) LIB_CALL
#pragma inline lib_vdi_sp(a2, a0, (short),) LIB_CALL
#pragma inline lib_vdi_spppp(a2, a0, (short),,,,) LIB_CALL
#pragma inline lib_vdi_pp(a2, a0,,) LIB_CALL
 #endif
#endif

void init_interrupts(void);
void reset_interrupts(void);

int lib_vst_font(Virtual *vwk, long fontID);
fix31 lib_vst_point(Virtual *vwk, fix31 height, short *charw, short *charh, short *cellw, short *cellh);
void lib_vrt_cpyfm_nocheck(Virtual *vwk, short mode, short *pxy, MFDB *src, MFDB *dst, short colors[]);
void lib_vro_cpyfm(Virtual *vwk, short mode, short *pxy, MFDB *src, MFDB *dst);
void lib_vs_clip(Virtual *, short, short *);
void lib_vr_trnfm(Virtual *, MFDB *, MFDB *);
void opnvwk_values(Virtual *, VDIpars *);
void CDECL lib_v_bez(Virtual *vwk, struct v_bez_pars *par);
long lib_vst_load_fonts(Virtual *vwk, long select);
void lib_vst_unload_fonts(Virtual *vwk, long select);
void CDECL lib_vqt_extent(Virtual *vwk, long length, short *string, short *points);
long CDECL lib_vst_effects(Virtual *vwk, long effects);
void CDECL lib_vst_alignment(Virtual *vwk, unsigned long halign, unsigned long valign, short *hresult, short *vresult);
long CDECL lib_vqt_name(Virtual * vwk, long number, short *name);
void CDECL lib_vqt_fontinfo(Virtual *vwk, short *intout, short *ptsout);
void CDECL lib_vqt_xfntinfo(Virtual *vwk, long flags, long id, long index, XFNT_INFO *info);
void CDECL lib_vqt_fontheader(Virtual *vwk, VQT_FHDR *fhdr);
fix31 CDECL lib_vst_arbpt(Virtual *vwk, fix31 height, short *charw, short *charh, short *cellw, short *cellh);
void CDECL lib_vqt_attributes(Virtual * vwk, short *settings);
unsigned short CDECL lib_vqt_char_index(Virtual *vwk, short *intin);
short CDECL lib_vst_charmap(Virtual *vwk, long mode);

void CDECL lib_vs_color(Virtual *vwk, long pen, RGB *values);
int CDECL lib_vq_color(Virtual *vwk, long pen, long flag, RGB *colour);
int CDECL lib_vs_fg_color(Virtual *vwk, long subfunction, long colour_space, COLOR_ENTRY *values);
int CDECL lib_vs_bg_color(Virtual *vwk, long subfunction, long colour_space, COLOR_ENTRY *values);
long CDECL lib_vq_fg_color(Virtual *vwk, long subfunction, COLOR_ENTRY *colour);
long CDECL lib_vq_bg_color(Virtual *vwk, long subfunction, COLOR_ENTRY *colour);
int CDECL colour_entry(Virtual *vwk, long subfunction, short *intin, short *intout);
int CDECL set_colour_table(Virtual *vwk, long subfunction, short *intin);
int CDECL colour_table(Virtual *vwk, long subfunction, short *intin, short *intout);
int CDECL inverse_table(Virtual *vwk, long subfunction, short *intin, short *intout);

extern char const ft2_version[];
extern int (*external_init) (void);
extern void (*external_term) (void);
extern Fontheader* (*external_load_font)(Virtual *vwk, const char *font);
extern long        (*external_vqt_extent)(Virtual *vwk, Fontheader *font, short *text, long length);
extern long        (*external_vqt_width)(Virtual *vwk, Fontheader *font, long ch);
extern Fontheader* (*external_vst_point)(Virtual *vwk, fix31 size, short *sizes);
extern long        (*external_renderer)(Virtual *vwk, unsigned long coords,
                    short *text, long length);
extern void*       (*external_char_bitmap)(Virtual *vwk, Fontheader *font, long ch, short *bitmap_info);
extern void*       (*external_char_advance)(Virtual *vwk, Fontheader *font, long ch, short *advance_info);

extern void        (*external_xfntinfo)(Virtual *vwk, Fontheader *font, long flags, XFNT_INFO *info);
extern void        (*external_fontheader)(Virtual *vwk, Fontheader *font, VQT_FHDR *fhdr);
extern unsigned short (*external_char_index) (Virtual *vwk, Fontheader *font, short *intin);

#ifdef FVDI_DEBUG
void display_output(VDIpars *pars);
#endif

void CDECL wide_line(Virtual *vwk, short *pts, long numpts, Fgbg colour, short *points, long mode);
void CDECL do_arrow(Virtual *vwk, short *pts, long numpts, Fgbg colour, short *points, long mode);

void CDECL v_opnvwk(Virtual *vwk, VDIpars * pars);
void CDECL v_opnwk(VDIpars *pars);
void CDECL v_clsvwk(Virtual *vwk, VDIpars *pars);
void CDECL v_clswk(Virtual *vwk, VDIpars *pars);
void CDECL vq_devinfo(VDIpars *pars);

void CDECL scall_v_clswk(long handle);

void CDECL bconout_char(long ch);

void CDECL vq_chcells(Virtual *vwk, short *rows, short *columns);
void CDECL v_curup(Virtual *vwk);
void CDECL v_curdown(Virtual *vwk);
void CDECL v_curright(Virtual *vwk);
void CDECL v_curleft(Virtual *vwk);
void CDECL v_eeol(Virtual *vwk);
void CDECL v_eeos(Virtual *vwk);
void CDECL v_curhome(Virtual *vwk);
void CDECL v_exit_cur(Virtual *vwk);
void CDECL v_enter_cur(Virtual *vwk);
void CDECL vs_curaddress(Virtual *vwk, long row, long column);
void CDECL v_curtext(Virtual *vwk, short *text, long length);
void CDECL v_rvon(Virtual *vwk);
void CDECL v_rvoff(Virtual *vwk);
void CDECL vq_curaddress(Virtual *vwk, short *row, short *column);
void CDECL vq_tabstatus(Virtual *vwk, short *tablet);
void CDECL v_hardcopy(Virtual *vwk);
void CDECL v_dspcur(Virtual *vwk, long x, long y);
void CDECL v_rmcur(Virtual *vwk);
void CDECL v_form_adv(Virtual *vwk);
void CDECL v_output_window(Virtual *vwk, long x1, long y1, long x2, long y2);
void CDECL v_clear_display_list(Virtual *vwk);
void CDECL v_bit_image(Virtual *vwk, char *filename, long aspect, long scaling, long num_pts, short points[]);

/*
 * fallback implementations
 */
long CDECL default_line(Virtual *vwk, DrvLine *pars);
long CDECL default_blit(Virtual *vwk, MFDB *src, long src_x, long src_y, MFDB *dst, long dst_x, long dst_y, long w, long h, long operation);
void CDECL default_text(Virtual *vwk, short *text, long length, long dst_x, long dst_y, short *offsets);
void CDECL default_fill(Virtual *vwk, long x, long y, long w, long h, short *pattern, long colour, long mode, long interior_style);
void CDECL default_expand(Virtual *vwk, MFDB *src, long src_x, long src_y, MFDB *dst, long dst_x, long dst_y, long w, long h, long operation, long colour);

#endif
