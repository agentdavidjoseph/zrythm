/*
 * gui/widgets/arranger.c - The timeline containing regions
 *
 * Copyright (C) 2018 Alexandros Theodotou
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "zrythm.h"
#include "project.h"
#include "settings.h"
#include "gui/widgets/region.h"
#include "audio/automation_track.h"
#include "audio/channel.h"
#include "audio/instrument_track.h"
#include "audio/midi_region.h"
#include "audio/mixer.h"
#include "audio/track.h"
#include "audio/transport.h"
#include "gui/widgets/arranger.h"
#include "gui/widgets/automation_curve.h"
#include "gui/widgets/automation_point.h"
#include "gui/widgets/automation_track.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/color_area.h"
#include "gui/widgets/inspector.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/midi_arranger.h"
#include "gui/widgets/midi_arranger_bg.h"
#include "gui/widgets/midi_modifier_arranger.h"
#include "gui/widgets/midi_modifier_arranger_bg.h"
#include "gui/widgets/midi_note.h"
#include "gui/widgets/midi_ruler.h"
#include "gui/widgets/piano_roll.h"
#include "gui/widgets/piano_roll_labels.h"
#include "gui/widgets/region.h"
#include "gui/widgets/ruler.h"
#include "gui/widgets/timeline_arranger.h"
#include "gui/widgets/timeline_bg.h"
#include "gui/widgets/timeline_ruler.h"
#include "gui/widgets/track.h"
#include "gui/widgets/tracklist.h"
#include "utils/arrays.h"
#include "utils/ui.h"

#include <gtk/gtk.h>

G_DEFINE_TYPE_WITH_PRIVATE (ArrangerWidget,
                            arranger_widget,
                            GTK_TYPE_OVERLAY)

#define T_MIDI prv->type == ARRANGER_TYPE_MIDI
#define T_TIMELINE prv->type == ARRANGER_TYPE_TIMELINE
#define T_MIDI_MODIFIER prv->type == \
  ARRANGER_TYPE_MIDI_MODIFIER
#define GET_PRIVATE ARRANGER_WIDGET_GET_PRIVATE (self)

ArrangerWidgetPrivate *
arranger_widget_get_private (ArrangerWidget * self)
{
  return arranger_widget_get_instance_private (self);
}

/**
 * Gets x position in pixels.
 *
 */
int
arranger_widget_pos_to_px (ArrangerWidget * self,
                          Position * pos)
{
  GET_PRIVATE;
  RulerWidget * ruler = T_MIDI ?
    Z_RULER_WIDGET (MIDI_RULER) :
    Z_RULER_WIDGET (MW_RULER);
  int px = ruler_widget_pos_to_px (ruler,
                                   pos);
  return px + SPACE_BEFORE_START;
}

void
arranger_widget_px_to_pos (ArrangerWidget * self,
                           Position * pos,
                           int              px)
{
  /* clamp at 0 */
  if (px < SPACE_BEFORE_START)
    px = SPACE_BEFORE_START;
  GET_PRIVATE;
  RulerWidget * ruler = T_MIDI ?
    Z_RULER_WIDGET (MIDI_RULER) :
    Z_RULER_WIDGET (MW_RULER);
  ruler_widget_px_to_pos (ruler,
                          pos,
                          px - SPACE_BEFORE_START);
}

/**
 * Gets called to set the position/size of each overlayed widget.
 */
static gboolean
get_child_position (GtkOverlay   *overlay,
                    GtkWidget    *widget,
                    GdkRectangle *allocation,
                    gpointer      user_data)
{
  ArrangerWidget * self = (ArrangerWidget *) overlay;
  GET_PRIVATE;

  if (T_MIDI)
    {
      midi_arranger_widget_set_allocation (
        Z_MIDI_ARRANGER_WIDGET (self),
        widget,
        allocation);
    }
  else if (T_TIMELINE)
    {
      timeline_arranger_widget_set_allocation (
        Z_TIMELINE_ARRANGER_WIDGET (self),
        widget,
        allocation);
    }
  else if (T_MIDI_MODIFIER)
    {
      midi_modifier_arranger_widget_set_allocation (
        Z_MIDI_MODIFIER_ARRANGER_WIDGET (self),
        widget,
        allocation);
    }
  return TRUE;
}

