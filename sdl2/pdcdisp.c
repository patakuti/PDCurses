/* PDCurses */

#include "pdcsdl.h"

#include <stdlib.h>
#include <string.h>

#ifdef PDC_WIDE
# include "../common/acsgr.h"
#else
# include "../common/acs437.h"
#endif

#define MAXRECT 200     /* maximum number of rects to queue up before
                           an update is forced; the number was chosen
                           arbitrarily */

#ifdef PDC_WIDE
/* Check if a Unicode code point is a wide (CJK) character that needs 2 cells */
static int _is_wide_char(chtype ch)
{
    unsigned long cmp = (unsigned long)(ch & A_CHARTEXT);

    /* East Asian Wide and Fullwidth characters */
    if (cmp >= 0x1100 &&
        (cmp <= 0x115f ||                    /* Hangul Jamo init. consonants */
         cmp == 0x2329 ||
         cmp == 0x232a ||
         (cmp >= 0x2e80 && cmp <= 0x4dbf &&
          cmp != 0x303f) ||                  /* CJK ... Yi */
         (cmp >= 0x4e00 && cmp <= 0xa4cf) || /* CJK Unified Ideographs, Yi */
         (cmp >= 0xa960 && cmp <= 0xa97f) || /* Hangul Jamo Extended-A */
         (cmp >= 0xac00 && cmp <= 0xd7a3) || /* Hangul Syllables */
         (cmp >= 0xf900 && cmp <= 0xfaff) || /* CJK Compatibility Ideographs */
         (cmp >= 0xfe10 && cmp <= 0xfe19) || /* Vertical forms */
         (cmp >= 0xfe30 && cmp <= 0xfe6f) || /* CJK Compatibility Forms */
         (cmp >= 0xff00 && cmp <= 0xff60) || /* Fullwidth Forms */
         (cmp >= 0xffe0 && cmp <= 0xffe6) ||
         (cmp >= 0x20000 && cmp <= 0x2fffd) ||
         (cmp >= 0x30000 && cmp <= 0x3fffd))) {
        return 1;
    }
    return 0;
}
#endif

static SDL_Rect uprect[MAXRECT];       /* table of rects to update */
static chtype oldch = (chtype)(-1);    /* current attribute */
static int rectcount = 0;              /* index into uprect */
static short foregr = -2, backgr = -2; /* current foreground, background */
static bool blinked_off = FALSE;

/* do the real updates on a delay */

void PDC_update_rects(void)
{
    int i;

    if (rectcount)
    {
        /* if the maximum number of rects has been reached, we're
           probably better off doing a full screen update */

        if (rectcount == MAXRECT)
            SDL_UpdateWindowSurface(pdc_window);
        else
        {
            int w = pdc_screen->w;
            int h = pdc_screen->h;

            for (i = 0; i < rectcount; i++)
            {
                if (uprect[i].x > w ||
                    uprect[i].y > h ||
                    !uprect[i].w || !uprect[i].h)
                {
                    if (i + 1 < rectcount)
                    {
                        memmove(uprect + i, uprect + i + 1,
                                (rectcount - i + 1) * sizeof(*uprect));
                        --i;
                    }
                    rectcount--;
                    continue;
                }

                if (uprect[i].x + uprect[i].w > w)
                    uprect[i].w = min(w, w - uprect[i].x);

                if (uprect[i].y + uprect[i].h > h)
                    uprect[i].h = min(h, h - uprect[i].y);
            }

            if (rectcount > 0)
                SDL_UpdateWindowSurfaceRects(pdc_window, uprect, rectcount);
        }

        rectcount = 0;
    }
}

/* set the font colors to match the chtype's attribute */

