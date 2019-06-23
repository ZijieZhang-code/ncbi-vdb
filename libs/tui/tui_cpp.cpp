/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

#include <iostream>
#include <string>

#include <klib/printf.h>

#include <tui/tui.hpp>

namespace tui {

    Tui * Tui::instance = NULL;

    Tui * Tui::getInstance( void )
    { 
        if ( !instance ) instance = new Tui();
        return instance;
    };

    void Tui::clean_up( void )
    {
        if ( instance ) delete instance;
        instance = NULL;
    }

    
    bool Tui_Event::get_from_tui( void )
    {
        Tui * t = Tui::getInstance();
        rc_t rc = KTUIGet ( t->tui_, &ev_ );
        return ( rc == 0 && !empty() );
    }


    Grid::Grid( void * data )
    {
        memset( &grid_data_, 0, sizeof grid_data_ );
        grid_data_.instance = this;
        grid_data_.cb_str = static_str_cb;
        grid_data_.cb_int = static_int_cb;
        grid_data_.data = data;
    };


    bool Dlg::Resize( Tui_Rect const &r )
    {
        bool res = Draw( true );
        if ( res ) res = SetRect( r, false );
        if ( res ) res = Draw( false );
        return res;
    }

    bool Dlg::SetCaptionF( const char * fmt, ... )
    {
        va_list args;
        va_start ( args, fmt );
        char buffer[ 1042 ];
        rc_t rc = string_vprintf ( buffer, sizeof buffer, NULL, fmt, args );
        if ( rc == 0 )
            return SetCaption( buffer );
        else
            return false;
    }

    bool Dlg::SetWidgetCaptionF( tui_id id, const char * fmt, ... )
    {
        va_list args;
        va_start ( args, fmt );
        char buffer[ 1042 ];
        rc_t rc = string_vprintf ( buffer, sizeof buffer, NULL, fmt, args );
        if ( rc == 0 )
            return SetWidgetCaption( id, buffer );
        else
            return false;
    }

    bool Dlg::SetWidgetTextF( tui_id id, const char * fmt, ... )
    {
        va_list args;
        va_start ( args, fmt );
        char buffer[ 1042 ];
        rc_t rc = string_vprintf ( buffer, sizeof buffer, NULL, fmt, args );
        if ( rc == 0 )
            return SetWidgetText( id, buffer );
        else
            return false;
    }

    bool Dlg::AddWidgetStringN( tui_id id, int n, ... )
    {
        bool res = true;
        va_list args;
        va_start ( args, n );
        for ( int i = 0; i < n && res; i++ )
            res = AddWidgetString( id, va_arg ( args, const char * ) );
        va_end ( args );
        return res;
    }


    bool Dlg::AddWidgetStringF( tui_id id, const char * fmt, ... )
    {
        va_list args;
        va_start ( args, fmt );
        char buffer[ 1042 ];
        size_t num_writ;
        rc_t rc = string_vprintf ( buffer, sizeof buffer, &num_writ, fmt, args );
        if ( rc == 0 )
            return AddWidgetString( id, buffer );
        else
            return false;
    }


    bool Dlg_Runner::handle_tui_event( Tui_Event &ev )
    {
        bool res = false;
        switch( ev.get_type() )
        {
            case ktui_event_kb : {
                                    KTUI_key key_type = ev.get_key_type();
                                    if ( key_type == ktui_alpha )
                                    {
                                        int code = ev.get_key();
                                        res = on_kb_alpha( dlg_, data_, code );
                                        if ( !res )
                                        {
                                           /* safety-net: ESC always closes the dialog */
                                           res = ( code == 27 );
                                           if ( res ) dlg_.SetDone( true );
                                        }
                                    }
                                    else
                                        res = on_kb_special_key( dlg_, data_, key_type );
                                  }
                                    break;

            case ktui_event_mouse : res = on_mouse( dlg_, data_,
                                                     ev.get_mouse_x(),
                                                     ev.get_mouse_y(),
                                                     ev.get_mouse_button() );
                                    break;

            case ktui_event_window : res = on_win( dlg_, data_,
                                                    ev.get_window_width(),
                                                    ev.get_window_height() );
                                      break;
        }
        return res;
    };


    bool Dlg_Runner::handle_dlg_event_loop( void )
    {
        bool done = false;
        bool res = false;
        while ( !done )
        {
            Tui_Dlg_Event dev;
            if ( dlg_.GetDlgEvent( &dev.ev_ ) )
            {
                switch( dev.get_type() )
                {
                    case ktuidlg_event_none         : done = true; break;
                    case ktuidlg_event_focus        : res = on_focus( dlg_, data_, dev ); break;
                    case ktuidlg_event_focus_lost   : res = on_focus_lost( dlg_, data_, dev ); break;
                    case ktuidlg_event_select       : res = on_select( dlg_, data_, dev ); break;
                    case ktuidlg_event_changed      : res = on_changed( dlg_, data_, dev ); break;
                    case ktuidlg_event_menu         : res = on_menu( dlg_, data_, dev ); break;
                }
            }
        }
        return res;
    }


    bool Dlg_Runner::handle_dlg_event( Tui_Event &ev )
    {
        bool res = false;
        if ( dlg_.HandleEvent( ev ) )
            res = handle_dlg_event_loop();
        return res;
    };


    void Dlg_Runner::run( void )
    {
        dlg_.Draw();
        while ( !dlg_.IsDone() )
        {
            Tui_Event ev;
            if ( ev.get_from_tui() )
            {
                if ( !handle_dlg_event( ev ) )
                    handle_tui_event( ev );
            }
            else
                handle_dlg_event_loop();
        }
    };

}