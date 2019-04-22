/*
 * Copyright (C) 2018-2019 Alexandros Theodotou <alex at zrythm dot org>
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

/** \file
 * The audio engine of zyrthm. */

#include <math.h>
#include <stdlib.h>
#include <signal.h>

#include "config.h"

#include "audio/automation_track.h"
#include "audio/automation_tracklist.h"
#include "audio/channel.h"
#include "audio/engine.h"
#include "audio/engine_jack.h"
#include "audio/engine_pa.h"
#include "audio/midi.h"
#include "audio/mixer.h"
#include "audio/routing.h"
#include "audio/transport.h"
#include "plugins/plugin.h"
#include "plugins/plugin_manager.h"
#include "plugins/lv2_plugin.h"
#include "project.h"
#include "zrythm.h"

#include <gtk/gtk.h>

#ifdef HAVE_JACK
#include <jack/jack.h>
#include <jack/midiport.h>
#endif

 void
engine_update_frames_per_tick (int beats_per_bar,
                               int bpm,
                               int sample_rate)
{
  AUDIO_ENGINE->frames_per_tick =
    (sample_rate * 60.f * beats_per_bar) /
    (bpm * TICKS_PER_BAR);

  /* update positions */
  transport_update_position_frames (&AUDIO_ENGINE->transport);
}

/**
 * Init audio engine.
 *
 * loading is 1 if loading a project.
 */
void
engine_init (AudioEngine * self,
             int           loading)
{
  g_message ("Initializing audio engine...");

  transport_init (&self->transport,
                  loading);

  int ab_code =
    g_settings_get_enum (
      S_PREFERENCES,
      "audio-backend");
  self->audio_backend = AUDIO_BACKEND_NONE;
  switch (ab_code)
    {
    case 1:
      self->audio_backend = AUDIO_BACKEND_JACK;
      break;
    case 3:
      self->audio_backend = AUDIO_BACKEND_PORT_AUDIO;
      break;
    default:
      g_warn_if_reached ();
      break;
    }

  int mb_code =
    g_settings_get_enum (
      S_PREFERENCES,
      "midi-backend");
  self->midi_backend = MIDI_BACKEND_NONE;
  switch (mb_code)
    {
    case 1:
      self->midi_backend = MIDI_BACKEND_JACK;
      break;
    default:
      break;
    }

  /* init semaphores */
  zix_sem_init (&self->port_operation_lock, 1);
  /*zix_sem_init (&MIXER->channel_process_sem, 1);*/

  /* load ports from IDs */
  if (loading)
    {
      mixer_init_loaded ();

      stereo_ports_init_loaded (self->stereo_in);
      stereo_ports_init_loaded (self->stereo_out);
      self->midi_in =
        project_get_port (
          self->midi_in_id);
      self->midi_editor_manual_press =
        project_get_port (
          self->midi_editor_manual_press_id);
    }

  switch (self->audio_backend)
    {
#ifdef HAVE_JACK
    case AUDIO_BACKEND_JACK:
      jack_setup (self, loading);
      break;
#endif
#ifdef HAVE_PORT_AUDIO
    case AUDIO_BACKEND_PORT_AUDIO:
      pa_setup (self);
      break;
#endif
    case NUM_AUDIO_BACKENDS:
    default:
      g_warn_if_reached ();
      break;
    }

  self->buf_size_set = false;
}

void
audio_engine_close (AudioEngine * self)
{
  g_message ("closing audio engine...");

  switch (self->audio_backend)
    {
#ifdef HAVE_JACK
    case AUDIO_BACKEND_JACK:
      jack_client_close (AUDIO_ENGINE->client);
      break;
#endif
#ifdef HAVE_PORT_AUDIO
    case AUDIO_BACKEND_PORT_AUDIO:
      pa_terminate (AUDIO_ENGINE);
      break;
#endif
    case NUM_AUDIO_BACKENDS:
    default:
      g_warn_if_reached ();
      break;
    }
}

/**
 * To be called by each implementation to prepare the
 * structures before processing.
 *
 * Clears buffers, marks all as unprocessed, etc.
 */
