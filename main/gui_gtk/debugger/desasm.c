/*
 * debugger/desasm.c
 * 
 * Debugger for Mupen64 - davFr
 * Copyright (C) 2002 davFr - robind@esiee.fr
 *
 * Mupen64 is copyrighted (C) 2002 Hacktarux
 * Mupen64 homepage: http://mupen64.emulation64.com
 *         email address: hacktarux@yahoo.fr
 * 
 * This program is free software; you can redistribute it and/
 * or modify it under the terms of the GNU General Public Li-
 * cence as published by the Free Software Foundation; either
 * version 2 of the Licence.
 *
 * This program is distributed in the hope that it will be use-
 * ful, but WITHOUT ANY WARRANTY; without even the implied war-
 * ranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public Licence for more details.
 *
 * You should have received a copy of the GNU General Public
 * Licence along with this program; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139,
 * USA.
 *
**/

#include <stdio.h>
#include "desasm.h"
#include "ui_disasm_list.h"

//TODO: Lots and lots
// to differanciate between update (need reload) and scroll (doesn't need reload)
// to reorganise whole code.
 
static uint16 max_row=30;   //i plan to update this value on widget resizing.
static uint32 previous_focus;

static GtkWidget *clDesasm, *buRun;
static DisasmList *cmDesasm;

static GdkColor color_normal, color_BP, color_PC, color_PC_on_BP;

// Callback functions
static void on_click( GtkWidget *widget, GdkEventButton *event );
static void on_scroll(GtkAdjustment *adjustment, gpointer user_data);
static void on_step();
static void on_run();
static void on_goto();

static void on_close();



//]}=-=-=-=-=-=-=-=-=-=-=[ Initialisation Desassembleur ]=-=-=-=-=-=-=-=-=-=-={[

void init_desasm()
{
    char title[64];
    GtkWidget *boxH1,
            *scrollbar1,
            *boxV1,
                *buStep,
                *buGoTo;
    GtkObject*  adj;

    desasm_opened = 1;
    
    winDesasm = gtk_window_new( GTK_WINDOW_TOPLEVEL );
    sprintf( title, "%s - %s", "Desassembler", DEBUGGER_VERSION );
    gtk_window_set_title( GTK_WINDOW(winDesasm), title );
    gtk_window_set_default_size( GTK_WINDOW(winDesasm), 380, 500);
    gtk_container_set_border_width( GTK_CONTAINER(winDesasm), 2);

    boxH1 = gtk_hbox_new( FALSE, 0 );
    gtk_container_add( GTK_CONTAINER(winDesasm), boxH1 );

    //=== Creation of the Disassembled Code Display ===/
    cmDesasm = disasm_list_new();

    clDesasm = gtk_tree_view_new_with_model(GTK_TREE_MODEL(cmDesasm));
    g_object_unref(cmDesasm);

    GtkCellRenderer    *renderer;
    GtkTreeViewColumn  *col;
    renderer = gtk_cell_renderer_text_new();
    col = gtk_tree_view_column_new_with_attributes("Address", renderer, "text", 0, NULL);
    gtk_tree_view_append_column( GTK_TREE_VIEW( clDesasm ), col);
    col = gtk_tree_view_column_new_with_attributes("Opcode", renderer, "text", 1, NULL);
    gtk_tree_view_append_column( GTK_TREE_VIEW( clDesasm ), col);
    col = gtk_tree_view_column_new_with_attributes("Args", renderer, "text", 2, NULL);
    gtk_tree_view_append_column( GTK_TREE_VIEW( clDesasm ), col);

    adj = gtk_adjustment_new(0, -500, 500, 1, max_row, max_row);
    gtk_tree_view_set_vadjustment( GTK_TREE_VIEW( clDesasm ), GTK_ADJUSTMENT(adj));

    gtk_box_pack_start( GTK_BOX(boxH1), clDesasm, TRUE, TRUE, 0 );

    
    // (doubles) value, lower, upper, step_increment, page_increment, page_size.

        scrollbar1 = gtk_vscrollbar_new( GTK_ADJUSTMENT(adj) );
    gtk_box_pack_start( GTK_BOX(boxH1), scrollbar1, FALSE, FALSE, 0 );
    
    //=== Creation of the Buttons =====================/
    boxV1 = gtk_vbox_new( FALSE, 2 );
    gtk_box_pack_end( GTK_BOX(boxH1), boxV1, FALSE, FALSE, 0 );
    
    buRun = gtk_button_new_with_label( "Run" );
    gtk_box_pack_start( GTK_BOX(boxV1), buRun, FALSE, FALSE, 5 );
    buStep = gtk_button_new_with_label( "Next" );
    gtk_box_pack_start( GTK_BOX(boxV1), buStep, FALSE, FALSE, 0 );
    buGoTo = gtk_button_new_with_label( "Go To..." );
    gtk_box_pack_start( GTK_BOX(boxV1), buGoTo, FALSE, FALSE, 20 );

    gtk_widget_show_all( winDesasm );

    //=== Signal Connection ===========================/
    //gtk_signal_connect( GTK_OBJECT(clDesasm), "button_press_event",
    //                GTK_SIGNAL_FUNC(on_click), NULL );
    //gtk_signal_connect( GTK_OBJECT(adj), "value-changed",
    //                GTK_SIGNAL_FUNC(on_scroll), NULL );
    gtk_signal_connect( GTK_OBJECT(buRun), "clicked", on_run, NULL );
    gtk_signal_connect( GTK_OBJECT(buStep), "clicked", on_step, NULL );
    gtk_signal_connect( GTK_OBJECT(buGoTo), "clicked", on_goto, NULL );
    gtk_signal_connect( GTK_OBJECT(winDesasm), "destroy", on_close, NULL );

    //=== Colors Initialisation =======================/
//je l'aurais bien fait en global, mais ca entrainait des erreurs. Explication?
    color_normal.red = 0xFFFF;
    color_normal.green = 0xFFFF;
    color_normal.blue = 0xFFFF;

    color_BP.red=0xFFFF;
    color_BP.green=0xFA00;
    color_BP.blue=0x0;

    color_PC.red=0x0;
    color_PC.green=0xA000;
    color_PC.blue=0xFFFF;

    color_PC_on_BP.red=0xFFFF;
    color_PC_on_BP.green=0x0;
    color_PC_on_BP.blue=0x0;

    previous_focus = 0x00000000;
}




