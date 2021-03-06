/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2013 CERN
 * Copyright (C) 2016-2019 KiCad Developers, see AUTHORS.txt for contributors.
 * @author Jean-Pierre Charras, jp.charras at wanadoo.fr
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, you may find one here:
 * http://www.gnu.org/licenses/old-licenses/gpl-2.0.html
 * or you may search the http://www.gnu.org website for the version 2 license,
 * or you may write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA
 */

#include <fctsys.h>
#include <common.h>
#include <kicad_device_context.h>
#include <id.h>

#include <class_drawpanel.h>
#include <pl_editor_frame.h>
#include <hotkeys.h>
#include <pl_editor_id.h>


// Remark: the hotkey message info is used as keyword in hotkey config files and
// as comments in help windows, therefore translated only when displayed
// they are marked _HKI to be extracted by translation tools
// See hotkeys_basic.h for more info


/* How to add a new hotkey:
 * add a new id in the enum hotkey_id_command like MY_NEW_ID_FUNCTION.
 * add a new EDA_HOTKEY entry like:
 * static EDA_HOTKEY HkMyNewEntry(_HKI("Command Label"), MY_NEW_ID_FUNCTION, default key value);
 * 'Command Label' is the name used in hotkey list display, and the identifier in the
 * hotkey list file
 * 'MY_NEW_ID_FUNCTION' is the id event function used in the switch in OnHotKey() function.
 * 'Default key value' is the default hotkey for this command.
 * Can be overrided by the user hotkey list
 * Add the 'HkMyNewEntry' pointer in the  s_PlEditor_Hotkey_List list
 * Add the new code in the switch in OnHotKey() function.
 *
 *  Note: If an hotkey is a special key, be sure the corresponding wxWidget keycode (WXK_XXXX)
 *  is handled in the hotkey_name_descr s_Hotkey_Name_List list (see hotkeys_basic.cpp)
 *  and see this list for some ascii keys (space ...)
 */

// Hotkey list:

// mouse click command:
static EDA_HOTKEY HkMouseLeftClick( _HKI( "Mouse Left Click" ), HK_LEFT_CLICK, WXK_RETURN, 0 );
static EDA_HOTKEY HkMouseLeftDClick( _HKI( "Mouse Left Double Click" ), HK_LEFT_DCLICK,
                                     WXK_END, 0 );

static EDA_HOTKEY HkResetLocalCoord( _HKI( "Reset Local Coordinates" ), HK_RESET_LOCAL_COORD, ' ' );
static EDA_HOTKEY HkZoomAuto( _HKI( "Zoom Auto" ), HK_ZOOM_AUTO, WXK_HOME );
static EDA_HOTKEY HkZoomCenter( _HKI( "Zoom Center" ), HK_ZOOM_CENTER, WXK_F4 );
static EDA_HOTKEY HkZoomRedraw( _HKI( "Zoom Redraw" ), HK_ZOOM_REDRAW, WXK_F3 );
static EDA_HOTKEY HkZoomOut( _HKI( "Zoom Out" ), HK_ZOOM_OUT, WXK_F2 );
static EDA_HOTKEY HkZoomIn( _HKI( "Zoom In" ), HK_ZOOM_IN, WXK_F1 );
static EDA_HOTKEY HkZoomSelection( _HKI( "Zoom to Selection" ), HK_ZOOM_SELECTION,
                                   GR_KB_CTRL + WXK_F5 );
static EDA_HOTKEY HkHelp( _HKI( "List Hotkeys" ), HK_HELP, GR_KB_CTRL + WXK_F1 );
static EDA_HOTKEY HkMoveItem( _HKI( "Move Item" ), HK_MOVE, 'M' );
static EDA_HOTKEY HkDeleteItem( _HKI( "Delete Item" ), HK_DELETE_ITEM, WXK_DELETE );

// Common: hotkeys_basic.h
static EDA_HOTKEY HkUndo( _HKI( "Undo" ), HK_UNDO, GR_KB_CTRL + 'Z', (int) wxID_UNDO );
static EDA_HOTKEY HkRedo( _HKI( "Redo" ), HK_REDO, GR_KB_CTRL + 'Y', (int) wxID_REDO );

static EDA_HOTKEY HkCut( _HKI( "Cut" ), HK_CUT, GR_KB_CTRL + 'X' );
static EDA_HOTKEY HkCopy( _HKI( "Copy" ), HK_COPY, GR_KB_CTRL + 'C' );
static EDA_HOTKEY HkPaste( _HKI( "Paste" ), HK_PASTE, GR_KB_CTRL + 'V' );

static EDA_HOTKEY HkNew( _HKI( "New" ), HK_NEW, GR_KB_CTRL + 'N', (int) wxID_NEW );
static EDA_HOTKEY HkOpen( _HKI( "Open" ), HK_OPEN, GR_KB_CTRL + 'O', (int) wxID_OPEN );
static EDA_HOTKEY HkSave( _HKI( "Save" ), HK_SAVE, GR_KB_CTRL + 'S', (int) wxID_SAVE );
static EDA_HOTKEY HkSaveAs( _HKI( "Save As" ), HK_SAVEAS, GR_KB_CTRL + GR_KB_SHIFT + 'S',
                            (int) wxID_SAVEAS );
static EDA_HOTKEY HkPrint( _HKI( "Print" ), HK_PRINT, GR_KB_CTRL + 'P', (int) wxID_PRINT );
static EDA_HOTKEY HkPreferences( _HKI( "Preferences" ), HK_PREFERENCES, GR_KB_CTRL + ',', (int) wxID_PREFERENCES );

// List of common hotkey descriptors
EDA_HOTKEY* s_Common_Hotkey_List[] =
{
    &HkNew,            &HkOpen,              &HkSave,              &HkSaveAs,       &HkPrint,
    &HkUndo,           &HkRedo,
    &HkCut,            &HkCopy,              &HkPaste,             &HkDeleteItem,
    &HkZoomIn,         &HkZoomOut,           &HkZoomRedraw,        &HkZoomCenter,
    &HkZoomAuto,       &HkZoomSelection,     &HkResetLocalCoord,
    &HkHelp,           &HkPreferences,
    &HkMouseLeftClick, &HkMouseLeftDClick,
    NULL
};

EDA_HOTKEY* s_PlEditor_Hotkey_List[] =
{
    &HkMoveItem,
    &HkDeleteItem,
    NULL
};

// Titles for hotkey editor and hotkey display
static wxString commonSectionTitle( _HKI( "Common" ) );

// list of sections and corresponding hotkey list for Pl_Editor
// (used to create an hotkey config file)
static wxString s_PlEditorSectionTag( wxT( "[pl_editor]" ) );
static wxString s_PlEditorSectionTitle( _HKI( "Page Layout Editor" ) );

struct EDA_HOTKEY_CONFIG PlEditorHotkeysDescr[] =
{
    { &g_CommonSectionTag,    s_Common_Hotkey_List,     &commonSectionTitle    },
    { &s_PlEditorSectionTag,  s_PlEditor_Hotkey_List,   &s_PlEditorSectionTitle  },
    { NULL,                   NULL,                     NULL                     }
};


EDA_HOTKEY* PL_EDITOR_FRAME::GetHotKeyDescription( int aCommand ) const
{
    EDA_HOTKEY* HK_Descr = GetDescriptorFromCommand( aCommand, s_Common_Hotkey_List );

    if( HK_Descr == NULL )
        HK_Descr = GetDescriptorFromCommand( aCommand, s_PlEditor_Hotkey_List );

    return HK_Descr;
}