void
engine_process_prepare (uint32_t nframes)
{
  int i, j;
  AUDIO_ENGINE->last_time_taken =
    g_get_monotonic_time ();
  AUDIO_ENGINE->nframes = nframes;

  if (TRANSPORT->play_state == PLAYSTATE_PAUSE_REQUESTED)
    {
      g_message ("pause requested handled");
      /*TRANSPORT->play_state = PLAYSTATE_PAUSED;*/
      /*zix_sem_post (&TRANSPORT->paused);*/
    }
  else if (TRANSPORT->play_state == PLAYSTATE_ROLL_REQUESTED)
    {
      TRANSPORT->play_state = PLAYSTATE_ROLLING;
    }

  int ret =
    zix_sem_try_wait (
      &AUDIO_ENGINE->port_operation_lock);
  if (!ret && !AUDIO_ENGINE->exporting)
    {
      AUDIO_ENGINE->skip_cycle = 1;
      return;
    }

  /* reset all buffers */
  port_clear_buffer (AUDIO_ENGINE->midi_in);

  /* prepare channels for this cycle */
  for (i = -1; i < MIXER->num_channels; i++)
    {
      Channel * channel;
      if (i == -1)
        channel = MIXER->master;
      else
        channel = MIXER->channels[i];
      /*for (int j = 0; j < STRIP_SIZE; j++)*/
        /*{*/
          /*if (channel->plugins[j])*/
            /*{*/
              /*g_atomic_int_set (*/
                /*&channel->plugins[j]->processed, 0);*/
            /*}*/
        /*}*/
      /*g_atomic_int_set (*/
        /*&channel->processed, 0);*/
      channel_prepare_process (channel);
    }

  /* for each automation track, update the val */
  /* TODO move to routing process node */
  AutomationTracklist * atl;
  AutomationTrack * at;
  float val;
  for (i = 0; i < TRACKLIST->num_tracks; i++)
    {
      atl =
        track_get_automation_tracklist (
          TRACKLIST->tracks[i]);
      if (!atl)
        continue;
      for (j = 0;
           j < atl->num_automation_tracks;
           j++)
        {
          at = atl->automation_tracks[j];
          val =
            automation_track_get_normalized_val_at_pos (
              at, &PLAYHEAD);
          /*g_message ("val received %f",*/
                     /*val);*/
          /* if there was an automation event
           * at the playhead position, val is
           * positive */
          if (val >= 0.f)
            automatable_set_val_from_normalized (
              at->automatable,
              val);
        }
    }

  AUDIO_ENGINE->filled_stereo_out_bufs = 0;
}

static void
receive_midi_events (
  AudioEngine * self,
  uint32_t      nframes,
  int           print)
{
  switch (self->midi_backend)
    {
#ifdef HAVE_JACK
    case MIDI_BACKEND_JACK:
      engine_jack_receive_midi_events (
        self, nframes, print);
      break;
#endif
    case NUM_MIDI_BACKENDS:
    default:
      break;
    }
}

static long count = 0;
/**
 * Processes current cycle.
 *
 * To be called by each implementation in its
 * callback.
 */
int
engine_process (
  AudioEngine * self,
  uint32_t      nframes)
{
  if (!g_atomic_int_get (&self->run))
    return 0;

  count++;
  self->cycle = count;

  /* run pre-process code */
  engine_process_prepare (nframes);

  if (AUDIO_ENGINE->skip_cycle)
    {
      AUDIO_ENGINE->skip_cycle = 0;
      return 0;
    }

  /* puts MIDI in events in the MIDI in port */
  receive_midi_events (self, nframes, 1);

  /* this will keep looping until everything was
   * processed in this cycle */
  /*mixer_process (nframes);*/
  /*g_message ("==========================================================================");*/
  router_start_cycle (MIXER->graph);
  /*g_message ("end==========================================================================");*/

  /* run post-process code */
  engine_post_process (self);

  /*
   * processing finished, return 0 (OK)
   */
  return 0;
}

/**
 * To be called after processing for common logic.
 */
void
engine_post_process (AudioEngine * self)
{
  zix_sem_post (&self->port_operation_lock);

  /* stop panicking */
  if (self->panic)
    {
      self->panic = 0;
    }

  /* already processed recording start */
  /*if (IS_TRANSPORT_ROLLING && TRANSPORT->starting_recording)*/
    /*{*/
      /*TRANSPORT->starting_recording = 0;*/
    /*}*/

  /* loop position back if about to exit loop */
  if (TRANSPORT->loop && /* if looping */
      IS_TRANSPORT_ROLLING && /* if rolling */
      (TRANSPORT->playhead_pos.frames <=  /* if current pos is inside loop */
          TRANSPORT->loop_end_pos.frames) &&
      ((TRANSPORT->playhead_pos.frames + self->nframes) > /* if next pos will be outside loop */
          TRANSPORT->loop_end_pos.frames))
    {
      transport_move_playhead (
        &TRANSPORT->loop_start_pos,
        1);
    }
  else if (IS_TRANSPORT_ROLLING)
    {
      /* move playhead as many samples as processed */
      transport_add_to_playhead (self->nframes);
    }

  AUDIO_ENGINE->last_time_taken =
    g_get_monotonic_time () - AUDIO_ENGINE->last_time_taken;
  /*g_message ("last time taken: %ld");*/
  if (AUDIO_ENGINE->max_time_taken <
      AUDIO_ENGINE->last_time_taken)
    AUDIO_ENGINE->max_time_taken =
      AUDIO_ENGINE->last_time_taken;
  /*g_message ("last time %ld, max time %ld",*/
             /*AUDIO_ENGINE->last_time_taken,*/
             /*AUDIO_ENGINE->max_time_taken);*/
}

/**
 * Closes any connections and free's data.
 */
void
engine_tear_down ()
{
  g_message ("tearing down audio engine...");

#ifdef HAVE_JACK
  if (AUDIO_ENGINE->audio_backend ==
        AUDIO_BACKEND_JACK)
    jack_tear_down ();
#endif

  /* TODO free data */
}
