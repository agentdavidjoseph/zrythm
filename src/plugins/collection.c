/*
 * Copyright (C) 2020 Alexandros Theodotou <alex at zrythm dot org>
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

#include "plugins/collection.h"
#include "utils/objects.h"

void
plugin_collection_init_loaded (
  PluginCollection * self)
{
  g_return_if_fail (self);

  for (int i = 0; i < self->num_descriptors; i++)
    {
      self->descriptors[i]->category =
        plugin_descriptor_string_to_category (
          self->descriptors[i]->category_str);
    }
}

/**
 * Creates a new plugin collection.
 */
PluginCollection *
plugin_collection_new (void)
{
  PluginCollection * self =
    object_new (PluginCollection);

  self->name = g_strdup ("");

  return self;
}

/**
 * Clones a plugin collection.
 */
PluginCollection *
plugin_collection_clone (
  const PluginCollection * self)
{
  PluginCollection * clone = plugin_collection_new ();

  for (int i = 0; i < self->num_descriptors; i++)
    {
      clone->descriptors[clone->num_descriptors++] =
        plugin_descriptor_clone (self->descriptors[i]);
    }

  clone->name = g_strdup (self->name);
  if (self->description)
    {
      clone->description =
        g_strdup (self->description);
    }

  return clone;
}

char *
plugin_collection_get_name (
  PluginCollection * self)
{
  return self->name;
}

void
plugin_collection_set_name (
  PluginCollection * self,
  const char *       name)
{
  self->name = g_strdup (name);
}

/**
 * Appends a descriptor to the collection.
 */
void
plugin_collection_add_descriptor (
  PluginCollection *       self,
  const PluginDescriptor * descr)
{
  /* TODO check if exists */

  PluginDescriptor * new_descr =
    plugin_descriptor_clone (descr);
  if (descr->path)
    {
      GFile * file =
        g_file_new_for_path (descr->path);
      new_descr->ghash = g_file_hash (file);
      g_object_unref (file);
    }
  self->descriptors[self->num_descriptors++] =
    new_descr;
}

/**
 * Removes the descriptor matching the given one from
 * the collection.
 */
void
plugin_collection_remove_descriptor (
  PluginCollection *       self,
  const PluginDescriptor * descr)
{
  /* TODO */
}

/**
 * Removes all the descriptors.
 */
void
plugin_collection_clear (
  PluginCollection * self)
{
  for (int i = 0; i < self->num_descriptors; i++)
    {
      plugin_descriptor_free (self->descriptors[i]);
    }
  self->num_descriptors = 0;
}

void
plugin_collection_free (
  PluginCollection * self)
{
  for (int i = 0; i < self->num_descriptors; i++)
    {
      object_free_w_func_and_null (
        plugin_descriptor_free,
        self->descriptors[i]);
    }

  g_free_and_null (self->name);
  g_free_and_null (self->description);
}