void
arranger_widget_get_hit_widgets_in_range (
  ArrangerWidget *  self,
  ArrangerChildType type,
  double            start_x,
  double            start_y,
  double            offset_x,
  double            offset_y,
  GtkWidget **      array, ///< array to fill
  int *             array_size) ///< array_size to fill
{
  GList *children, *iter;

  /* go through each overlay child */
  children = gtk_container_get_children (GTK_CONTAINER (self));
  for(iter = children; iter != NULL; iter = g_list_next (iter))
    {
      GtkWidget * widget = GTK_WIDGET (iter->data);

      GtkAllocation allocation;
      gtk_widget_get_allocation (widget,
                                 &allocation);

      gint wx, wy;
      gtk_widget_translate_coordinates(
                GTK_WIDGET (self),
                GTK_WIDGET (widget),
                start_x,
                start_y,
                &wx,
                &wy);

      int x_hit, y_hit;
      if (offset_x < 0)
        {
          x_hit = wx + offset_x <= allocation.width &&
            wx > 0;
        }
      else
        {
          x_hit = wx <= allocation.width &&
            wx + offset_x > 0;
        }
      if (!x_hit) continue;
      if (offset_y < 0)
        {
          y_hit = wy + offset_y <= allocation.height &&
            wy > 0;
        }
      else
        {
          y_hit = wy <= allocation.height &&
            wy + offset_y > 0;
        }
      if (!y_hit) continue;

      if (type == ARRANGER_CHILD_TYPE_MIDI_NOTE &&
          Z_IS_MIDI_NOTE_WIDGET (widget))
        {
          array[(*array_size)++] = widget;
        }
      else if (type == ARRANGER_CHILD_TYPE_REGION &&
               Z_IS_REGION_WIDGET (widget))
        {
          array[(*array_size)++] = widget;
        }
      else if (type == ARRANGER_CHILD_TYPE_AP &&
               Z_IS_AUTOMATION_POINT_WIDGET (widget))
        {
          array[(*array_size)++] = widget;
        }
    }
}

GtkWidget *
arranger_widget_get_hit_widget (ArrangerWidget *  self,
                ArrangerChildType type,
                double            x,
                double            y)
{
  GList *children, *iter;

  /* go through each overlay child */
  children = gtk_container_get_children (GTK_CONTAINER (self));
  for(iter = children; iter != NULL; iter = g_list_next (iter))
    {
      GtkWidget * widget = GTK_WIDGET (iter->data);

      GtkAllocation allocation;
      gtk_widget_get_allocation (widget,
                                 &allocation);

      gint wx, wy;
      gtk_widget_translate_coordinates(
                GTK_WIDGET (self),
                GTK_WIDGET (widget),
                x,
                y,
                &wx,
                &wy);

      /* if hit */
      if (wx >= 0 &&
          wx <= allocation.width &&
          wy >= 0 &&
          wy <= allocation.height)
        {
          if (type == ARRANGER_CHILD_TYPE_MIDI_NOTE &&
              Z_IS_MIDI_NOTE_WIDGET (widget))
            {
              return widget;
            }
          else if (type == ARRANGER_CHILD_TYPE_REGION &&
                   Z_IS_REGION_WIDGET (widget))
            {
              return widget;
            }
          else if (type == ARRANGER_CHILD_TYPE_AP &&
                   Z_IS_AUTOMATION_POINT_WIDGET (widget))
            {
              return widget;
            }
          else if (type == ARRANGER_CHILD_TYPE_AC &&
                   Z_IS_AUTOMATION_CURVE_WIDGET (widget))
            {
              return widget;
            }
        }
    }
  return NULL;
}

static void
update_inspector (ArrangerWidget *self)
{
  GET_PRIVATE;
  if (T_MIDI)
    {
      midi_arranger_widget_update_inspector (
        Z_MIDI_ARRANGER_WIDGET (self));
    }
  else if (T_TIMELINE)
    {
      timeline_arranger_widget_update_inspector (
        Z_TIMELINE_ARRANGER_WIDGET (self));
    }
}

/**
 * Selects the region.
 */
