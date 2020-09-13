/*
 * Copyright (C) 2019-2020 Alexandros Theodotou <alex at zrythm dot org>
 *
 * This file is part of Zrythm
 *
 * Zrythm is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Zrythm is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Zrythm.  If not, see <https://www.gnu.org/licenses/>.
 */

/**
 * \file
 *
 * Action for a group of ArrangerObject's.
 */

#ifndef __UNDO_ARRANGER_SELECTIONS_ACTION_H__
#define __UNDO_ARRANGER_SELECTIONS_ACTION_H__

#include <stdbool.h>
#include <stdint.h>

#include "actions/undoable_action.h"
#include "audio/midi_function.h"
#include "audio/position.h"
#include "audio/quantize_options.h"
#include "gui/backend/automation_selections.h"
#include "gui/backend/chord_selections.h"
#include "gui/backend/midi_arranger_selections.h"
#include "gui/backend/timeline_selections.h"

typedef struct ArrangerSelections ArrangerSelections;
typedef struct ArrangerObject ArrangerObject;
typedef struct Position Position;
typedef struct QuantizeOptions QuantizeOptions;

/**
 * @addtogroup actions
 *
 * @{
 */

/**
 * Type used when the action is a RESIZE action.
 */
typedef enum ArrangerSelectionsActionResizeType
{
  ARRANGER_SELECTIONS_ACTION_RESIZE_L,
  ARRANGER_SELECTIONS_ACTION_RESIZE_R,
  ARRANGER_SELECTIONS_ACTION_RESIZE_L_LOOP,
  ARRANGER_SELECTIONS_ACTION_RESIZE_R_LOOP,
  ARRANGER_SELECTIONS_ACTION_RESIZE_L_FADE,
  ARRANGER_SELECTIONS_ACTION_RESIZE_R_FADE,
  ARRANGER_SELECTIONS_ACTION_STRETCH_L,
  ARRANGER_SELECTIONS_ACTION_STRETCH_R,
} ArrangerSelectionsActionResizeType;

static const cyaml_strval_t
arranger_selections_action_resize_type_strings[] =
{
  { "Resize L",
    ARRANGER_SELECTIONS_ACTION_RESIZE_L },
  { "Resize R",
    ARRANGER_SELECTIONS_ACTION_RESIZE_R },
  { "Resize L (loop)",
    ARRANGER_SELECTIONS_ACTION_RESIZE_L_LOOP },
  { "Resize R (loop)",
    ARRANGER_SELECTIONS_ACTION_RESIZE_R_LOOP },
  { "Resize L (fade)",
    ARRANGER_SELECTIONS_ACTION_RESIZE_L_FADE },
  { "Resize R (fade)",
    ARRANGER_SELECTIONS_ACTION_RESIZE_R_FADE },
  { "Stretch L",
    ARRANGER_SELECTIONS_ACTION_STRETCH_L },
  { "Stretch R",
    ARRANGER_SELECTIONS_ACTION_STRETCH_R },
};

/**
 * Type used when the action is an EDIT action.
 */
typedef enum ArrangerSelectionsActionEditType
{
  /** Edit the name of the ArrangerObject's in the
   * selection. */
  ARRANGER_SELECTIONS_ACTION_EDIT_NAME,

  /**
   * Edit a Position of the ArrangerObject's in
   * the selection.
   *
   * This will just set all of the positions on the
   * object.
   */
  ARRANGER_SELECTIONS_ACTION_EDIT_POS,

  /**
   * Edit a primitive (int, etc) member of
   * ArrangerObject's in the selection.
   *
   * This will simply set all relevant primitive
   * values in an ArrangerObject when doing/undoing.
   */
  ARRANGER_SELECTIONS_ACTION_EDIT_PRIMITIVE,

  /** For editing the MusicalScale inside
   * ScaleObject's. */
  ARRANGER_SELECTIONS_ACTION_EDIT_SCALE,

  /** Editing fade positions or curve options. */
  ARRANGER_SELECTIONS_ACTION_EDIT_FADES,

  /** Change mute status. */
  ARRANGER_SELECTIONS_ACTION_EDIT_MUTE,

  /** For ramping MidiNote velocities or
   * AutomationPoint values.
   * (this is handled by EDIT_PRIMITIVE) */
  //ARRANGER_SELECTIONS_ACTION_EDIT_RAMP,

  /** MIDI function. */
  ARRANGER_SELECTIONS_ACTION_EDIT_MIDI_FUNCTION,
} ArrangerSelectionsActionEditType;

