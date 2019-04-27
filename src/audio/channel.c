/*
 * Copyright (C) 2018-2019 Alexandros Theodotou <alex and zrythm dot org>
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

/**
 * \file
 *
 * A channel on the mixer.
 */

#include <math.h>
#include <unistd.h>
#include <stdlib.h>

#include "config.h"

#include "audio/audio_track.h"
#include "audio/automation_track.h"
#include "audio/automation_tracklist.h"
#include "audio/channel.h"
#include "audio/engine_jack.h"
#include "audio/instrument_track.h"
#include "audio/mixer.h"
#include "audio/pan.h"
#include "audio/track.h"
#include "audio/transport.h"
#include "plugins/lv2_plugin.h"
#include "gui/widgets/automation_tracklist.h"
#include "gui/widgets/bot_dock_edge.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/channel.h"
#include "gui/widgets/connections.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/track.h"
#include "gui/widgets/tracklist.h"
#include "project.h"
#include "utils/arrays.h"
#include "utils/math.h"
#include "utils/objects.h"

#include <gtk/gtk.h>

/**
 * Handles the recording logic inside the process cycle.
 */
void
channel_handle_recording (Channel * self)
{
  Region * region =
    track_get_region_at_pos (self->track,
                             &PLAYHEAD);
  /* get end position TODO snap */
  Position tmp;
  position_set_to_pos (&tmp, &PLAYHEAD);
  position_add_frames (
    &tmp,
    AUDIO_ENGINE->nframes + 1);

  if (self->type == CT_MIDI)
    {
      MidiRegion * mr = (MidiRegion *) region;

      if (region) /* if inside a region */
        {
          /* set region end pos */
          region_set_end_pos (region,
                              &tmp);
        }
      else /* if not already in a region */
        {
          /* create region */
          mr = midi_region_new (self->track,
                                &PLAYHEAD,
                                &tmp);
          region = (Region *) mr;
          track_add_region (self->track,
                            region);
        }

      /* convert MIDI data to midi notes */
#ifdef HAVE_JACK
      if (MIDI_IN_NUM_EVENTS > 0)
        {
          MidiNote * mn;
          for (int i = 0; i < MIDI_IN_NUM_EVENTS; i++)
            {
              jack_midi_event_t * event = &MIDI_IN_EVENT(i);
              jack_midi_event_get (event,
                                   AUDIO_ENGINE->port_buf,
                                   i);
              uint8_t type = event->buffer[0] & 0xf0;
              Velocity * vel;
              switch (type)
                {
                  case MIDI_CH1_NOTE_ON:
                    vel =
                      velocity_new (event->buffer[2]);
                    mn =
                      midi_note_new (
                        mr,
                        &PLAYHEAD,
                        &tmp,
                        event->buffer[1],
                        vel);
                    midi_region_add_midi_note (mr, mn);

                    /* add to unended notes */
                    array_append (mr->unended_notes,
                                  mr->num_unended_notes,
                                  mn);
                    break;
                  case MIDI_CH1_NOTE_OFF:
                    mn =
                      midi_region_find_unended_note (
                        mr,
                        event->buffer[1]);
                    midi_note_set_end_pos (mn,
                                           &tmp);
                    break;
                  case MIDI_CH1_CTRL_CHANGE:
                    /* TODO */
                    break;
                  default:
                          break;
                }
            } /* for loop num events */
        } /* if have midi events */
#endif
    } /* if channel type MIDI */
}

/**
 * Prepares the channel for processing.
 *
 * To be called before the main cycle each time on
 * all channels.
 */
