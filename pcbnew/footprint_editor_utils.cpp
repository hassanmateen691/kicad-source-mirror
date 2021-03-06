/*
 * This program source code file is part of KiCad, a free EDA CAD application.
 *
 * Copyright (C) 1992-2018 KiCad Developers, see AUTHORS.txt for contributors.
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

/**
 * @file footprint_editor_utils.cpp
 */

#include <fctsys.h>
#include <kiface_i.h>
#include <kiway.h>
#include <kiway_express.h>
#include <class_drawpanel.h>
#include <pcb_draw_panel_gal.h>
#include <confirm.h>
#include <gestfich.h>
#include <pgm_base.h>
#include <trigo.h>
#include <3d_viewer/eda_3d_viewer.h>
#include <kicad_device_context.h>
#include <macros.h>
#include <invoke_pcb_dialog.h>
#include <pcb_layer_widget.h>
#include <board_commit.h>
#include <view/view.h>
#include <class_board.h>
#include <class_module.h>
#include <class_edge_mod.h>

#include <ratsnest_data.h>
#include <pcbnew.h>
#include <pcbnew_id.h>
#include <footprint_edit_frame.h>
#include <footprint_viewer_frame.h>
#include <footprint_tree_pane.h>
#include <fp_lib_table.h>
#include <widgets/lib_tree.h>
#include <collectors.h>
#include <tool/tool_manager.h>
#include <tools/pcb_actions.h>

#include <dialog_edit_footprint_for_fp_editor.h>
#include <dialog_move_exact.h>
#include <dialog_create_array.h>
#include <wildcards_and_files_ext.h>
#include <menus_helpers.h>
#include <footprint_wizard_frame.h>
#include <config_params.h>

#include <functional>
using namespace std::placeholders;


void FOOTPRINT_EDIT_FRAME::LoadModuleFromBoard( wxCommandEvent& event )
{
    Load_Module_From_BOARD( NULL );
}


void FOOTPRINT_EDIT_FRAME::LoadModuleFromLibrary( LIB_ID aFPID)
{
    bool is_last_fp_from_brd = IsCurrentFPFromBoard();

    MODULE* module = LoadFootprint( aFPID );

    if( !module )
        return;

    if( !Clear_Pcb( true ) )
        return;

    SetCrossHairPosition( wxPoint( 0, 0 ) );
    AddModuleToBoard( module );

    if( GetBoard()->m_Modules )
    {
        GetBoard()->m_Modules->ClearFlags();

        // if either m_Reference or m_Value are gone, reinstall them -
        // otherwise you cannot see what you are doing on board
        TEXTE_MODULE* ref = &GetBoard()->m_Modules->Reference();
        TEXTE_MODULE* val = &GetBoard()->m_Modules->Value();

        if( val && ref )
        {
            ref->SetType( TEXTE_MODULE::TEXT_is_REFERENCE );    // just in case ...

            if( ref->GetLength() == 0 )
                ref->SetText( wxT( "Ref**" ) );

            val->SetType( TEXTE_MODULE::TEXT_is_VALUE );        // just in case ...

            if( val->GetLength() == 0 )
                val->SetText( wxT( "Val**" ) );
        }
    }

    Zoom_Automatique( false );

    Update3DView();

    GetScreen()->ClrModify();

    updateView();
    m_canvas->Refresh();

    // Update the bitmap of the ID_MODEDIT_SAVE tool if needed.
    if( is_last_fp_from_brd )
        ReCreateHToolbar();

    m_treePane->GetLibTree()->ExpandLibId( aFPID );
    m_treePane->GetLibTree()->CenterLibId( aFPID );
    m_treePane->GetLibTree()->Refresh();        // update highlighting
}