static void _set_attr(chtype ch)
{
    attr_t sysattrs = SP->termattrs;

#ifdef PDC_WIDE
    TTF_SetFontStyle(pdc_ttffont,
        ( ((ch & A_BOLD) && (sysattrs & A_BOLD)) ?
            TTF_STYLE_BOLD : 0) |
        ( ((ch & A_ITALIC) && (sysattrs & A_ITALIC)) ?
            TTF_STYLE_ITALIC : 0) );
#endif

    ch &= (A_COLOR|A_BOLD|A_BLINK|A_REVERSE);

    if (oldch != ch)
    {
        short newfg, newbg;

        if (SP->mono)
            return;

        pair_content(PAIR_NUMBER(ch), &newfg, &newbg);

        if ((ch & A_BOLD) && !(sysattrs & A_BOLD))
            newfg |= 8;
        if ((ch & A_BLINK) && !(sysattrs & A_BLINK))
            newbg |= 8;

        if (ch & A_REVERSE)
        {
            short tmp = newfg;
            newfg = newbg;
            newbg = tmp;
        }

        if (newfg != foregr)
        {
#ifndef PDC_WIDE
            SDL_SetPaletteColors(pdc_font->format->palette,
                                 pdc_color + newfg, pdc_flastc, 1);
#endif
            foregr = newfg;
        }

        if (newbg != backgr)
        {
#ifndef PDC_WIDE
            if (newbg == -1)
                SDL_SetColorKey(pdc_font, SDL_TRUE, 0);
            else
            {
                if (backgr == -1)
                    SDL_SetColorKey(pdc_font, SDL_FALSE, 0);

                SDL_SetPaletteColors(pdc_font->format->palette,
                                     pdc_color + newbg, 0, 1);
            }
#endif
            backgr = newbg;
        }

        oldch = ch;
    }
}

#ifdef PDC_WIDE

/* Draw some of the ACS_* "graphics" */

bool _grprint(chtype ch, SDL_Rect dest)
{
    Uint32 col = pdc_mapped[foregr];
    int hmid = (pdc_fheight - pdc_fthick) >> 1;
    int wmid = (pdc_fwidth - pdc_fthick) >> 1;

    switch (ch)
    {
    case ACS_ULCORNER:
        dest.h = pdc_fheight - hmid;
        dest.y += hmid;
        dest.w = pdc_fthick;
        dest.x += wmid;
        SDL_FillRect(pdc_screen, &dest, col);
        dest.w = pdc_fwidth - wmid;
        goto S1;
    case ACS_LLCORNER:
        dest.h = hmid;
        dest.w = pdc_fthick;
        dest.x += wmid;
        SDL_FillRect(pdc_screen, &dest, col);
        dest.w = pdc_fwidth - wmid;
        dest.y += hmid;
        goto S1;
    case ACS_URCORNER:
        dest.h = pdc_fheight - hmid;
        dest.w = pdc_fthick;
        dest.y += hmid;
        dest.x += wmid;
        SDL_FillRect(pdc_screen, &dest, col);
        dest.w = wmid;
        dest.x -= wmid;
        goto S1;
    case ACS_LRCORNER:
        dest.h = hmid + pdc_fthick;
        dest.w = pdc_fthick;
        dest.x += wmid;
        SDL_FillRect(pdc_screen, &dest, col);
        dest.w = wmid;
        dest.x -= wmid;
        dest.y += hmid;
        goto S1;
    case ACS_LTEE:
        dest.h = pdc_fthick;
        dest.w = pdc_fwidth - wmid;
        dest.x += wmid;
        dest.y += hmid;
        SDL_FillRect(pdc_screen, &dest, col);
        dest.w = pdc_fthick;
        dest.x -= wmid;
        goto VLINE;
    case ACS_RTEE:
        dest.w = wmid;
    case ACS_PLUS:
        dest.h = pdc_fthick;
        dest.y += hmid;
        SDL_FillRect(pdc_screen, &dest, col);
    VLINE:
        dest.h = pdc_fheight;
        dest.y -= hmid;
    case ACS_VLINE:
        dest.w = pdc_fthick;
        dest.x += wmid;
        goto DRAW;
    case ACS_TTEE:
        dest.h = pdc_fheight - hmid;
        dest.w = pdc_fthick;
        dest.x += wmid;
        dest.y += hmid;
        SDL_FillRect(pdc_screen, &dest, col);
        dest.w = pdc_fwidth;
        dest.x -= wmid;
        goto S1;
    case ACS_BTEE:
        dest.h = hmid;
        dest.w = pdc_fthick;
        dest.x += wmid;
        SDL_FillRect(pdc_screen, &dest, col);
        dest.w = pdc_fwidth;
        dest.x -= wmid;
    case ACS_HLINE:
        dest.y += hmid;
        goto S1;
    case ACS_S3:
        dest.y += hmid >> 1;
        goto S1;
    case ACS_S7:
        dest.y += hmid + (hmid >> 1);
        goto S1;
    case ACS_S9:
        dest.y += pdc_fheight - pdc_fthick;
    case ACS_S1:
    S1:
        dest.h = pdc_fthick;
    case ACS_BLOCK:
    DRAW:
        SDL_FillRect(pdc_screen, &dest, col);
        return TRUE;
    default: ;
    }

    return FALSE;  /* didn't draw it -- fall back to acs_map */
}