void
channel_prepare_process (Channel * channel)
{
  Plugin * plugin;
  int i,j;

  /* clear buffers */
  if (channel->type == CT_MASTER ||
      channel->type == CT_AUDIO ||
      channel->type == CT_BUS)
    {
      port_clear_buffer (channel->stereo_in->l);
      port_clear_buffer (channel->stereo_in->r);
    }
  if (channel->type == CT_MIDI)
    {
      port_clear_buffer (channel->midi_in);
      port_clear_buffer (channel->piano_roll);
    }
  port_clear_buffer (channel->stereo_out->l);
  port_clear_buffer (channel->stereo_out->r);
  for (j = 0; j < STRIP_SIZE; j++)
    {
      plugin = channel->plugins[j];
      if (!plugin)
        continue;

      for (i = 0; i < plugin->num_in_ports;
           i++)
        {
          port_clear_buffer (plugin->in_ports[i]);
        }
      for (i = 0; i < plugin->num_out_ports;
           i++)
        {
          port_clear_buffer (plugin->out_ports[i]);
        }
      for (i = 0;
           i < plugin->num_unknown_ports; i++)
        {
          port_clear_buffer (
            plugin->unknown_ports[i]);
        }
    }
  channel->filled_stereo_in_bufs = 0;
}

void
channel_init_loaded (Channel * ch)
{
  g_message ("initing channel");
  /* plugins */
  for (int i = 0; i < STRIP_SIZE; i++)
    ch->plugins[i] =
      project_get_plugin (ch->plugin_ids[i]);

  /* fader */
  ch->fader.channel = ch;

  /* stereo in/out ports */
  ch->stereo_in->l =
    project_get_port (ch->stereo_in->l_id);
  ch->stereo_in->r =
    project_get_port (ch->stereo_in->r_id);
  ch->stereo_out->l =
    project_get_port (ch->stereo_out->l_id);
  ch->stereo_out->r =
    project_get_port (ch->stereo_out->r_id);

  /* midi in / piano roll ports */
  ch->midi_in =
    project_get_port (ch->midi_in_id);
  ch->piano_roll =
    project_get_port (ch->piano_roll_id);
  ch->piano_roll->flags = PORT_FLAG_PIANO_ROLL;
  ch->midi_in->midi_events =
    midi_events_new (1);
  ch->piano_roll->midi_events =
    midi_events_new (1);

  /* routing */
  if (ch->output_id > -1)
    ch->output =
      project_get_channel (ch->output_id);

  /* automatables */
  for (int i = 0; i < ch->num_automatables; i++)
    ch->automatables[i] =
      project_get_automatable (
        ch->automatable_ids[i]);

  /* track */
  ch->track =
    project_get_track (ch->track_id);

  /*zix_sem_init (&ch->processed_sem, 1);*/
  /*zix_sem_init (&ch->start_processing_sem, 0);*/

  ch->widget = channel_widget_new (ch);
}

/**
 * Creates, inits, and returns a new channel with given info.
 */
