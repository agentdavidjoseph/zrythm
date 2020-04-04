/*
 * Copyright (C) 2018-2020 Alexandros Theodotou <alex at zrythm dot org>
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

#include "gui/backend/tracklist_selections.h"
#include "gui/widgets/center_dock.h"
#include "gui/widgets/foldable_notebook.h"
#include "gui/widgets/main_window.h"
#include "gui/widgets/inspector.h"
#include "gui/widgets/inspector_plugin.h"
#include "gui/widgets/inspector_track.h"
#include "gui/widgets/left_dock_edge.h"
#include "gui/widgets/visibility.h"
#include "settings/settings.h"
#include "project.h"
#include "utils/gtk.h"
#include "utils/resources.h"
#include "utils/ui.h"
#include "zrythm.h"

#include <glib/gi18n.h>

G_DEFINE_TYPE (LeftDockEdgeWidget,
               left_dock_edge_widget,
               GTK_TYPE_BOX)

void
left_dock_edge_widget_refresh (
  LeftDockEdgeWidget * self)
{
  if (PROJECT->last_selection ==
        SELECTION_TYPE_TRACK)
    {
      inspector_track_widget_show_tracks (
        self->track_inspector,
        TRACKLIST_SELECTIONS);
    }
  else if (PROJECT->last_selection ==
           SELECTION_TYPE_PLUGIN)
    {
      inspector_plugin_widget_show (
        self->plugin_inspector, MIXER_SELECTIONS);
    }
}

void
left_dock_edge_widget_setup (
  LeftDockEdgeWidget * self)
{
  foldable_notebook_widget_setup (
    self->inspector_notebook,
    MW_CENTER_DOCK->left_rest_paned,
    GTK_POS_LEFT);

  /*inspector_widget_setup (*/
    /*self->inspector);*/
  inspector_track_widget_setup (
    self->track_inspector, TRACKLIST_SELECTIONS);

  visibility_widget_refresh (
    self->visibility);
}

static void
left_dock_edge_widget_init (
  LeftDockEdgeWidget * self)
{
  gtk_widget_init_template (GTK_WIDGET (self));

  const int min_width = 180;
  GtkWidget * img;

  /* setup track inspector */
  self->track_inspector =
    inspector_track_widget_new ();
  img =
    gtk_image_new_from_icon_name (
      "z-media-album-track",
      GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_widget_set_tooltip_text (
    img, _("Track inspector"));
  GtkBox * inspector_box =
    GTK_BOX (
      gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  gtk_container_add (
    GTK_CONTAINER (inspector_box),
    GTK_WIDGET (self->track_inspector));
  gtk_widget_set_visible (
    GTK_WIDGET (inspector_box), 1);
  gtk_notebook_prepend_page (
    GTK_NOTEBOOK (self->inspector_notebook),
    GTK_WIDGET (inspector_box), img);
  gtk_widget_set_size_request (
    GTK_WIDGET (self->track_inspector),
    min_width, -1);

  /* setup plugin inspector */
  self->plugin_inspector =
    inspector_plugin_widget_new ();
  img =
    gtk_image_new_from_icon_name (
      "plug-solid-small",
      GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_widget_set_tooltip_text (
    img, _("Plugin inspector"));
  inspector_box =
    GTK_BOX (
      gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  gtk_container_add (
    GTK_CONTAINER (inspector_box),
    GTK_WIDGET (self->plugin_inspector));
  gtk_widget_set_visible (
    GTK_WIDGET (inspector_box), 1);
  gtk_notebook_append_page (
    GTK_NOTEBOOK (self->inspector_notebook),
    GTK_WIDGET (inspector_box), img);
  gtk_widget_set_size_request (
    GTK_WIDGET (self->plugin_inspector),
    min_width, -1);

  /* setup visibility */
  img =
    gtk_image_new_from_icon_name (
      "z-view-visible",
      GTK_ICON_SIZE_LARGE_TOOLBAR);
  gtk_widget_set_tooltip_text (
    img, _("Visibility"));
  self->visibility = visibility_widget_new ();
  gtk_widget_set_visible (
    GTK_WIDGET (self->visibility), 1);
  GtkBox * visibility_box =
    GTK_BOX (
      gtk_box_new (GTK_ORIENTATION_VERTICAL, 0));
  gtk_container_add (
    GTK_CONTAINER (visibility_box),
    GTK_WIDGET (self->visibility));
  gtk_widget_set_visible (
    GTK_WIDGET (visibility_box), 1);
  gtk_notebook_append_page (
    GTK_NOTEBOOK (self->inspector_notebook),
    GTK_WIDGET (visibility_box),
    img);
  gtk_widget_set_size_request (
    GTK_WIDGET (self->visibility),
    min_width, -1);
}

static void
left_dock_edge_widget_class_init (
  LeftDockEdgeWidgetClass * _klass)
{
  GtkWidgetClass * klass = GTK_WIDGET_CLASS (_klass);
  resources_set_class_template (
    klass, "left_dock_edge.ui");

  gtk_widget_class_set_css_name (
    klass, "left-dock-edge");

  gtk_widget_class_bind_template_child (
    GTK_WIDGET_CLASS (klass),
    LeftDockEdgeWidget,
    inspector_notebook);
}
