/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
 * URL: http://schismtracker.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */
 
/* this header supplies vista+ structures for mingw32 */
#ifndef SYS_WIN32_VISTA_H_
#define SYS_WIN32_VISTA_H_

typedef int (WINAPI *DTT_CALLBACK_PROC)(HDC,LPWSTR,int,RECT*,UINT,LPARAM);

typedef struct _DTTOPTS {
    DWORD dwSize;
    DWORD dwFlags;
    COLORREF crText;
    COLORREF crBorder;
    COLORREF crShadow;
    int iTextShadowType;
    POINT ptShadowOffset;
    int iBorderSize;
    int iFontPropId;
    int iColorPropId;
    int iStateId;
    BOOL fApplyOverlay;
    int iGlowSize;
    DTT_CALLBACK_PROC pfnDrawTextCallback;
    LPARAM lParam;
} DTTOPTS, *PDTTOPTS;

/* DTTOPTS.dwFlags bits */
#ifndef DTT_TEXTCOLOR
#define DTT_TEXTCOLOR    0x00000001
#endif
#ifndef DTT_BORDERCOLOR
#define DTT_BORDERCOLOR  0x00000002
#endif
#ifndef DTT_SHADOWCOLOR
#define DTT_SHADOWCOLOR  0x00000004
#endif
#ifndef DTT_SHADOWTYPE
#define DTT_SHADOWTYPE   0x00000008
#endif
#ifndef DTT_SHADOWOFFSET
#define DTT_SHADOWOFFSET 0x00000010
#endif
#ifndef DTT_BORDERSIZE
#define DTT_BORDERSIZE   0x00000020
#endif
#ifndef DTT_FONTPROP
#define DTT_FONTPROP     0x00000040
#endif
#ifndef DTT_COLORPROP
#define DTT_COLORPROP    0x00000080
#endif
#ifndef DTT_STATEID
#define DTT_STATEID      0x00000100
#endif
#ifndef DTT_CALCRECT
#define DTT_CALCRECT     0x00000200
#endif
#ifndef DTT_APPLYOVERLAY
#define DTT_APPLYOVERLAY 0x00000400
#endif
#ifndef DTT_GLOWSIZE
#define DTT_GLOWSIZE     0x00000800
#endif
#ifndef DTT_CALLBACK
#define DTT_CALLBACK     0x00001000
#endif
#ifndef DTT_COMPOSITED
#define DTT_COMPOSITED   0x00002000
#endif
#ifndef DTT_VALIDBITS
#define DTT_VALIDBITS    0x00003fff
#endif

#ifndef ODS_SELECTED
#define ODS_SELECTED     0x0001 /* Selected */
#endif
#ifndef ODS_GRAYED
#define ODS_GRAYED       0x0002 /* Grayed (Menus only) */
#endif
#ifndef ODS_DISABLED
#define ODS_DISABLED     0x0004 /* Disabled */
#endif
#ifndef ODS_CHECKED
#define ODS_CHECKED      0x0008 /* Checked (Menus only) */
#endif
#ifndef ODS_FOCUS
#define ODS_FOCUS        0x0010 /* Has focus */
#endif
#ifndef ODS_DEFAULT
#define ODS_DEFAULT      0x0020 /* Default */
#endif
#ifndef ODS_HOTLIGHT
#define ODS_HOTLIGHT     0x0040 /* Highlighted when under mouse */
#endif
#ifndef ODS_INACTIVE
#define ODS_INACTIVE     0x0080 /* Inactive */
#endif
#ifndef ODS_NOACCEL
#define ODS_NOACCEL      0x0100 /* No keyboard accelerator */
#endif
#ifndef ODS_NOFOCUSRECT
#define ODS_NOFOCUSRECT  0x0200 /* No focus rectangle */
#endif
#ifndef ODS_COMBOBOXEDIT
#define ODS_COMBOBOXEDIT 0x1000 /* Edit of a combo box */
#endif