static Channel *
_create_channel (char * name)
{
  Channel * channel = calloc (1, sizeof (Channel));

  /* create ports */
  char * pll =
    g_strdup_printf ("%s stereo in L",
                     name);
  char * plr =
    g_strdup_printf ("%s stereo in R",
                     name);
  channel->stereo_in =
    stereo_ports_new (
      port_new_with_type (TYPE_AUDIO,
                          FLOW_INPUT,
                          pll),
      port_new_with_type (TYPE_AUDIO,
                          FLOW_INPUT,
                          plr));
  g_free (pll);
  g_free (plr);
  pll =
    g_strdup_printf ("%s MIDI in",
                     name);
  channel->midi_in =
    port_new_with_type (
      TYPE_EVENT,
      FLOW_INPUT,
      pll);
  channel->midi_in_id = channel->midi_in->id;
  channel->midi_in->midi_events =
    midi_events_new (1);
  g_free (pll);
  pll =
    g_strdup_printf ("%s Stereo out L",
                     name);
  plr =
    g_strdup_printf ("%s Stereo out R",
                     name);
  channel->stereo_out =
    stereo_ports_new (
     port_new_with_type (TYPE_AUDIO,
                         FLOW_OUTPUT,
                         pll),
     port_new_with_type (TYPE_AUDIO,
                         FLOW_OUTPUT,
                         plr));
  g_message ("Created stereo out ports");
  g_free (pll);
  g_free (plr);
  port_set_owner_channel (channel->stereo_in->l,
                          channel);
  port_set_owner_channel (channel->stereo_in->r,
                          channel);
  port_set_owner_channel (channel->stereo_out->l,
                          channel);
  port_set_owner_channel (channel->stereo_out->r,
                          channel);
  port_set_owner_channel (channel->midi_in,
                          channel);

  /* init plugins */
  for (int i = 0; i < STRIP_SIZE; i++)
    {
      channel->plugins[i] = NULL;
      channel->plugin_ids[i] = -1;
    }

  fader_init (&channel->fader, channel);

  /* connect MIDI in port from engine's jack port */
  if (AUDIO_ENGINE->midi_backend !=
        MIDI_BACKEND_DUMMY)
    {
      port_connect (AUDIO_ENGINE->midi_in,
                    channel->midi_in);
    }

  /* init semaphores */
  /*zix_sem_init (&channel->processed_sem, 1);*/
  /*zix_sem_init (&channel->start_processing_sem, 0);*/

  /* set up piano roll port */
  char * tmp =
    g_strdup_printf ("%s Piano Roll",
                     name);
  channel->piano_roll =
    port_new_with_type (
      TYPE_EVENT,
      FLOW_INPUT,
      tmp);
  channel->piano_roll->flags = PORT_FLAG_PIANO_ROLL;
  channel->piano_roll_id =
    channel->piano_roll->id;
  channel->piano_roll->is_piano_roll = 1;
  channel->piano_roll->owner_backend = 0;
  channel->piano_roll->owner_ch = channel;
  channel->piano_roll->midi_events =
    midi_events_new (1);

  channel->visible = 1;

  project_add_channel (channel);

  return channel;
}

/**
 * Sets fader to 0.0.
 */
void
channel_reset_fader (Channel * self)
{
  fader_set_amp (&self->fader, 1.0f);
}

/**
 * Generates automatables for the channel.
 *
 * Should be called as soon as it is created
 */
static void
generate_automatables (Channel * channel)
{
  g_message ("Generating automatables for channel %s",
             channel->track->name);

  /* generate channel automatables if necessary */
  if (!channel_get_automatable (
        channel,
        AUTOMATABLE_TYPE_CHANNEL_FADER))
    {
      array_append (
        channel->automatables,
        channel->num_automatables,
        automatable_create_fader (channel));
      channel->automatable_ids[
        channel->num_automatables - 1] =
          channel->automatables[
            channel->num_automatables - 1]->id;
    }
  if (!channel_get_automatable (
        channel,
        AUTOMATABLE_TYPE_CHANNEL_PAN))
    {
      array_append (channel->automatables,
                    channel->num_automatables,
                    automatable_create_pan (channel));
      channel->automatable_ids[
        channel->num_automatables - 1] =
          channel->automatables[
            channel->num_automatables - 1]->id;
    }
  if (!channel_get_automatable (
        channel,
        AUTOMATABLE_TYPE_CHANNEL_MUTE))
    {
      array_append (channel->automatables,
                    channel->num_automatables,
                    automatable_create_mute (channel));
      channel->automatable_ids[
        channel->num_automatables - 1] =
          channel->automatables[
            channel->num_automatables - 1]->id;
    }
}

/**
 * Used when loading projects.
 */
Channel *
channel_get_or_create_blank (int id)
{
  if (MIXER->channels[id])
    {
      return MIXER->channels[id];
    }

  Channel * channel = calloc (1, sizeof (Channel));

  channel->id = id;

  /* thread related */
  /*setup_thread (channel);*/

  MIXER->channels[id] = channel;
  MIXER->num_channels++;

  g_message ("[channel_new] Creating blank channel %d", id);

  return channel;
}