void
arranger_widget_toggle_select (ArrangerWidget *  self,
               ArrangerChildType type,
               void *            child,
               int               append)
{
  GET_PRIVATE;

  void ** array;
  int * num;

  if (T_TIMELINE)
    {
      TimelineArrangerWidget * taw =
        Z_TIMELINE_ARRANGER_WIDGET (self);
      if (type == ARRANGER_CHILD_TYPE_REGION)
        {
          array = (void **) taw->regions;
          num = &taw->num_regions;
        }
      else if (type == ARRANGER_CHILD_TYPE_AP)
        {
          array = (void **) taw->automation_points;
          num = &taw->num_automation_points;
        }
    }
  if (T_MIDI)
    {
      MidiArrangerWidget * maw =
        Z_MIDI_ARRANGER_WIDGET (self);
      if (type == ARRANGER_CHILD_TYPE_MIDI_NOTE)
        {
          array = (void **) maw->midi_notes;
          num = &maw->num_midi_notes;
        }
    }

  if (!append)
    {
      /* deselect existing selections */
      for (int i = 0; i < (*num); i++)
        {
          void * r = array[i];
          if (type == ARRANGER_CHILD_TYPE_REGION)
            {
              region_widget_select (((Region *)r)->widget, 0);
            }
        }
      *num = 0;
    }

  /* if already selected */
  if (array_contains (array,
                      *num,
                      child))
    {
      /* deselect */
      array_delete (array,
                    num,
                    child);
      if (type == ARRANGER_CHILD_TYPE_REGION)
        {
          region_widget_select (((Region *)child)->widget, 0);
        }
    }
  else /* not selected */
    {
      /* select */
      array_append (array,
                    num,
                    child);
      if (type == ARRANGER_CHILD_TYPE_REGION)
        {
          region_widget_select (((Region *)child)->widget, 1);
        }
    }
}

void
arranger_widget_select_all (ArrangerWidget *  self,
                            int               select)
{
  GET_PRIVATE;

  if (T_MIDI)
    {
      midi_arranger_widget_select_all (
        Z_MIDI_ARRANGER_WIDGET (self),
        select);
    }
  else if (T_TIMELINE)
    {
      timeline_arranger_widget_select_all (
        Z_TIMELINE_ARRANGER_WIDGET (self),
        select);
    }
  update_inspector (self);
}


static void
show_context_menu (ArrangerWidget * self)
{
  GET_PRIVATE;

  if (T_MIDI)
    {
      midi_arranger_widget_show_context_menu (
        Z_MIDI_ARRANGER_WIDGET (self));
    }
  else if (T_TIMELINE)
    {
      timeline_arranger_widget_show_context_menu (
        Z_TIMELINE_ARRANGER_WIDGET (self));
    }
}

static void
on_right_click (GtkGestureMultiPress *gesture,
               gint                  n_press,
               gdouble               x,
               gdouble               y,
               gpointer              user_data)
{
  ArrangerWidget * self = (ArrangerWidget *) user_data;

  if (n_press == 1)
    {
      show_context_menu (self);
    }
}

static gboolean
on_key_action (GtkWidget *widget,
               GdkEventKey  *event,
               gpointer   user_data)
{
  ArrangerWidget * self = (ArrangerWidget *) user_data;

  if (event->state & GDK_CONTROL_MASK &&
      event->type == GDK_KEY_PRESS &&
      event->keyval == GDK_KEY_a)
    {
      arranger_widget_select_all (self, 1);
    }

  return FALSE;
}

/**
 * On button press.
 *
 * This merely sets the number of clicks. It is always called
 * before drag_begin, so the logic is done in drag_begin.
 */
static void
multipress_pressed (GtkGestureMultiPress *gesture,
               gint                  n_press,
               gdouble               x,
               gdouble               y,
               gpointer              user_data)
{
  ArrangerWidget * self = (ArrangerWidget *) user_data;
  GET_PRIVATE;

  prv->n_press = n_press;
}