#endif

/* draw a cursor at (y, x) */

/* Calculate pixel X position for cursor/highlight operations.
 * Skips placeholders to match the logical column position used by Lynx. */
static int _col_to_pixel_x_cursor(int row, int col)
{
    int pixel_x = 0;
    int i;
    chtype *line;

    if (!curscr || row < 0 || row >= SP->lines || col < 0)
        return pdc_xoffset;

    line = curscr->_y[row];

    for (i = 0; i < col && i < SP->cols; i++) {
        chtype ch = line[i];
        /* Check if this is a placeholder (second cell of wide char) */
        if ((ch & A_CHARTEXT) == 0x00 && i > 0) {
            /* Skip - this is the second cell of a wide character. */
            continue;
        }
        pixel_x += pdc_fwidth;
    }

    return pixel_x + pdc_xoffset;
}

/* Calculate pixel X position for text drawing operations.
 * All cells contribute to position, including placeholders. */
static int _col_to_pixel_x_draw(int row, int col)
{
    (void)row;  /* Unused */
    return col * pdc_fwidth + pdc_xoffset;
}

void PDC_gotoyx(int row, int col)
{
    SDL_Rect src, dest;
    chtype ch;
    int oldrow, oldcol;
#ifdef PDC_WIDE
    Uint16 chstr[2] = {0, 0};
#endif

    PDC_LOG(("PDC_gotoyx() - called: row %d col %d from row %d col %d\n",
             row, col, SP->cursrow, SP->curscol));

    oldrow = SP->cursrow;
    oldcol = SP->curscol;

    /* clear the old cursor */

    PDC_transform_line(oldrow, oldcol, 1, curscr->_y[oldrow] + oldcol);

    if (!SP->visibility)
        return;

    /* draw a new cursor by overprinting the existing character in
       reverse, either the full cell (when visibility == 2) or the
       lowest quarter of it (when visibility == 1) */

    ch = curscr->_y[row][col] ^ A_REVERSE;

    _set_attr(ch);

    src.h = (SP->visibility == 1) ? pdc_fheight >> 2 : pdc_fheight;
    src.w = pdc_fwidth;

    dest.y = (row + 1) * pdc_fheight - src.h + pdc_yoffset;
    dest.x = _col_to_pixel_x_draw(row, col);  /* Use draw version for consistency */
    dest.h = src.h;
    dest.w = src.w;

#ifdef PDC_WIDE
    SDL_FillRect(pdc_screen, &dest, pdc_mapped[backgr]);

    if (!(SP->visibility == 2 && (ch & A_ALTCHARSET && !(ch & 0xff80)) &&
        _grprint(ch & (0x7f | A_ALTCHARSET), dest)))
    {
        if (ch & A_ALTCHARSET && !(ch & 0xff80))
            ch = acs_map[ch & 0x7f];

        chstr[0] = ch & A_CHARTEXT;

        pdc_font = TTF_RenderUNICODE_Blended(pdc_ttffont, chstr,
                                             pdc_color[foregr]);
        if (pdc_font)
        {
            int center = pdc_fwidth > pdc_font->w ?
                        (pdc_fwidth - pdc_font->w) >> 1 : 0;
            src.x = 0;
            src.y = pdc_fheight - src.h;
            dest.x += center;
            SDL_BlitSurface(pdc_font, &src, pdc_screen, &dest);
            dest.x -= center;
            SDL_FreeSurface(pdc_font);
            pdc_font = NULL;
        }
    }
#else
    if (ch & A_ALTCHARSET && !(ch & 0xff80))
        ch = acs_map[ch & 0x7f];

    src.x = (ch & 0xff) % 32 * pdc_fwidth;
    src.y = (ch & 0xff) / 32 * pdc_fheight + (pdc_fheight - src.h);

    SDL_BlitSurface(pdc_font, &src, pdc_screen, &dest);
#endif

    if (oldrow != row || oldcol != col)
    {
        SDL_Rect ime_rect;

        if (rectcount == MAXRECT)
            PDC_update_rects();

        uprect[rectcount++] = dest;

        /* Update IME candidate window position to follow cursor */
        ime_rect.x = _col_to_pixel_x_cursor(row, col);  /* Use cursor version */
        ime_rect.y = row * pdc_fheight + pdc_yoffset;
        ime_rect.w = pdc_fwidth;
        ime_rect.h = pdc_fheight;
        SDL_SetTextInputRect(&ime_rect);
    }
}