/**
 * Creates a channel of the given type with the given label
 */
Channel *
channel_create (ChannelType type,
                char *      label)
{
  g_warn_if_fail (label);

  Channel * channel = _create_channel (label);

  channel->type = type;

  /* set default output */
  if (type == CT_MASTER)
    {
      channel->output = NULL;
      channel->output_id = -1;
      channel->id = 0;
      port_connect (
        channel->stereo_out->l,
        AUDIO_ENGINE->stereo_out->l);
      port_connect (
        channel->stereo_out->r,
        AUDIO_ENGINE->stereo_out->r);
    }
  else
    {
      channel->id = mixer_get_next_channel_id ();
      channel->output = MIXER->master;
      channel->output_id = MIXER->master->id;
    }

  if (type == CT_BUS ||
      type == CT_AUDIO ||
      type == CT_MASTER)
    {
      /* connect stereo in to stereo out */
      port_connect (channel->stereo_in->l,
                    channel->stereo_out->l);
      port_connect (channel->stereo_in->r,
                    channel->stereo_out->r);
    }

  if (type != CT_MASTER)
    {
      /* connect channel out ports to master */
      port_connect (channel->stereo_out->l,
                    MIXER->master->stereo_in->l);
      port_connect (channel->stereo_out->r,
                    MIXER->master->stereo_in->r);
    }

  channel->track = track_new (channel, label);
  generate_automatables (channel);

  g_message ("Created channel %s of type %i", label, type);

  return channel;
}

void
channel_set_phase (void * _channel, float phase)
{
  Channel * channel = (Channel *) _channel;
  channel->fader.phase = phase;

  /* FIXME use an event */
  /*if (channel->widget)*/
    /*gtk_label_set_text (channel->widget->phase_reading,*/
                        /*g_strdup_printf ("%.1f", phase));*/
}

float
channel_get_phase (void * _channel)
{
  Channel * channel = (Channel *) _channel;
  return channel->fader.phase;
}

/*void*/
/*channel_set_volume (void * _channel, float volume)*/
/*{*/
  /*Channel * channel = (Channel *) _channel;*/
  /*channel->volume = volume;*/
  /*[> TODO update tooltip <]*/
  /*gtk_label_set_text (channel->widget->phase_reading,*/
                      /*g_strdup_printf ("%.1f", volume));*/
  /*g_idle_add ((GSourceFunc) gtk_widget_queue_draw,*/
              /*GTK_WIDGET (channel->widget->fader));*/
/*}*/

/*float*/
/*channel_get_volume (void * _channel)*/
/*{*/
  /*Channel * channel = (Channel *) _channel;*/
  /*return channel->volume;*/
/*}*/

/*static int*/
/*redraw_fader_asnyc (Channel * channel)*/
/*{*/
  /*gtk_widget_queue_draw (*/
    /*GTK_WIDGET (channel->widget->fader));*/

  /*return FALSE;*/
/*}*/


/*static int*/
/*redraw_pan_async (Channel * channel)*/
/*{*/
  /*gtk_widget_queue_draw (*/
    /*GTK_WIDGET (channel->widget->pan));*/

  /*return FALSE;*/
/*}*/

void
channel_set_pan (void * _channel, float pan)
{
  Channel * channel = (Channel *) _channel;
  channel->fader.pan = pan;
  /*g_idle_add ((GSourceFunc) redraw_pan_async,*/
              /*channel);*/
}

float
channel_get_pan (void * _channel)
{
  Channel * channel = (Channel *) _channel;
  return channel->fader.pan;
}

float
channel_get_current_l_db (void * _channel)
{
  Channel * channel = (Channel *) _channel;
  return channel->fader.l_port_db;
}

float
channel_get_current_r_db (void * _channel)
{
  Channel * channel = (Channel *) _channel;
  return channel->fader.r_port_db;
}