static const cyaml_strval_t
arranger_selections_action_edit_type_strings[] =
{
  { "Name",
    ARRANGER_SELECTIONS_ACTION_EDIT_NAME },
  { "Pos",
    ARRANGER_SELECTIONS_ACTION_EDIT_POS },
  { "Primitive",
    ARRANGER_SELECTIONS_ACTION_EDIT_PRIMITIVE },
  { "Scale",
    ARRANGER_SELECTIONS_ACTION_EDIT_SCALE },
  { "Fades",
    ARRANGER_SELECTIONS_ACTION_EDIT_FADES },
  { "Mute",
    ARRANGER_SELECTIONS_ACTION_EDIT_MUTE },
  { "MIDI function",
    ARRANGER_SELECTIONS_ACTION_EDIT_MIDI_FUNCTION },
};

/**
 * The action.
 */
typedef struct ArrangerSelectionsAction
{
  UndoableAction       parent_instance;

  /**
   * A clone of the ArrangerSelections.
   */
  ArrangerSelections * sel;

  /**
   * A clone of the ArrangerSelections after the
   * change (used in the EDIT action and
   * quantize).
   */
  ArrangerSelections * sel_after;

  /** Type of edit action, if an Edit action. */
  ArrangerSelectionsActionEditType edit_type;

  ArrangerSelectionsActionResizeType resize_type;

  /** Ticks diff. */
  double               ticks;
  /** Tracks moved. */
  int                  delta_tracks;
  /** Lanes moved. */
  int                  delta_lanes;
  /** Chords moved (up/down in the Chord editor). */
  int                  delta_chords;
  /** Delta of MidiNote pitch. */
  int                  delta_pitch;
  /** Delta of MidiNote velocity. */
  int                  delta_vel;
  /**
   * Difference in a normalized amount, such as
   * automation point normalized value.
   */
  double               delta_normalized_amount;

  /** String, when changing a string. */
  char *               str;

  /** Position, when changing a Position. */
  Position             pos;

  /** Used when splitting - these are the split
   * ArrangerObject's. */
  ArrangerObject *     r1[800];
  ArrangerObject *     r2[800];

  /** Number of split objects inside r1 and r2
   * each. */
  int                  num_split_objs;

  /**
   * If this is 1, the first "do" call does
   * nothing in some cases.
   *
   * Set internally and either used or ignored.
   */
  int                  first_run;

  /** QuantizeOptions clone, if quantizing. */
  QuantizeOptions *    opts;

  /** The original velocities when ramping. */
  uint8_t *            vel_before;

  /** The velocities changed to when ramping. */
  uint8_t *            vel_after;

  /**
   * The arranger object, if the action can only
   * affect a single object rather than selections,
   * like a region name change.
   */
  //ArrangerObject *     obj;

  /* --- below for serialization only --- */
  ChordSelections *    chord_sel;
  ChordSelections *    chord_sel_after;
  TimelineSelections * tl_sel;
  TimelineSelections * tl_sel_after;
  MidiArrangerSelections * ma_sel;
  MidiArrangerSelections * ma_sel_after;
  AutomationSelections * automation_sel;
  AutomationSelections * automation_sel_after;

  /* arranger objects that can be split */
  ZRegion *            region_r1[800];
  ZRegion *            region_r2[800];
  MidiNote *           mn_r1[800];
  MidiNote *           mn_r2[800];

  /** Used for automation autofill action. */
  ZRegion *            region_before;
  ZRegion *            region_after;

  /* single objects */
#if 0
  ZRegion *            region;
  MidiNote *           midi_note;
  ScaleObject *        scale;
  Marker *             marker;
#endif

} ArrangerSelectionsAction;