void _new_packet(attr_t attr, int lineno, int x, int len, const chtype *srcp)
{
    SDL_Rect src, dest, lastrect;
    int j;
#ifdef PDC_WIDE
    Uint16 chstr[2] = {0, 0};
#endif
    attr_t sysattrs = SP->termattrs;
    short hcol = SP->line_color;
    bool blink = blinked_off && (attr & A_BLINK) && (sysattrs & A_BLINK);

    if (rectcount == MAXRECT)
        PDC_update_rects();

#ifdef PDC_WIDE
    src.x = 0;
    src.y = 0;
#endif
    src.h = pdc_fheight;
    src.w = pdc_fwidth;

    dest.y = pdc_fheight * lineno + pdc_yoffset;
    dest.x = _col_to_pixel_x_draw(lineno, x);  /* Use draw version */
    dest.h = pdc_fheight;
    dest.w = pdc_fwidth * len;

    /* if the previous rect was just above this one, with the same width
       and horizontal position, then merge the new one with it instead
       of adding a new entry */

    if (rectcount)
        lastrect = uprect[rectcount - 1];

    if (rectcount && lastrect.x == dest.x && lastrect.w == dest.w)
    {
        if (lastrect.y + lastrect.h == dest.y)
            uprect[rectcount - 1].h = lastrect.h + pdc_fheight;
        else
            if (lastrect.y != dest.y)
                uprect[rectcount++] = dest;
    }
    else
        uprect[rectcount++] = dest;

    _set_attr(attr);

    if (backgr == -1)
        SDL_BlitSurface(pdc_tileback, &dest, pdc_screen, &dest);
#ifdef PDC_WIDE
    else
        SDL_FillRect(pdc_screen, &dest, pdc_mapped[backgr]);
#endif

    if (hcol == -1)
        hcol = foregr;

    for (j = 0; j < len; j++)
    {
        chtype ch = srcp[j];

        if (blink)
            ch = ' ';

        dest.w = pdc_fwidth;

        if (ch & A_ALTCHARSET && !(ch & 0xff80))
        {
#ifdef PDC_WIDE
            if (_grprint(ch & (0x7f | A_ALTCHARSET), dest))
            {
                dest.x += pdc_fwidth;
                continue;
            }
#endif
            ch = acs_map[ch & 0x7f];
        }

#ifdef PDC_WIDE
        {
            int is_wide = _is_wide_char(ch);

            ch &= A_CHARTEXT;

            /* Skip placeholder cells (value 0) inserted after wide characters.
               The wide character has already drawn over this area. */
            if (ch == 0) {
                dest.x += pdc_fwidth;
                continue;
            }

            if (ch != ' ')
            {
                if (chstr[0] != ch)
                {
                    chstr[0] = ch;

                    if (pdc_font)
                        SDL_FreeSurface(pdc_font);

                    pdc_font = TTF_RenderUNICODE_Blended(pdc_ttffont, chstr,
                                                         pdc_color[foregr]);
                }

                if (pdc_font)
                {
                    if (is_wide) {
                        /* Wide character: clear 2 cells of background first,
                           then render the full glyph (use NULL for src to blit entire surface) */
                        SDL_Rect wide_bg = dest;
                        wide_bg.w = pdc_fwidth * 2;
                        if (backgr != -1)
                            SDL_FillRect(pdc_screen, &wide_bg, pdc_mapped[backgr]);
                        SDL_BlitSurface(pdc_font, NULL, pdc_screen, &dest);
                    } else {
                        /* Narrow character: center within 1 cell */
                        int center = pdc_fwidth > pdc_font->w ?
                            (pdc_fwidth - pdc_font->w) >> 1 : 0;
                        dest.x += center;
                        SDL_BlitSurface(pdc_font, NULL, pdc_screen, &dest);
                        dest.x -= center;
                    }
                }
            }
        }
#else
        src.x = (ch & 0xff) % 32 * pdc_fwidth;
        src.y = (ch & 0xff) / 32 * pdc_fheight;

        SDL_BlitSurface(pdc_font, &src, pdc_screen, &dest);
#endif

        if (!blink && (attr & (A_LEFT | A_RIGHT)))
        {
            dest.w = pdc_fthick;

            if (attr & A_LEFT)
                SDL_FillRect(pdc_screen, &dest, pdc_mapped[hcol]);

            if (attr & A_RIGHT)
            {
                dest.x += pdc_fwidth - pdc_fthick;
                SDL_FillRect(pdc_screen, &dest, pdc_mapped[hcol]);
                dest.x -= pdc_fwidth - pdc_fthick;
            }
        }

        dest.x += pdc_fwidth;
    }

#ifdef PDC_WIDE
    if (pdc_font)
    {
        SDL_FreeSurface(pdc_font);
        pdc_font = NULL;
    }
#endif

    if (!blink && (attr & A_UNDERLINE))
    {
        dest.y += pdc_fheight - pdc_fthick;
        dest.x = pdc_fwidth * x + pdc_xoffset;
        dest.h = pdc_fthick;
        dest.w = pdc_fwidth * len;

        SDL_FillRect(pdc_screen, &dest, pdc_mapped[hcol]);
    }
}