void FOOTPRINT_EDIT_FRAME::Process_Special_Functions( wxCommandEvent& event )
{
    int        id = event.GetId();
    wxPoint    pos;

    INSTALL_UNBUFFERED_DC( dc, m_canvas );

    wxGetMousePosition( &pos.x, &pos.y );

    pos.y += 20;

    switch( id )
    {
    case wxID_CUT:
    case wxID_COPY:
    case ID_TOOLBARH_PCB_SELECT_LAYER:
    case ID_MODEDIT_PAD_SETTINGS:
        break;

    default:
        if( m_canvas->IsMouseCaptured() )
        {
            //  for all other commands: stop the move in progress
            m_canvas->CallEndMouseCapture( &dc );
        }

        SetNoToolSelected();

        break;
    }

    switch( id )
    {
    case ID_OPEN_MODULE_VIEWER:
        {
            FOOTPRINT_VIEWER_FRAME* viewer = (FOOTPRINT_VIEWER_FRAME*) Kiway().Player( FRAME_PCB_MODULE_VIEWER, false );

            if( !viewer )
            {
                viewer = (FOOTPRINT_VIEWER_FRAME*) Kiway().Player( FRAME_PCB_MODULE_VIEWER, true );
                viewer->Show( true );
                viewer->Zoom_Automatique( false );
            }
            else
            {
                // On Windows, Raise() does not bring the window on screen, when iconized
                if( viewer->IsIconized() )
                    viewer->Iconize( false );

                viewer->Raise();

                // Raising the window does not set the focus on Linux.  This should work on
                // any platform.
                if( wxWindow::FindFocus() != viewer )
                    viewer->SetFocus();
            }
        }
        break;

    case ID_MODEDIT_DELETE_PART:
        if( DeleteModuleFromLibrary( getTargetFPID(), true ) )
        {
            if( getTargetFPID() == GetLoadedFPID() )
                Clear_Pcb( false );

            SyncLibraryTree( true );
        }
        break;

    case ID_MODEDIT_NEW_MODULE:
        {
            LIB_ID selected = m_treePane->GetLibTree()->GetSelectedLibId();
            MODULE* module = CreateNewModule( wxEmptyString );

            if( !module )
                break;

            if( !Clear_Pcb( true ) )
                break;

            SetCrossHairPosition( wxPoint( 0, 0 ) );
            AddModuleToBoard( module );

            // Initialize data relative to nets and netclasses (for a new
            // module the defaults are used)
            // This is mandatory to handle and draw pads
            GetBoard()->BuildListOfNets();
            module->SetPosition( wxPoint( 0, 0 ) );

            if( GetBoard()->m_Modules )
                GetBoard()->m_Modules->ClearFlags();

            Zoom_Automatique( false );
            GetScreen()->SetModify();

            // If selected from the library tree then go ahead and save it there
            if( !selected.GetLibNickname().empty() )
            {
                LIB_ID fpid = module->GetFPID();
                fpid.SetLibNickname( selected.GetLibNickname() );
                module->SetFPID( fpid );
                SaveFootprint( module );
                GetScreen()->ClrModify();
            }

            updateView();
            m_canvas->Refresh();
            Update3DView();

            SyncLibraryTree( false );
        }
        break;

    case ID_MODEDIT_NEW_MODULE_FROM_WIZARD:
        {
            LIB_ID selected = m_treePane->GetLibTree()->GetSelectedLibId();

            if( GetScreen()->IsModify() && !GetBoard()->IsEmpty() )
            {
                if( !HandleUnsavedChanges( this, _( "The current footprint has been modified.  Save changes?" ),
                                           [&]()->bool { return SaveFootprint( GetBoard()->m_Modules ); } ) )
                {
                    break;
                }
            }

            FOOTPRINT_WIZARD_FRAME* wizard = (FOOTPRINT_WIZARD_FRAME*) Kiway().Player(
                        FRAME_PCB_FOOTPRINT_WIZARD, true, this );

            if( wizard->ShowModal( NULL, this ) )
            {
                // Creates the new footprint from python script wizard
                MODULE* module = wizard->GetBuiltFootprint();

                if( module == NULL )        // i.e. if create module command aborted
                    break;

                Clear_Pcb( false );

                SetCrossHairPosition( wxPoint( 0, 0 ) );

                //  Add the new object to board
                AddModuleToBoard( module );

                // Initialize data relative to nets and netclasses (for a new
                // module the defaults are used)
                // This is mandatory to handle and draw pads
                GetBoard()->BuildListOfNets();
                module->SetPosition( wxPoint( 0, 0 ) );
                module->ClearFlags();

                Zoom_Automatique( false );
                GetScreen()->SetModify();

                // If selected from the library tree then go ahead and save it there
                if( !selected.GetLibNickname().empty() )
                {
                    LIB_ID fpid = module->GetFPID();
                    fpid.SetLibNickname( selected.GetLibNickname() );
                    module->SetFPID( fpid );
                    SaveFootprint( module );
                    GetScreen()->ClrModify();
                }

                updateView();
                m_canvas->Refresh();
                Update3DView();

                SyncLibraryTree( false );
            }

            wizard->Destroy();
        }
        break;

    case ID_MODEDIT_SAVE:
        if( getTargetFPID() == GetLoadedFPID() )
        {
            if( SaveFootprint( GetBoard()->m_Modules ) )
            {
                m_toolManager->GetView()->Update( GetBoard()->m_Modules );

                if( GetGalCanvas() )
                    GetGalCanvas()->ForceRefresh();
                else
                    GetCanvas()->Refresh();

                GetScreen()->ClrModify();
            }
        }

        m_treePane->GetLibTree()->Refresh();
        break;

    case ID_MODEDIT_SAVE_AS:
        if( getTargetFPID().GetLibItemName().empty() )
        {
            // Save Library As
            const wxString& src_libNickname = getTargetFPID().GetLibNickname();
            wxString src_libFullName = Prj().PcbFootprintLibs()->GetFullURI( src_libNickname );

            if( SaveLibraryAs( src_libFullName ) )
                SyncLibraryTree( true );
        }
        else if( getTargetFPID() == GetLoadedFPID() )
        {
            // Save Board Footprint As
            MODULE* footprint = GetBoard()->m_Modules;

            if( footprint && SaveFootprintAs( footprint ) )
            {
                m_footprintNameWhenLoaded = footprint->GetFPID().GetLibItemName();
                m_toolManager->GetView()->Update( GetBoard()->m_Modules );
                GetScreen()->ClrModify();

                if( GetGalCanvas() )
                    GetGalCanvas()->ForceRefresh();
                else
                    GetCanvas()->Refresh();

                SyncLibraryTree( true );
            }
        }
        else
        {
            // Save Selected Footprint As
            MODULE* footprint = LoadFootprint( getTargetFPID() );

            if( footprint && SaveFootprintAs( footprint ) )
                SyncLibraryTree( true );
        }

        m_treePane->GetLibTree()->Refresh();
        break;

    case ID_MODEDIT_REVERT_PART:
        RevertFootprint();
        break;

    case ID_MODEDIT_CUT_PART:
    case ID_MODEDIT_COPY_PART:
        if( getTargetFPID().IsValid() )
        {
            LIB_ID fpID = getTargetFPID();

            if( fpID == GetLoadedFPID() )
                m_copiedModule.reset( new MODULE( *GetBoard()->m_Modules.GetFirst() ) );
            else
                m_copiedModule.reset( LoadFootprint( fpID ) );

            if( id == ID_MODEDIT_CUT_PART )
            {
                if( fpID == GetLoadedFPID() )
                    Clear_Pcb( false );

                DeleteModuleFromLibrary( fpID, false );
            }

            SyncLibraryTree( true );
        }
        break;

    case ID_MODEDIT_PASTE_PART:
        if( m_copiedModule && !getTargetFPID().GetLibNickname().empty() )
        {
            wxString newLib = getTargetFPID().GetLibNickname();
            MODULE*  newModule( m_copiedModule.get() );
            wxString newName = newModule->GetFPID().GetLibItemName();

            while( Prj().PcbFootprintLibs()->FootprintExists( newLib, newName ) )
                newName += _( "_copy" );

            newModule->SetFPID( LIB_ID( newLib, newName ) );
            saveFootprintInLibrary( newModule, newLib );

            SyncLibraryTree( true );
            m_treePane->GetLibTree()->SelectLibId( newModule->GetFPID() );
        }
        break;

    case ID_ADD_FOOTPRINT_TO_BOARD:
        SaveFootprintToBoard( true );
        break;

    case ID_MODEDIT_IMPORT_PART:
        if( ! Clear_Pcb( true ) )
            break;                  // this command is aborted

        SetCrossHairPosition( wxPoint( 0, 0 ) );
        Import_Module();

        if( GetBoard()->m_Modules )
            GetBoard()->m_Modules->ClearFlags();

        GetScreen()->SetModify();
        Zoom_Automatique( false );
        m_canvas->Refresh();
        Update3DView();

        break;

    case ID_MODEDIT_EXPORT_PART:
        if( getTargetFPID() == GetLoadedFPID() )
            Export_Module( GetBoard()->m_Modules );
        else
            Export_Module( LoadFootprint( getTargetFPID() ) );
        break;

    case ID_MODEDIT_CREATE_NEW_LIB:
    {
        wxFileName fn( CreateNewLibrary() );
        wxString name = fn.GetName();

        if( !name.IsEmpty() )
        {
            LIB_ID newLib( name, wxEmptyString );

            SyncLibraryTree( true );
            m_treePane->GetLibTree()->SelectLibId( newLib );
        }
    }
        break;

    case ID_MODEDIT_ADD_LIBRARY:
        AddLibrary();
        break;

    case ID_MODEDIT_EDIT_MODULE:
        LoadModuleFromLibrary( m_treePane->GetLibTree()->GetSelectedLibId() );
        break;

    case ID_MODEDIT_PAD_SETTINGS:
        InstallPadOptionsFrame( NULL );
        break;

    case ID_MODEDIT_CHECK:
        // Currently: not implemented
        break;

    case ID_MODEDIT_EDIT_MODULE_PROPERTIES:
        if( GetBoard()->m_Modules )
        {
            editFootprintProperties( GetBoard()->m_Modules );
            m_canvas->Refresh();
        }
        break;

    break;

    case ID_GEN_IMPORT_GRAPHICS_FILE:
        if( GetBoard()->m_Modules )
        {
            InvokeDialogImportGfxModule( this, GetBoard()->m_Modules );
            m_canvas->Refresh();
        }
        break;

    default:
        wxLogDebug( wxT( "FOOTPRINT_EDIT_FRAME::Process_Special_Functions error" ) );
        break;
    }
}