//]=-=-=-=-=-=-=-=-=-=-=[ Mise-a-jour Desassembleur ]=-=-=-=-=-=-=-=-=-=-=[

int add_instr( uint32 address )
// Add an disassembled instruction to the display
{
    uint32  instr;
    int new_row;    //index of the append row.
    char    **line;
    int status;
    line = malloc(3*sizeof(char*)); // desassembled instruction:
    line[0] = malloc(32*sizeof(char)); // - address
    line[1] = malloc(32*sizeof(char)); // - OP-code
    line[2] = malloc(32*sizeof(char)); // - parameters

    sprintf( line[0], "%lX", address);
    status = get_instruction( address, &instr);
    if( status == 1) {
        strcpy( line[1], "X+X+X+X");
        strcpy( line[2], "UNREADABLE");
    }
    else if( status == 2 ) {
        strcpy( line[1], "???????");
        strcpy( line[2], "Behind TLB");
    }
    else {
        r4300_decode_op( instr, line[1], line[2] );
    }
    //new_row=gtk_clist_append( GTK_CLIST(clDesasm), line );
    //gtk_clist_set_selectable( GTK_CLIST(clDesasm), new_row, FALSE );
    //gtk_clist_set_row_data( GTK_CLIST(clDesasm), new_row, (gpointer) address );

    return new_row;
}


void reload_instr( uint32 address, int row )
//No more used for the moment.
{
    uint32 instr;
    char opcode[32];
    char args[32];

    int status = get_instruction( address, &instr);
    if( status == 1 ) {
        strcpy( opcode, "X+X+X+X");
        strcpy( args, "UNREADABLE");
    }
    else if( status == 2 ) {
        strcpy( opcode, "???????");
        strcpy( args, "Behind TLB");
    }
    else {
        r4300_decode_op( instr, opcode, args );
    }
    //gtk_clist_set_text( GTK_CLIST(clDesasm), row, 1, opcode);
    //gtk_clist_set_text( GTK_CLIST(clDesasm), row, 2, args);
}