/* update the given physical line to look like the corresponding line in
   curscr */

void PDC_transform_line(int lineno, int x, int len, const chtype *srcp)
{
    attr_t old_attr, attr;
    int i, j;

    PDC_LOG(("PDC_transform_line() - called: lineno=%d\n", lineno));

    old_attr = *srcp & (A_ATTRIBUTES ^ A_ALTCHARSET);

    for (i = 1, j = 1; j < len; i++, j++)
    {
        attr = srcp[i] & (A_ATTRIBUTES ^ A_ALTCHARSET);

        if (attr != old_attr)
        {
            _new_packet(old_attr, lineno, x, i, srcp);
            old_attr = attr;
            srcp += i;
            x += i;
            i = 0;
        }
    }

    _new_packet(old_attr, lineno, x, i, srcp);
}

static Uint32 _blink_timer(Uint32 interval, void *param)
{
    SDL_Event event;

    event.type = SDL_USEREVENT;
    SDL_PushEvent(&event);
    return(interval);
}

void PDC_blink_text(void)
{
    static SDL_TimerID blinker_id = 0;
    int i, j, k;

    oldch = (chtype)(-1);

    if (!(SP->termattrs & A_BLINK))
    {
        SDL_RemoveTimer(blinker_id);
        blinker_id = 0;
    }
    else if (!blinker_id)
    {
        blinker_id = SDL_AddTimer(500, _blink_timer, NULL);
        blinked_off = TRUE;
    }

    blinked_off = !blinked_off;

    for (i = 0; i < SP->lines; i++)
    {
        const chtype *srcp = curscr->_y[i];

        for (j = 0; j < SP->cols; j++)
            if (srcp[j] & A_BLINK)
            {
                k = j;
                while (k < SP->cols && (srcp[k] & A_BLINK))
                    k++;
                PDC_transform_line(i, j, k - j, srcp + j);
                j = k;
            }
    }

    oldch = (chtype)(-1);

    PDC_doupdate();
}