void
channel_set_current_l_db (Channel * channel, float val)
{
  channel->fader.l_port_db = val;
}

void
channel_set_current_r_db (Channel * channel, float val)
{
  channel->fader.r_port_db = val;
}

/**
 * Removes a plugin at pos from the channel.
 *
 * If deleting_channel is 1, the automation tracks
 * associated with he plugin are not deleted at
 * this time.
 */
void
channel_remove_plugin (
  Channel * channel,
  int pos,
  int deleting_channel)
{
  Plugin * plugin = channel->plugins[pos];
  if (plugin)
    {
      g_message ("Removing %s from %s:%d",
                 plugin->descr->name,
                 channel->track->name, pos);
      channel->plugins[pos] = NULL;
      channel->plugin_ids[pos] = -1;
      if (plugin->descr->protocol == PROT_LV2)
        {
          Lv2Plugin * lv2_plugin =
            (Lv2Plugin *) plugin->original_plugin;
          if (GTK_IS_WIDGET (lv2_plugin->window))
            g_signal_handler_disconnect (
              lv2_plugin->window,
              lv2_plugin->delete_event_id);
          lv2_close_ui (lv2_plugin);
        }
      plugin_disconnect (plugin);
      free_later (plugin, plugin_free);
    }

  if (!deleting_channel)
    automation_tracklist_update (
      &channel->track->automation_tracklist);
}

/**
 * Adds given plugin to given position in the strip.
 *
 * The plugin must be already instantiated at this point.
 */