static void
drag_begin (GtkGestureDrag * gesture,
               gdouble         start_x,
               gdouble         start_y,
               gpointer        user_data)
{
  ArrangerWidget * self = (ArrangerWidget *) user_data;
  GET_PRIVATE;

  prv->start_x = start_x;
  prv->start_y = start_y;

  if (!gtk_widget_has_focus (GTK_WIDGET (self)))
    gtk_widget_grab_focus (GTK_WIDGET (self));

  GdkEventSequence *sequence =
    gtk_gesture_single_get_current_sequence (GTK_GESTURE_SINGLE (gesture));
  /*guint button =*/
    /*gtk_gesture_single_get_current_button (GTK_GESTURE_SINGLE (gesture));*/
  const GdkEvent * event =
    gtk_gesture_get_last_event (GTK_GESTURE (gesture), sequence);
  GdkModifierType state_mask;
  gdk_event_get_state (event, &state_mask);

  MidiNoteWidget * midi_note_widget = T_MIDI ?
    midi_arranger_widget_get_hit_midi_note (
      Z_MIDI_ARRANGER_WIDGET (self), start_x, start_y) :
    NULL;
  RegionWidget * rw = T_TIMELINE ?
    timeline_arranger_widget_get_hit_region (
      Z_TIMELINE_ARRANGER_WIDGET (self), start_x, start_y) :
    NULL;
  AutomationCurveWidget * ac_widget = T_TIMELINE ?
    timeline_arranger_widget_get_hit_curve (
      Z_TIMELINE_ARRANGER_WIDGET (self), start_x, start_y) :
    NULL;
  AutomationPointWidget * ap_widget = T_TIMELINE ?
    timeline_arranger_widget_get_hit_ap (
      Z_TIMELINE_ARRANGER_WIDGET (self), start_x, start_y) :
    NULL;
  int is_hit = midi_note_widget || rw || ac_widget || ap_widget;
  if (is_hit)
    {
      /* set selections, positions, actions, cursor */
      if (midi_note_widget)
        {
          midi_arranger_widget_on_drag_begin_note_hit (
            Z_MIDI_ARRANGER_WIDGET (self),
            midi_note_widget);
        }
      else if (rw)
        {
          timeline_arranger_widget_on_drag_begin_region_hit (
            Z_TIMELINE_ARRANGER_WIDGET (self),
            state_mask,
            start_x,
            rw);
        }
      else if (ap_widget)
        {
          timeline_arranger_widget_on_drag_begin_ap_hit (
            Z_TIMELINE_ARRANGER_WIDGET (self),
            state_mask,
            start_x,
            ap_widget);
        }
      else if (ac_widget)
        {
          Z_TIMELINE_ARRANGER_WIDGET (self)->start_ac =
            ac_widget->ac;
        }

      /* find start pos */
      position_init (&prv->start_pos);
      position_set_bar (&prv->start_pos, 2000);
      if (T_TIMELINE)
        {
          timeline_arranger_widget_find_start_poses (
            Z_TIMELINE_ARRANGER_WIDGET (self));
        }
    }
  else /* nothing hit */
    {
      if (prv->n_press == 1)
        {
          /* area selection */
          prv->action = ARRANGER_ACTION_STARTING_SELECTION;

          /* deselect all */
          arranger_widget_select_all (self, 0);
        }
      else if (prv->n_press == 2)
        {
          Position pos;
          arranger_widget_px_to_pos (self,
                                     &pos,
                                     start_x);

          Track * track = NULL;
          AutomationTrack * at = NULL;
          int note;
          Region * region = NULL;
          if (T_TIMELINE)
            {
              at = timeline_arranger_widget_get_automation_track_at_y (start_y);
              if (!at)
                track = timeline_arranger_widget_get_track_at_y (start_y);
            }
          else if (T_MIDI)
            {
              note = (PIANO_ROLL->piano_roll_labels->total_px - start_y) /
                PIANO_ROLL->piano_roll_labels->px_per_note;

              /* if inside a region */
              MidiArrangerWidget * maw =
                Z_MIDI_ARRANGER_WIDGET (self);
              if (maw->channel)
                {
                  region = region_at_position (
                    maw->channel->track,
                    &pos);
                }
            }
          if (((T_TIMELINE && track) || (T_TIMELINE && at)) ||
              (T_MIDI && region))
            {
              if (T_TIMELINE && at)
                {
                  timeline_arranger_widget_create_ap (
                    Z_TIMELINE_ARRANGER_WIDGET (self),
                    at,
                    track,
                    &pos,
                    start_y);
                }
              /* double click inside a track */
              else if (T_TIMELINE && track)
                {
                  switch (track->type)
                    {
                    case TRACK_TYPE_INSTRUMENT:
                    case TRACK_TYPE_AUDIO:
                      timeline_arranger_widget_create_region (
                        Z_TIMELINE_ARRANGER_WIDGET (self),
                        track,
                        &pos);
                      break;
                    case TRACK_TYPE_MASTER:
                      break;
                    case TRACK_TYPE_CHORD:
                      timeline_arranger_widget_create_chord (
                        Z_TIMELINE_ARRANGER_WIDGET (self),
                        track,
                        &pos);
                    case TRACK_TYPE_BUS:
                      break;
                    }
                }
              else if (T_MIDI && region)
                {
                  midi_arranger_widget_on_drag_begin_create_note (
                    Z_MIDI_ARRANGER_WIDGET (self),
                    &pos,
                    note,
                    (MidiRegion *) region);
                }
              gtk_widget_queue_allocate (GTK_WIDGET (self));
            }
        }
    }

  /* update inspector */
  update_inspector (self);
}