/* Render IME composition text as overlay at cursor position */
static void _render_composition_overlay(void)
{
#ifdef PDC_WIDE
    static int last_composition_width = 0;  /* Remember previous width */
    static int last_cursor_x = 0;
    static int last_cursor_y = 0;

    if (SP)
    {
        int cursor_x = SP->curscol;
        int cursor_y = SP->cursrow;
        int base_x = _col_to_pixel_x_cursor(cursor_y, cursor_x);  /* Use cursor version */
        int base_y = cursor_y * pdc_fheight + pdc_yoffset;

        /* Clear previous composition area if it was larger */
        if (last_composition_width > 0)
        {
            int last_base_x = _col_to_pixel_x_cursor(last_cursor_y, last_cursor_x);  /* Use cursor version */
            int last_base_y = last_cursor_y * pdc_fheight + pdc_yoffset;

            /* Restore the area by redrawing from curscr */
            int start_col = last_cursor_x;
            int num_cols = (last_composition_width + pdc_fwidth - 1) / pdc_fwidth + 1;
            if (start_col + num_cols > SP->cols)
                num_cols = SP->cols - start_col;
            if (num_cols > 0 && last_cursor_y < SP->lines)
            {
                PDC_transform_line(last_cursor_y, start_col, num_cols,
                                   curscr->_y[last_cursor_y] + start_col);
            }
        }

        if (pdc_composition_text[0])
        {
            SDL_Rect dest;
            SDL_Surface *text_surface;
            SDL_Color fg_color = {255, 255, 0, 255};  /* Yellow for composition */
            SDL_Color bg_color = {0, 0, 128, 255};    /* Dark blue background */
            int text_width, text_height;

            dest.x = base_x;
            dest.y = base_y;

            /* Get text dimensions */
            TTF_SizeUTF8(pdc_ttffont, pdc_composition_text, &text_width, &text_height);

            /* Draw background rectangle */
            SDL_Rect bg_rect = {dest.x, dest.y, text_width + 4, pdc_fheight};
            SDL_FillRect(pdc_screen, &bg_rect,
                         SDL_MapRGB(pdc_screen->format, bg_color.r, bg_color.g, bg_color.b));

            /* Render composition text */
            text_surface = TTF_RenderUTF8_Blended(pdc_ttffont, pdc_composition_text, fg_color);
            if (text_surface)
            {
                dest.x += 2;  /* Small padding */
                SDL_BlitSurface(text_surface, NULL, pdc_screen, &dest);
                SDL_FreeSurface(text_surface);
            }

            /* Draw underline to indicate composition state */
            SDL_Rect underline = {dest.x - 2, dest.y + pdc_fheight - 2, text_width + 4, 2};
            SDL_FillRect(pdc_screen, &underline,
                         SDL_MapRGB(pdc_screen->format, fg_color.r, fg_color.g, fg_color.b));

            /* Remember for next time */
            last_composition_width = text_width + 4;
            last_cursor_x = cursor_x;
            last_cursor_y = cursor_y;
        }
        else
        {
            last_composition_width = 0;
        }
    }
#endif
}

void PDC_doupdate(void)
{
    static int window_shown = 0;

    PDC_update_rects();

    /* Render IME composition overlay if active */
    _render_composition_overlay();

    /* Force immediate screen update to ensure IME input is visible */
    SDL_UpdateWindowSurface(pdc_window);

    /* Show window after first update to avoid black screen on startup */
    if (!window_shown)
    {
        SDL_ShowWindow(pdc_window);
        window_shown = 1;
    }
}

void PDC_pump_and_peep(void)
{
    SDL_Event event;

    if (SDL_PollEvent(&event))
    {
        if (SDL_WINDOWEVENT == event.type &&
            (SDL_WINDOWEVENT_RESTORED == event.window.event ||
             SDL_WINDOWEVENT_EXPOSED == event.window.event ||
             SDL_WINDOWEVENT_SHOWN == event.window.event))
        {
            SDL_UpdateWindowSurface(pdc_window);
            rectcount = 0;
        }
        else
            SDL_PushEvent(&event);
    }
}