void
channel_add_plugin (Channel * channel,    ///< the channel
                    int         pos,     ///< the position in the strip
                                        ///< (starting from 0)
                    Plugin      * plugin  ///< the plugin to add
                    )
{
  int prev_enabled = channel->enabled;
  channel->enabled = 0;

  /* free current plugin */
  /*Plugin * old = channel->plugins[pos];*/
  channel_remove_plugin (channel, pos, 0);

  g_message ("Inserting %s at %s:%d", plugin->descr->name,
             channel->track->name, pos);
  channel->plugins[pos] = plugin;
  channel->plugin_ids[pos] = plugin->id;
  plugin->channel = channel;
  plugin->channel_id = channel->id;

  Plugin * next_plugin = NULL;
  for (int i = pos + 1; i < STRIP_SIZE; i++)
    {
      next_plugin = channel->plugins[i];
      if (next_plugin)
        break;
    }

  Plugin * prev_plugin = NULL;
  for (int i = pos - 1; i >= 0; i--)
    {
      prev_plugin = channel->plugins[i];
      if (prev_plugin)
        break;
    }

  /* ------------------------------------------------------
   * connect input ports
   * ------------------------------------------------------*/
  /* if main plugin or no other plugin before it */
  if (pos == 0 || !prev_plugin)
    {
      switch (channel->type)
        {
        case CT_AUDIO:
          /* TODO connect L and R audio ports for recording */
          break;
        case CT_MIDI:
          /* Connect MIDI port and piano roll to the plugin */
          for (int i = 0; i < plugin->num_in_ports; i++)
            {
              Port * port = plugin->in_ports[i];
              if (port->type == TYPE_EVENT &&
                  port->flow == FLOW_INPUT)
                {
                  /*if (channel->recording)*/
                    port_connect (AUDIO_ENGINE->midi_in, port);
                    port_connect (channel->piano_roll, port);
                }
            }
          break;
        case CT_BUS:
        case CT_MASTER:
          /* disconnect channel stereo in */
          port_disconnect_all (channel->stereo_in->l);
          port_disconnect_all (channel->stereo_in->r);

          /* connect channel stereo in to plugin */
          int last_index = 0;
          for (int j = 0; j < 2; j++)
            {
              for (;
                   last_index < plugin->num_in_ports;
                   last_index++)
                {
                  if (plugin->in_ports[last_index]->type ==
                      TYPE_AUDIO)
                    {
                      if (j == 0)
                        {
                          port_connect (
                            channel->stereo_in->l,
                            plugin->in_ports[last_index]);
                          break;
                        }
                      else
                        {
                          port_connect (
                            channel->stereo_in->r,
                            plugin->in_ports[last_index]);
                          break;
                        }
                    }
                }
            }
          break;
        }
    }
  else if (prev_plugin)
    {
      /* connect each input port from the previous plugin sequentially */

      /* connect output AUDIO ports of prev plugin to AUDIO input ports of
       * plugin */
      int last_index = 0;
      for (int i = 0; i < plugin->num_in_ports; i++)
        {
          if (plugin->in_ports[i]->type == TYPE_AUDIO)
            {
              for (int j = last_index; j < prev_plugin->num_out_ports; j++)
                {
                  if (prev_plugin->out_ports[j]->type == TYPE_AUDIO)
                    {
                      port_connect (prev_plugin->out_ports[j],
                                    plugin->in_ports[i]);
                    }
                  last_index = j;
                  if (last_index == prev_plugin->num_out_ports - 1)
                    break;
                }
            }
        }
    }


  /* ------------------------------------------------------
   * connect output ports
   * ------------------------------------------------------*/


  /* connect output AUDIO ports of plugin to AUDIO input ports of
   * next plugin */
  if (next_plugin)
    {
      int last_index = 0;
      for (int i = 0; i < plugin->num_out_ports; i++)
        {
          if (plugin->out_ports[i]->type == TYPE_AUDIO)
            {
              for (int j = last_index; j < next_plugin->num_in_ports; j++)
                {
                  if (next_plugin->in_ports[j]->type == TYPE_AUDIO)
                    {
                      port_connect (plugin->out_ports[i],
                                    next_plugin->in_ports[j]);
                    }
                  last_index = j;
                  if (last_index == next_plugin->num_in_ports - 1)
                    break;
                }
            }
        }
    }
  /* if last plugin, connect to channel's AUDIO_OUT */
  else
    {
      int count = 0;
      for (int i = 0; i < plugin->num_out_ports; i++)
        {
          /* prepare destinations */
          Port * dests[2];
          dests[0] = channel->stereo_out->l;
          dests[1] = channel->stereo_out->r;

          /* connect to destinations one by one */
          Port * port = plugin->out_ports[i];
          if (port->type == TYPE_AUDIO)
            {
              port_connect (port, dests[count++]);
            }
          if (count == 2)
            break;
        }
    }
  channel->enabled = prev_enabled;

  plugin_generate_automatables (plugin);

  EVENTS_PUSH (ET_PLUGIN_ADDED,
               plugin);

  mixer_recalculate_graph (MIXER, 1);
}

/**
 * Returns the index of the last active slot.
 */
int
channel_get_last_active_slot_index (Channel * channel)
{
  int index = -1;
  for (int i = 0; i < STRIP_SIZE; i++)
    {
      if (channel->plugins[i])
        index = i;
    }
  return index;
}

/**
 * Returns the index on the mixer.
 */
int
channel_get_index (Channel * channel)
{
  for (int i = 0; i < MIXER->num_channels; i++)
    {
      if (MIXER->channels[i] == channel)
        return i;
    }
  g_error ("Channel index for %s not found", channel->track->name);
}

/**
 * Convenience method to get the first active plugin in the channel
 */
Plugin *
channel_get_first_plugin (Channel * channel)
{
  /* find first plugin */
  Plugin * plugin = NULL;
  for (int i = 0; i < STRIP_SIZE; i++)
    {
      if (channel->plugins[i])
        {
          plugin = channel->plugins[i];
          break;
        }
    }
  return plugin;
}


/**
 * Connects or disconnects the MIDI editor key press port to the channel's
 * first plugin
 */