static const cyaml_schema_field_t
  arranger_selections_action_fields_schema[] =
{
  CYAML_FIELD_MAPPING (
    "parent_instance", CYAML_FLAG_DEFAULT,
    ArrangerSelectionsAction, parent_instance,
    undoable_action_fields_schema),
  YAML_FIELD_ENUM (
    ArrangerSelectionsAction, edit_type,
    arranger_selections_action_edit_type_strings),
  YAML_FIELD_ENUM (
    ArrangerSelectionsAction, resize_type,
    arranger_selections_action_resize_type_strings),
  YAML_FIELD_FLOAT (
    ArrangerSelectionsAction, ticks),
  YAML_FIELD_INT (
    ArrangerSelectionsAction, delta_tracks),
  YAML_FIELD_INT (
    ArrangerSelectionsAction, delta_lanes),
  YAML_FIELD_INT (
    ArrangerSelectionsAction, delta_chords),
  YAML_FIELD_INT (
    ArrangerSelectionsAction, delta_pitch),
  YAML_FIELD_INT (
    ArrangerSelectionsAction, delta_vel),
  YAML_FIELD_FLOAT (
    ArrangerSelectionsAction,
    delta_normalized_amount),
  YAML_FIELD_STRING_PTR_OPTIONAL (
    ArrangerSelectionsAction, str),
  YAML_FIELD_MAPPING_EMBEDDED (
    ArrangerSelectionsAction, pos,
    position_fields_schema),
  YAML_FIELD_MAPPING_PTR_OPTIONAL (
    ArrangerSelectionsAction, opts,
    quantize_options_fields_schema),
  CYAML_FIELD_MAPPING_PTR (
    "chord_sel",
    CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
    ArrangerSelectionsAction, chord_sel,
    chord_selections_fields_schema),
  CYAML_FIELD_MAPPING_PTR (
    "tl_sel",
    CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
    ArrangerSelectionsAction, tl_sel,
    timeline_selections_fields_schema),
  YAML_FIELD_MAPPING_PTR_OPTIONAL (
    ArrangerSelectionsAction, ma_sel,
    midi_arranger_selections_fields_schema),
  YAML_FIELD_MAPPING_PTR_OPTIONAL (
    ArrangerSelectionsAction, automation_sel,
    automation_selections_fields_schema),
  YAML_FIELD_MAPPING_PTR_OPTIONAL (
    ArrangerSelectionsAction, chord_sel_after,
    chord_selections_fields_schema),
  YAML_FIELD_MAPPING_PTR_OPTIONAL (
    ArrangerSelectionsAction, tl_sel_after,
    timeline_selections_fields_schema),
  YAML_FIELD_MAPPING_PTR_OPTIONAL (
    ArrangerSelectionsAction, ma_sel_after,
    midi_arranger_selections_fields_schema),
  YAML_FIELD_MAPPING_PTR_OPTIONAL (
    ArrangerSelectionsAction, automation_sel_after,
    automation_selections_fields_schema),
  YAML_FIELD_MAPPING_PTR_OPTIONAL (
    ArrangerSelectionsAction, region_before,
    region_fields_schema),
  YAML_FIELD_MAPPING_PTR_OPTIONAL (
    ArrangerSelectionsAction, region_after,
    region_fields_schema),
  CYAML_FIELD_SEQUENCE_COUNT (
    "region_r1", CYAML_FLAG_DEFAULT,
    ArrangerSelectionsAction, region_r1,
    num_split_objs,
    &region_schema, 0, CYAML_UNLIMITED),
  CYAML_FIELD_SEQUENCE_COUNT (
    "region_r2", CYAML_FLAG_DEFAULT,
    ArrangerSelectionsAction, region_r2,
    num_split_objs,
    &region_schema, 0, CYAML_UNLIMITED),
  CYAML_FIELD_SEQUENCE_COUNT (
    "mn_r1", CYAML_FLAG_DEFAULT,
    ArrangerSelectionsAction, mn_r1,
    num_split_objs,
    &midi_note_schema, 0, CYAML_UNLIMITED),
  CYAML_FIELD_SEQUENCE_COUNT (
    "mn_r2", CYAML_FLAG_DEFAULT,
    ArrangerSelectionsAction, mn_r2,
    num_split_objs,
    &midi_note_schema, 0, CYAML_UNLIMITED),
#if 0
  CYAML_FIELD_MAPPING_PTR (
    "region", CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
    ArrangerSelectionsAction, region,
    region_fields_schema),
  CYAML_FIELD_MAPPING_PTR (
    "midi_note",
    CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
    ArrangerSelectionsAction, midi_note,
    midi_note_fields_schema),
  CYAML_FIELD_MAPPING_PTR (
    "scale",
    CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
    ArrangerSelectionsAction, scale,
    scale_object_fields_schema),
  CYAML_FIELD_MAPPING_PTR (
    "marker",
    CYAML_FLAG_POINTER | CYAML_FLAG_OPTIONAL,
    ArrangerSelectionsAction, marker,
    marker_fields_schema),
#endif

  CYAML_FIELD_END
};