static void
drag_update (GtkGestureDrag * gesture,
               gdouble         offset_x,
               gdouble         offset_y,
               gpointer        user_data)
{
  ArrangerWidget * self =
    Z_ARRANGER_WIDGET (user_data);
  GET_PRIVATE;

  /* set action to selecting if starting selection. this is because
   * drag_update never gets called if it's just a click, so we can check at
   * drag_end and see if anything was selected */
  if (prv->action == ARRANGER_ACTION_STARTING_SELECTION)
    {
      prv->action = ARRANGER_ACTION_SELECTING;
    }
  else if (prv->action == ARRANGER_ACTION_STARTING_MOVING)
    {
      prv->action = ARRANGER_ACTION_MOVING;
    }

  /* if drawing a selection */
  if (prv->action == ARRANGER_ACTION_SELECTING)
    {
      /* deselect all */
      arranger_widget_select_all (self, 0);

      if (T_TIMELINE)
        {
          timeline_arranger_widget_find_and_select_items (
            Z_TIMELINE_ARRANGER_WIDGET (self),
            offset_x,
            offset_y);
        }
      else if (T_MIDI)
        {
          midi_arranger_widget_find_and_select_midi_notes (
            Z_MIDI_ARRANGER_WIDGET (self),
            offset_x,
            offset_y);
        }
    } /* endif ARRANGER_ACTION_SELECTING */

  /* handle x */
  else if (prv->action == ARRANGER_ACTION_RESIZING_L)
    {
      Position pos;
      arranger_widget_px_to_pos (self,
                                 &pos,
                                 prv->start_x + offset_x);
      if (T_TIMELINE)
        {
          timeline_arranger_widget_snap_regions_l (
            Z_TIMELINE_ARRANGER_WIDGET (self),
            &pos);
        }
      else if (T_MIDI)
        {
          midi_arranger_widget_snap_midi_notes_l (
            Z_MIDI_ARRANGER_WIDGET (self),
            &pos);
        }
    } /* endif RESIZING_L */
  else if (prv->action == ARRANGER_ACTION_RESIZING_R)
    {
      Position pos;
      arranger_widget_px_to_pos (self,
                                 &pos,
                                 prv->start_x + offset_x);
      if (T_TIMELINE)
        {
          timeline_arranger_widget_snap_regions_r (
            Z_TIMELINE_ARRANGER_WIDGET (self),
            &pos);
        }
      else if (T_MIDI)
        {
          midi_arranger_widget_snap_midi_notes_r (
            Z_MIDI_ARRANGER_WIDGET (self),
            &pos);
        }
    } /*endif RESIZING_R */

  /* if moving the selection */
  else if (prv->action == ARRANGER_ACTION_MOVING)
    {
      Position diff_pos;
      RulerWidget * ruler =
        T_MIDI ?
        Z_RULER_WIDGET (MIDI_RULER) :
        Z_RULER_WIDGET (MW_RULER);
      /* note: using ruler here because SPACE_BEFORE_START
       * is irrelevant */
      ruler_widget_px_to_pos (ruler,
                              &diff_pos,
                              offset_x);
      int frames_diff = position_to_frames (&diff_pos);
      Position new_start_pos;
      position_set_to_pos (&new_start_pos, &prv->start_pos);
      position_add_frames (&new_start_pos, frames_diff);
      if (SNAP_GRID_ANY_SNAP(prv->snap_grid))
        position_snap (NULL,
                       &new_start_pos,
                       NULL,
                       NULL,
                       prv->snap_grid);
      frames_diff = position_to_frames (&new_start_pos) -
        position_to_frames (&prv->start_pos);

      if (T_TIMELINE)
        {
          timeline_arranger_widget_move_items_x (
            Z_TIMELINE_ARRANGER_WIDGET (self),
            frames_diff);
        }
      else if (T_MIDI)
        {
          /*midi_arranger_widget_move_midi_notes_x (*/
            /*MIDI_ARRANGER_WIDGET (self),*/
            /*&pos);*/
        }
      /*int diff = position_to_frames (&pos) - position_to_frames (&self->start_pos);*/
      /*position_set_to_pos (&pos, &self->end_pos);*/
      /*position_add_frames (&pos,*/
                           /*diff);*/

      /* handle y */
      if (T_TIMELINE)
        {
          timeline_arranger_widget_move_items_y (
            Z_TIMELINE_ARRANGER_WIDGET (self),
            offset_y);
        }
      else if (T_MIDI)
        {
          midi_arranger_widget_move_midi_notes_y (
            Z_MIDI_ARRANGER_WIDGET (self),
            offset_y);
        }
    } /* endif MOVING */

  gtk_widget_queue_allocate(GTK_WIDGET (self));
  if (T_MIDI)
    {
      gtk_widget_queue_draw (GTK_WIDGET (self));
      gtk_widget_show_all (GTK_WIDGET (self));
    }
  else if (T_TIMELINE)
    {
      gtk_widget_queue_allocate (GTK_WIDGET (MIDI_ARRANGER));
    }
  prv->last_offset_x = offset_x;
  prv->last_offset_y = offset_y;

  /* update inspector */
  update_inspector (self);
}