#ifndef DT_TOP
#define DT_TOP                  0x00000000
#endif
#ifndef DT_LEFT
#define DT_LEFT                 0x00000000
#endif
#ifndef DT_CENTER
#define DT_CENTER               0x00000001
#endif
#ifndef DT_RIGHT
#define DT_RIGHT                0x00000002
#endif
#ifndef DT_VCENTER
#define DT_VCENTER              0x00000004
#endif
#ifndef DT_BOTTOM
#define DT_BOTTOM               0x00000008
#endif
#ifndef DT_WORDBREAK
#define DT_WORDBREAK            0x00000010
#endif
#ifndef DT_SINGLELINE
#define DT_SINGLELINE           0x00000020
#endif
#ifndef DT_EXPANDTABS
#define DT_EXPANDTABS           0x00000040
#endif
#ifndef DT_TABSTOP
#define DT_TABSTOP              0x00000080
#endif
#ifndef DT_NOCLIP
#define DT_NOCLIP               0x00000100
#endif
#ifndef DT_EXTERNALLEADING
#define DT_EXTERNALLEADING      0x00000200
#endif
#ifndef DT_CALCRECT
#define DT_CALCRECT             0x00000400
#endif
#ifndef DT_NOPREFIX
#define DT_NOPREFIX             0x00000800
#endif
#ifndef DT_INTERNAL
#define DT_INTERNAL             0x00001000
#endif
#ifndef DT_EDITCONTROL
#define DT_EDITCONTROL          0x00002000
#endif
#ifndef DT_PATH_ELLIPSIS
#define DT_PATH_ELLIPSIS        0x00004000
#endif
#ifndef DT_END_ELLIPSIS
#define DT_END_ELLIPSIS         0x00008000
#endif
#ifndef DT_MODIFYSTRING
#define DT_MODIFYSTRING         0x00010000
#endif
#ifndef DT_RTLREADING
#define DT_RTLREADING           0x00020000
#endif
#ifndef DT_WORD_ELLIPSIS
#define DT_WORD_ELLIPSIS        0x00040000
#endif
#ifndef DT_NOFULLWIDTHCHARBREAK
#define DT_NOFULLWIDTHCHARBREAK 0x00080000
#endif
#ifndef DT_HIDEPREFIX
#define DT_HIDEPREFIX           0x00100000
#endif
#ifndef DT_PREFIXONLY
#define DT_PREFIXONLY           0x00200000
#endif

enum BARITEMSTATES {
    MBI_NORMAL = 1,
    MBI_HOT = 2,
    MBI_PUSHED = 3,
    MBI_DISABLED = 4,
    MBI_DISABLEDHOT = 5,
    MBI_DISABLEDPUSHED = 6,
};

enum MENUPARTS {
    MENU_MENUITEM_TMSCHEMA = 1,
    MENU_MENUDROPDOWN_TMSCHEMA = 2,
    MENU_MENUBARITEM_TMSCHEMA = 3,
    MENU_MENUBARDROPDOWN_TMSCHEMA = 4,
    MENU_CHEVRON_TMSCHEMA = 5,
    MENU_SEPARATOR_TMSCHEMA = 6,
    MENU_BARBACKGROUND = 7,
    MENU_BARITEM = 8,
    MENU_POPUPBACKGROUND = 9,
    MENU_POPUPBORDERS = 10,
    MENU_POPUPCHECK = 11,
    MENU_POPUPCHECKBACKGROUND = 12,
    MENU_POPUPGUTTER = 13,
    MENU_POPUPITEM = 14,
    MENU_POPUPSEPARATOR = 15,
    MENU_POPUPSUBMENU = 16,
    MENU_SYSTEMCLOSE = 17,
    MENU_SYSTEMMAXIMIZE = 18,
    MENU_SYSTEMMINIMIZE = 19,
    MENU_SYSTEMRESTORE = 20,
};

// This sucks
#define HTHEME HANDLE

#endif /* SYS_WIN32_VSSTYLES_H_ */