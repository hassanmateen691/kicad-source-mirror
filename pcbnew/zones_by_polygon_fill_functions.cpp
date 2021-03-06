/*
 * @file zones_by_polygon_fill_functions.cpp
 */

/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 2009 Jean-Pierre Charras jp.charras at wanadoo.fr
 * Copyright (C) 2018 KiCad Developers, see AUTHORS.txt for contributors.
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

#include <wx/progdlg.h>

#include <fctsys.h>
#include <pgm_base.h>
#include <class_drawpanel.h>
#include <class_draw_panel_gal.h>
#include <ratsnest_data.h>
#include <pcb_edit_frame.h>
#include <macros.h>

#include <tool/tool_manager.h>
#include <tools/pcb_actions.h>

#include <class_board.h>
#include <class_track.h>
#include <class_zone.h>

#include <pcbnew.h>
#include <zones.h>

#include <board_commit.h>

#include <widgets/progress_reporter.h>
#include <zone_filler.h>


void PCB_EDIT_FRAME::Fill_All_Zones()
{
    auto toolMgr = GetToolManager();
    toolMgr->RunAction( PCB_ACTIONS::zoneFillAll, true );
}


void PCB_EDIT_FRAME::Check_All_Zones( wxWindow* aActiveWindow )
{
    if( !m_ZoneFillsDirty )
        return;

    std::vector<ZONE_CONTAINER*> toFill;

    for( auto zone : GetBoard()->Zones() )
        toFill.push_back(zone);

    BOARD_COMMIT commit( this );

    std::unique_ptr<WX_PROGRESS_REPORTER> progressReporter(
            new WX_PROGRESS_REPORTER( aActiveWindow, _( "Checking Zones" ), 4 ) );

    ZONE_FILLER filler( GetBoard(), &commit );
    filler.SetProgressReporter( progressReporter.get() );

    if( filler.Fill( toFill, true ) )
    {
        m_ZoneFillsDirty = false;

        GetGalCanvas()->ForceRefresh();
        GetCanvas()->Refresh();
    }
}