static void
drag_end (GtkGestureDrag *gesture,
               gdouble         offset_x,
               gdouble         offset_y,
               gpointer        user_data)
{
  ArrangerWidget * self = (ArrangerWidget *) user_data;
  GET_PRIVATE;

  prv->start_x = 0;
  prv->start_y = 0;
  prv->last_offset_x = 0;
  prv->last_offset_y = 0;
  if (T_MIDI)
    {
      midi_arranger_widget_on_drag_end (
        Z_MIDI_ARRANGER_WIDGET (self));
    }
  else if (T_TIMELINE)
    {
      timeline_arranger_widget_on_drag_end (
        Z_TIMELINE_ARRANGER_WIDGET (self));
    }

  /* if clicked on something, or clicked or nothing */
  if (prv->action == ARRANGER_ACTION_STARTING_SELECTION ||
      prv->action == ARRANGER_ACTION_STARTING_MOVING)
    {
      /* if clicked on something */
      if (prv->action == ARRANGER_ACTION_STARTING_MOVING)
        {
          /*set_state_and_redraw_tl_regions (self, RW_STATE_SELECTED);*/
          /*set_state_and_redraw_tl_automation_points (self,*/
            /*APW_STATE_SELECTED);*/
          /*set_state_and_redraw_me_midi_notes (self,*/
            /*MNW_STATE_SELECTED);*/
        }

      /* else if clicked on nothing */
      else if (prv->action == ARRANGER_ACTION_STARTING_SELECTION)
        {
          arranger_widget_select_all (self, 0);
        }
    }

  prv->action = ARRANGER_ACTION_NONE;
  gtk_widget_queue_draw (GTK_WIDGET (prv->bg));
}