void FOOTPRINT_EDIT_FRAME::editFootprintProperties( MODULE* aModule )
{
    LIB_ID oldFPID = aModule->GetFPID();

    DIALOG_FOOTPRINT_FP_EDITOR dialog( this, aModule );
    dialog.ShowModal();

    updateTitle();      // in case of a name change...
}


void FOOTPRINT_EDIT_FRAME::OnVerticalToolbar( wxCommandEvent& aEvent )
{
    int id = aEvent.GetId();
    int lastToolID = GetToolId();

    // Stop the current command and deselect the current tool.
    SetNoToolSelected();

    switch( id )
    {
    case ID_NO_TOOL_SELECTED:
        break;

    case ID_ZOOM_SELECTION:
        // This tool is located on the main toolbar: switch it on or off on click on it
        if( lastToolID != ID_ZOOM_SELECTION )
            SetToolID( ID_ZOOM_SELECTION, wxCURSOR_MAGNIFIER, _( "Zoom to selection" ) );
        else
            SetNoToolSelected();
        break;

    case ID_MODEDIT_LINE_TOOL:
        SetToolID( id, wxCURSOR_PENCIL, _( "Add line" ) );
        break;

    case ID_MODEDIT_ARC_TOOL:
        SetToolID( id, wxCURSOR_PENCIL, _( "Add arc" ) );
        break;

    case ID_MODEDIT_CIRCLE_TOOL:
        SetToolID( id, wxCURSOR_PENCIL, _( "Add circle" ) );
        break;

    case ID_MODEDIT_TEXT_TOOL:
        SetToolID( id, wxCURSOR_PENCIL, _( "Add text" ) );
        break;

    case ID_MODEDIT_ANCHOR_TOOL:
        SetToolID( id, wxCURSOR_PENCIL, _( "Place anchor" ) );
        break;

    case ID_MODEDIT_PLACE_GRID_COORD:
        SetToolID( id, wxCURSOR_PENCIL, _( "Set grid origin" ) );
        break;

    case ID_MODEDIT_PAD_TOOL:
        if( GetBoard()->m_Modules )
        {
            SetToolID( id, wxCURSOR_PENCIL, _( "Add pad" ) );
        }
        else
        {
            SetToolID( id, wxCURSOR_ARROW, _( "Pad properties" ) );
            InstallPadOptionsFrame( NULL );
            SetNoToolSelected();
        }
        break;

    case ID_MODEDIT_DELETE_TOOL:
        SetToolID( id, wxCURSOR_BULLSEYE, _( "Delete item" ) );
        break;

    case ID_MODEDIT_MEASUREMENT_TOOL:
        DisplayError( this, wxT( "Measurement Tool not available in Legacy Toolset" ) );
        SetNoToolSelected();
        break;

    default:
        wxFAIL_MSG( wxT( "Unknown command id." ) );
        SetNoToolSelected();
    }
}