static const cyaml_schema_value_t
  arranger_selections_action_schema =
{
  YAML_VALUE_PTR_NULLABLE (
    ArrangerSelectionsAction,
    arranger_selections_action_fields_schema),
};

void
arranger_selections_action_init_loaded (
  ArrangerSelectionsAction * self);

/**
 * Creates a new action for creating/deleting objects.
 *
 * @param create 1 to create 0 to delte.
 */
UndoableAction *
arranger_selections_action_new_create_or_delete (
  ArrangerSelections * sel,
  const bool           create);

#define \
arranger_selections_action_new_create(sel) \
  arranger_selections_action_new_create_or_delete ( \
    (ArrangerSelections *) sel, true)

#define \
arranger_selections_action_new_delete(sel) \
  arranger_selections_action_new_create_or_delete ( \
    (ArrangerSelections *) sel, false)

UndoableAction *
arranger_selections_action_new_record (
  ArrangerSelections * sel_before,
  ArrangerSelections * sel_after,
  const bool           already_recorded);

/**
 * Creates a new action for moving or duplicating
 * objects.
 *
 * @param move True to move, false to duplicate.
 * @param already_moved If this is true, the first
 *   DO will do nothing.
 * @param delta_normalized_amount Difference in a
 *   normalized amount, such as automation point
 *   normalized value.
 */
UndoableAction *
arranger_selections_action_new_move_or_duplicate (
  ArrangerSelections * sel,
  const bool           move,
  const double         ticks,
  const int            delta_chords,
  const int            delta_pitch,
  const int            delta_tracks,
  const int            delta_lanes,
  const double         delta_normalized_amount,
  const bool           already_moved);

/**
 * Creates a new action for linking regions.
 *
 * @param already_moved If this is true, the first
 *   DO will do nothing.
 * @param sel_before Original selections.
 * @param sel_after Selections after duplication.
 */
UndoableAction *
arranger_selections_action_new_link (
  ArrangerSelections * sel_before,
  ArrangerSelections * sel_after,
  const double         ticks,
  const int            delta_tracks,
  const int            delta_lanes,
  const bool           already_moved);

#define \
arranger_selections_action_new_move( \
  sel,ticks,chords,pitch,tracks,lanes,norm_amt, \
  already_moved) \
  arranger_selections_action_new_move_or_duplicate ( \
    (ArrangerSelections *) sel, 1, ticks, chords, \
    pitch, tracks, lanes, norm_amt, already_moved)
#define \
arranger_selections_action_new_duplicate( \
  sel,ticks,chords,pitch,tracks,lanes,norm_amt, \
  already_moved) \
  arranger_selections_action_new_move_or_duplicate ( \
    (ArrangerSelections *) sel, 0, ticks, chords, \
    pitch, tracks, lanes, norm_amt, already_moved)

#define \
arranger_selections_action_new_move_timeline( \
  sel,ticks,delta_tracks,delta_lanes, \
  already_moved) \
  arranger_selections_action_new_move ( \
    sel, ticks, 0, 0, delta_tracks, delta_lanes, \
    0, already_moved)