void
arranger_widget_setup (ArrangerWidget *   self,
                       SnapGrid *         snap_grid,
                       ArrangerWidgetType type)
{
  GET_PRIVATE;

  prv->snap_grid = snap_grid;
  prv->type = type;
  if (T_MIDI)
    {
      prv->bg = Z_ARRANGER_BG_WIDGET (
        midi_arranger_bg_widget_new (
          Z_RULER_WIDGET (MIDI_RULER),
          Z_ARRANGER_WIDGET (MIDI_ARRANGER)));
    }
  else if (T_TIMELINE)
    {
      prv->bg = Z_ARRANGER_BG_WIDGET (
        timeline_bg_widget_new (
          Z_RULER_WIDGET (MW_RULER),
          Z_ARRANGER_WIDGET (MW_TIMELINE)));
    }
  else if (T_MIDI_MODIFIER)
    {
      prv->bg = Z_ARRANGER_BG_WIDGET (
        midi_modifier_arranger_bg_widget_new (
          Z_RULER_WIDGET (MIDI_RULER),
          Z_ARRANGER_WIDGET (MIDI_MODIFIER_ARRANGER)));
    }

  gtk_container_add (GTK_CONTAINER (self),
                     GTK_WIDGET (prv->bg));

  /* make it able to notify */
  gtk_widget_add_events (GTK_WIDGET (self), GDK_ALL_EVENTS_MASK);
  gtk_widget_set_can_focus (GTK_WIDGET (self),
                           1);
  gtk_widget_set_focus_on_click (GTK_WIDGET (self),
                                 1);

  g_signal_connect (G_OBJECT (self), "get-child-position",
                    G_CALLBACK (get_child_position), NULL);
  g_signal_connect (G_OBJECT(prv->drag), "drag-begin",
                    G_CALLBACK (drag_begin),  self);
  g_signal_connect (G_OBJECT(prv->drag), "drag-update",
                    G_CALLBACK (drag_update),  self);
  g_signal_connect (G_OBJECT(prv->drag), "drag-end",
                    G_CALLBACK (drag_end),  self);
  g_signal_connect (G_OBJECT (prv->multipress), "pressed",
                    G_CALLBACK (multipress_pressed), self);
  g_signal_connect (G_OBJECT (prv->right_mouse_mp), "pressed",
                    G_CALLBACK (on_right_click), self);
  g_signal_connect (G_OBJECT (self), "key-press-event",
                    G_CALLBACK (on_key_action), self);
  g_signal_connect (G_OBJECT (self), "key-release-event",
                    G_CALLBACK (on_key_action), self);

  if (T_TIMELINE)
    {
      timeline_arranger_widget_setup (
        Z_TIMELINE_ARRANGER_WIDGET (self));
    }
  else if (T_MIDI)
    {
      /*midi_arranger_widget_setup (*/
        /*MIDI_ARRANGER_WIDGET (self));*/
    }
}

/**
 * Draws the selection in its background.
 *
 * Should only be called by the bg widgets when drawing.
 */
void
arranger_bg_draw_selections (ArrangerWidget * self,
                             cairo_t *        cr)
{
  GET_PRIVATE;

  double offset_x, offset_y;
  offset_x = prv->start_x + prv->last_offset_x > 0 ?
    prv->last_offset_x :
    1 - prv->start_x;
  offset_y = prv->start_y + prv->last_offset_y > 0 ?
    prv->last_offset_y :
    1 - prv->start_y;
  if (prv->action == ARRANGER_ACTION_SELECTING)
    {
      cairo_set_source_rgba (cr, 0.9, 0.9, 0.9, 1.0);
      cairo_rectangle (cr,
                       prv->start_x,
                       prv->start_y,
                       offset_x,
                       offset_y);
      cairo_stroke (cr);
      cairo_set_source_rgba (cr, 0.3, 0.3, 0.3, 0.3);
      cairo_rectangle (cr,
                       prv->start_x,
                       prv->start_y,
                       offset_x,
                       offset_y);
      cairo_fill (cr);
    }
}

/**
 * Readd children.
 */
void
arranger_widget_refresh_children (
  ArrangerWidget * self)
{
  ARRANGER_WIDGET_GET_PRIVATE (self);

  if (T_MIDI)
    midi_arranger_widget_refresh_children (
      Z_MIDI_ARRANGER_WIDGET (self));
  else if (T_TIMELINE)
    timeline_arranger_widget_refresh_children (
      Z_TIMELINE_ARRANGER_WIDGET (self));
  else if (T_MIDI_MODIFIER)
    midi_modifier_arranger_widget_refresh_children (
      Z_MIDI_MODIFIER_ARRANGER_WIDGET (self));
}

static void
arranger_widget_class_init (ArrangerWidgetClass * _klass)
{
}

static void
arranger_widget_init (ArrangerWidget *self)
{
  GET_PRIVATE;

  prv->drag =
    GTK_GESTURE_DRAG (
      gtk_gesture_drag_new (GTK_WIDGET (self)));
  prv->multipress =
    GTK_GESTURE_MULTI_PRESS (
      gtk_gesture_multi_press_new (GTK_WIDGET (self)));
  prv->right_mouse_mp =
    GTK_GESTURE_MULTI_PRESS (
      gtk_gesture_multi_press_new (GTK_WIDGET (self)));
  gtk_gesture_single_set_button (
    GTK_GESTURE_SINGLE (prv->right_mouse_mp),
                        GDK_BUTTON_SECONDARY);
}