void FOOTPRINT_EDIT_FRAME::OnEditItemRequest( BOARD_ITEM* aItem )
{
    switch( aItem->Type() )
    {
    case PCB_PAD_T:
        InstallPadOptionsFrame( static_cast<D_PAD*>( aItem ) );
        m_canvas->MoveCursorToCrossHair();
        break;

    case PCB_MODULE_T:
        editFootprintProperties( (MODULE*) aItem );
        m_canvas->MoveCursorToCrossHair();
        m_canvas->Refresh();
        break;

    case PCB_MODULE_TEXT_T:
        InstallTextOptionsFrame( aItem );
        break;

    case PCB_MODULE_EDGE_T :
        InstallGraphicItemPropertiesDialog( aItem );
        break;

    default:
        break;
    }
}


COLOR4D FOOTPRINT_EDIT_FRAME::GetGridColor()
{
    return Settings().Colors().GetItemColor( LAYER_GRID );
}


void FOOTPRINT_EDIT_FRAME::SetActiveLayer( PCB_LAYER_ID aLayer )
{
    PCB_BASE_FRAME::SetActiveLayer( aLayer );

    m_Layers->SelectLayer( GetActiveLayer() );
    m_Layers->OnLayerSelected();

    GetGalCanvas()->SetHighContrastLayer( aLayer );
    GetGalCanvas()->Refresh();
}