#define \
arranger_selections_action_new_duplicate_timeline( \
  sel,ticks,delta_tracks,delta_lanes, \
  already_moved) \
  arranger_selections_action_new_duplicate ( \
    sel, ticks, 0, 0, delta_tracks, delta_lanes, \
    0, already_moved)

#define \
arranger_selections_action_new_move_midi( \
  sel,ticks,delta_pitch, already_moved) \
  arranger_selections_action_new_move ( \
    sel, ticks, 0, delta_pitch, 0, 0, \
    0, already_moved)
#define \
arranger_selections_action_new_duplicate_midi( \
  sel,ticks,delta_pitch,already_moved) \
  arranger_selections_action_new_duplicate ( \
    sel, ticks, 0, delta_pitch, 0, 0, \
    0, already_moved)

#define \
arranger_selections_action_new_move_chord( \
  sel,ticks,delta_chords, already_moved) \
  arranger_selections_action_new_move ( \
    sel, ticks, delta_chords, 0, 0, 0, \
    0, already_moved)
#define \
arranger_selections_action_new_duplicate_chord( \
  sel,ticks,delta_chords, already_moved) \
  arranger_selections_action_new_duplicate ( \
    sel, ticks, delta_chords, 0, 0, 0, \
    0, already_moved)

#define \
arranger_selections_action_new_move_automation( \
  sel,ticks,norm_amt,already_moved) \
  arranger_selections_action_new_move ( \
    sel, ticks, 0, 0, 0, 0, norm_amt, already_moved)
#define \
arranger_selections_action_new_duplicate_automation( \
  sel,ticks,norm_amt,already_moved) \
  arranger_selections_action_new_duplicate ( \
    sel, ticks, 0, 0, 0, 0, norm_amt, already_moved)

/**
 * Creates a new action for editing properties
 * of an object.
 *
 * @param sel_before The selections before the
 *   change.
 * @param sel_after The selections after the
 *   change.
 * @param type Indication of which field has changed.
 */
UndoableAction *
arranger_selections_action_new_edit (
  ArrangerSelections *             sel_before,
  ArrangerSelections *             sel_after,
  ArrangerSelectionsActionEditType type,
  bool                             already_edited);

/**
 * Wrapper over
 * arranger_selections_action_new_edit() for MIDI
 * functions.
 */
UndoableAction *
arranger_selections_action_new_edit_midi_function (
  ArrangerSelections * sel_before,
  MidiFunctionType     midi_func_type);

/**
 * Creates a new action for automation autofill.
 *
 * @param region_before The region before the
 *   change.
 * @param region_after The region after the
 *   change.
 * @param already_changed Whether the change was
 *   already made.
 */
UndoableAction *
arranger_selections_action_new_automation_fill (
  ZRegion * region_before,
  ZRegion * region_after,
  bool      already_changed);

/**
 * Creates a new action for splitting
 * ArrangerObject's.
 *
 * @param pos Global position to split at.
 */
UndoableAction *
arranger_selections_action_new_split (
  ArrangerSelections * sel,
  const Position *     pos);

/**
 * Creates a new action for merging
 * ArrangerObject's.
 */
UndoableAction *
arranger_selections_action_new_merge (
  ArrangerSelections * sel);

/**
 * Creates a new action for resizing
 * ArrangerObject's.
 *
 * @param ticks How many ticks to add to the resizing
 *   edge.
 */
UndoableAction *
arranger_selections_action_new_resize (
  ArrangerSelections *               sel,
  ArrangerSelectionsActionResizeType type,
  const double                       ticks);

/**
 * Creates a new action fro quantizing
 * ArrangerObject's.
 *
 * @param opts Quantize options.
 */
UndoableAction *
arranger_selections_action_new_quantize (
  ArrangerSelections * sel,
  QuantizeOptions *    opts);

int
arranger_selections_action_do (
  ArrangerSelectionsAction * self);

int
arranger_selections_action_undo (
  ArrangerSelectionsAction * self);

char *
arranger_selections_action_stringize (
  ArrangerSelectionsAction * self);

void
arranger_selections_action_free (
  ArrangerSelectionsAction * self);

/**
 * @}
 */

#endif