void
channel_reattach_midi_editor_manual_press_port (Channel * channel,
                                                int     connect)
{
  /* find first plugin */
  Plugin * plugin = channel_get_first_plugin (channel);

  if (plugin)
    {
      if (channel->type == CT_MIDI)
        {
          /* Connect/Disconnect MIDI editor manual press port to the plugin */
          for (int i = 0; i < plugin->num_in_ports; i++)
            {
              Port * port = plugin->in_ports[i];
              if (port->type == TYPE_EVENT &&
                  port->flow == FLOW_INPUT)
                {
                  if (connect)
                    {
                      port_connect (AUDIO_ENGINE->midi_editor_manual_press, port);
                    }
                  else
                    {
                    port_disconnect (AUDIO_ENGINE->midi_editor_manual_press, port);
                    }
                }
            }
        }
    }
}

/**
 * Returns the plugin's strip index on the channel
 */
int
channel_get_plugin_index (Channel * channel,
                          Plugin *  plugin)
{
  for (int i = 0; i < STRIP_SIZE; i++)
    {
      if (channel->plugins[i] == plugin)
        {
          return i;
        }
    }
  g_warning ("channel_get_plugin_index: plugin not found");
  return -1;
}

/**
 * Convenience function to get the fader automatable of the channel.
 */
Automatable *
channel_get_automatable (Channel *       channel,
                         AutomatableType type)
{
  for (int i = 0; i < channel->num_automatables; i++)
    {
      Automatable * automatable = channel->automatables[i];

      if (type == automatable->type)
        return automatable;
    }
  return NULL;
}

/*static void*/
/*remove_automatable (*/
  /*Channel * self,*/
  /*Automatable * a)*/
/*{*/
  /*AutomationTracklist * atl =*/
    /*&self->track->automation_tracklist;*/

  /*for (int i = 0;*/
       /*i < atl->num_automation_tracks; i++)*/
    /*{*/
      /*if (atl->automation_tracks[i]->*/
            /*automatable == a)*/
        /*automation_tracklist_remove_*/

    /*}*/

  /*project_remove_automatable (a);*/
  /*automatable_free (a);*/
/*}*/

/**
 * Disconnects the channel from the processing
 * chain.
 *
 * This should be called immediately when the
 * channel is getting deleted, and channel_free
 * should be designed to be called later after
 * an arbitrary delay.
 */
void
channel_disconnect (Channel * channel)
{
  FOREACH_STRIP
    {
      if (channel->plugins[i])
        {
          channel_remove_plugin (channel, i, 1);
        }
    }
  port_disconnect_all (channel->stereo_in->l);
  port_disconnect_all (channel->stereo_in->r);
  port_disconnect_all (channel->midi_in);
  port_disconnect_all (channel->piano_roll);
  port_disconnect_all (channel->stereo_out->l);
  port_disconnect_all (channel->stereo_out->r);
}

/**
 * Frees the channel.
 */
void
channel_free (Channel * channel)
{
  g_warn_if_fail (channel);
  project_remove_channel (channel);

  project_remove_port (channel->stereo_in->l);
  port_free (channel->stereo_in->l);
  project_remove_port (channel->stereo_in->r);
  port_free (channel->stereo_in->r);
  project_remove_port (channel->midi_in);
  port_free (channel->midi_in);
  project_remove_port (channel->piano_roll);
  port_free (channel->piano_roll);
  project_remove_port (channel->stereo_out->l);
  port_free (channel->stereo_out->l);
  project_remove_port (channel->stereo_out->r);
  port_free (channel->stereo_out->r);

  Automatable * a;
  FOREACH_AUTOMATABLE (channel)
    {
      a = channel->automatables[i];
      /*remove_automatable (channel, a);*/
      project_remove_automatable (a);
      automatable_free (a);
    }

  if (channel->widget)
    gtk_widget_destroy (
      GTK_WIDGET (channel->widget));
}