void update_desasm( uint32 focused_address )
//Display disassembled instructions around a 'focused_address'
//  (8 instructions before 'focused_address', the rest after).
{
    int i, row;
    uint32 address;

    //gtk_clist_freeze( GTK_CLIST(clDesasm) );

    //=== Disassembly cleaning ========================/
    //for (i=0; i<max_row; i++) {
    //    gtk_clist_remove( GTK_CLIST(clDesasm), 0);
    //}
    
    //=== Disassembly filling =========================/
    //Display starts 8 instructions
    address = focused_address - 0x020;  //8 instructions before.
    for (i=0; i<max_row; i++) {
        address = address + 0x4;
        row = add_instr( address );
    //    if( check_breakpoints(address) != -1 )
    //        gtk_clist_set_background( GTK_CLIST(clDesasm), row, &color_BP);
    }

    //=== Update Color of new PC Row =================/
    //row = gtk_clist_find_row_from_data( GTK_CLIST(clDesasm), (gpointer) PC->addr);
    //if( check_breakpoints(PC->addr) == -1 )
    //    gtk_clist_set_background( GTK_CLIST(clDesasm), row, &color_PC);
    //else
    //    gtk_clist_set_background( GTK_CLIST(clDesasm), row, &color_PC_on_BP);

    //gtk_clist_thaw( GTK_CLIST(clDesasm) );
    previous_focus = focused_address;
}


void update_desasm_color( uint32 address )
{
    int row;    // row whose color has to be updated.
    GdkColor *new_color;

    //row = gtk_clist_find_row_from_data( GTK_CLIST(clDesasm), (gpointer) address );
    /*if( row != -1) {
        if( check_breakpoints(address) == -1 ) {
            if( address != PC->addr )
                new_color = &color_normal;
            else
                new_color = &color_PC;
        }
        else {
            if( address != PC->addr )
                new_color = &color_BP;
            else
                new_color = &color_PC_on_BP;
        }
    }*/
    //gtk_clist_set_background( GTK_CLIST(clDesasm), row, new_color);
}


void switch_button_to_run()
{ //Is called from debugger.c, when a breakpoint is reached.
    gtk_label_set_text( GTK_LABEL (GTK_BIN (buRun)->child), "Run");
}


//]=-=-=-=-=-=-=[ Les Fonctions de Retour des Signaux (CallBack) ]=-=-=-=-=-=-=[

static void on_run()
{
    if(run == 2) {
        run = 0;
        gtk_label_set_text( GTK_LABEL (GTK_BIN (buRun)->child), "Run");
    } else {
        run = 2;
        gtk_label_set_text( GTK_LABEL (GTK_BIN (buRun)->child),"Pause");
        pthread_cond_signal(&debugger_done_cond);
    }
}


static void on_step()
{
    if(run == 2) {
        gtk_label_set_text( GTK_LABEL (GTK_BIN (buRun)->child), "Run");
    } else {
        pthread_cond_signal(&debugger_done_cond);
    }
    run = 0;
}


static void on_goto()
{//TODO: to open a dialog, get & check the entry, and go there.
    update_desasm( 0x0A4000040 );
}


static void on_scroll(GtkAdjustment *adjustment, gpointer user_data)
{
    sint32 delta= 4 *((sint32) adjustment->value);
    if(delta==0) return;

    gtk_adjustment_set_value(adjustment, 0);
    update_desasm( (uint32) previous_focus+delta );
}


static void on_click( GtkWidget *clist, GdkEventButton *event )
//Add a breakpoint on double-clicked linelines.
{
    //FIXME: minor corner case - Bad color when clicked row is PC.
    int clicked_row;
    uint32 clicked_address;
    int break_number;
    
    if(event->type==GDK_2BUTTON_PRESS) {
        gtk_clist_get_selection_info( GTK_CLIST(clist), event->x, event->y, &clicked_row, NULL);
        clicked_address =(uint32) gtk_clist_get_row_data( GTK_CLIST(clist), clicked_row);
        printf( "[DASM] click on row: %d\t address: 0x%lX\n", clicked_row, clicked_address );

	break_number = lookup_breakpoint(clicked_address, BPT_FLAG_EXEC);
        if( break_number==-1 ) {
            add_breakpoint( clicked_address );
            gtk_clist_set_background(  GTK_CLIST(clist), clicked_row, &color_BP);
        }
        else if(BPT_CHECK_FLAG(g_Breakpoints[break_number], BPT_FLAG_ENABLED))
        {
            disable_breakpoint( break_number );
            gtk_clist_set_background(  GTK_CLIST(clist), clicked_row, &color_normal);
        }
        else
        {
            enable_breakpoint( break_number );
            gtk_clist_set_background(  GTK_CLIST(clist), clicked_row, &color_BP);
        }
    update_breakpoints();
    }
}


static void on_close()
{
    desasm_opened = 0;

//  What should be happening then? Currently, thread is killed.
    run = 0;            // 0(stop) ou 2(run)
}