bool FOOTPRINT_EDIT_FRAME::OpenProjectFiles( const std::vector<wxString>& aFileSet, int aCtl )
{
    if( ! Clear_Pcb( true ) )
        return false;                  // //this command is aborted

    SetCrossHairPosition( wxPoint( 0, 0 ) );
    Import_Module( aFileSet[0] );

    if( GetBoard()->m_Modules )
        GetBoard()->m_Modules->ClearFlags();

    GetScreen()->ClrModify();
    Zoom_Automatique( false );
    m_canvas->Refresh();

    return true;
}


void FOOTPRINT_EDIT_FRAME::KiwayMailIn( KIWAY_EXPRESS& mail )
{
    const std::string& payload = mail.GetPayload();

    switch( mail.Command() )
    {
    case MAIL_FP_EDIT:
        if( !payload.empty() )
        {
            wxFileName fpFileName( payload );
            wxString libNickname;
            wxString msg;

            FP_LIB_TABLE*        libTable = Prj().PcbFootprintLibs();
            const LIB_TABLE_ROW* libTableRow = libTable->FindRowByURI( fpFileName.GetPath() );

            if( !libTableRow )
            {
                msg.Printf( _( "The current configuration does not include the footprint library\n"
                               "\"%s\".\nUse Manage Footprint Libraries to edit the configuration." ),
                            fpFileName.GetPath() );
                DisplayErrorMessage( this, _( "Library not found in footprint library table." ), msg );
                break;
            }

            libNickname = libTableRow->GetNickName();

            if( !libTable->HasLibrary( libNickname, true ) )
            {
                msg.Printf( _( "The library with the nickname \"%s\" is not enabled\n"
                               "in the current configuration.  Use Manage Footprint Libraries to\n"
                               "edit the configuration." ), libNickname );
                DisplayErrorMessage( this, _( "Footprint library not enabled." ), msg );
                break;
            }

            LIB_ID  fpId( libNickname, fpFileName.GetName() );

            if( m_treePane )
            {
                m_treePane->GetLibTree()->SelectLibId( fpId );
                wxCommandEvent event( COMPONENT_SELECTED );
                wxPostEvent( m_treePane, event );
            }
        }

        break;

    default:
        ;
    }
}
