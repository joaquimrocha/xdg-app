/*
 * Copyright © 2014 Red Hat, Inc
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authors:
 *       Alexander Larsson <alexl@redhat.com>
 */

#include "config.h"

#include <locale.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "libgsystem.h"
#include "libglnx/libglnx.h"

#include "xdg-app-builtins.h"
#include "xdg-app-utils.h"

static gboolean opt_show_details;
static gboolean opt_user;
static gboolean opt_system;

static GOptionEntry options[] = {
  { "user", 0, 0, G_OPTION_ARG_NONE, &opt_user, "Show user installations", NULL },
  { "system", 0, 0, G_OPTION_ARG_NONE, &opt_system, "Show system-wide installations", NULL },
  { "show-details", 'd', 0, G_OPTION_ARG_NONE, &opt_show_details, "Show remote details", NULL },
  { NULL }
};

gboolean
xdg_app_builtin_list_remotes (int argc, char **argv, GCancellable *cancellable, GError **error)
{
  g_autoptr(GOptionContext) context = NULL;
  g_autoptr(XdgAppDir) user_dir = NULL;
  g_autoptr(XdgAppDir) system_dir = NULL;
  XdgAppDir *dirs[2] = { 0 };
  guint i = 0, n_dirs = 0, j;
  XdgAppTablePrinter *printer;

  context = g_option_context_new (" - List remote repositories");

  if (!xdg_app_option_context_parse (context, options, &argc, &argv, XDG_APP_BUILTIN_FLAG_NO_DIR, NULL, cancellable, error))
    return FALSE;

  if (!opt_user && !opt_system)
    {
      opt_system = TRUE;
    }

  if (opt_user)
    {
      user_dir = xdg_app_dir_get_user ();
      dirs[n_dirs++] = user_dir;
    }

  if (opt_system)
    {
      system_dir = xdg_app_dir_get_system ();
      dirs[n_dirs++] = system_dir;
    }

  printer = xdg_app_table_printer_new ();

  for (j = 0; j < n_dirs; j++)
    {
      XdgAppDir *dir = dirs[j];
      g_auto(GStrv) remotes = NULL;

      remotes = xdg_app_dir_list_remotes (dir, cancellable, error);
      if (remotes == NULL)
        return FALSE;

      for (i = 0; remotes[i] != NULL; i++)
        {
          char *remote_name = remotes[i];

          if (opt_show_details)
            {
              g_autofree char *remote_url = NULL;
              g_autofree char *title = NULL;
              int prio;
              g_autofree char *prio_as_string = NULL;
              gboolean gpg_verify = TRUE;

              xdg_app_table_printer_add_column (printer, remote_name);

              title = xdg_app_dir_get_remote_title (dir, remote_name);
              if (title)
                xdg_app_table_printer_add_column (printer, title);
              else
                xdg_app_table_printer_add_column (printer, "-");

              ostree_repo_remote_get_url (xdg_app_dir_get_repo (dir), remote_name, &remote_url, NULL);

              xdg_app_table_printer_add_column (printer, remote_url);

              prio = xdg_app_dir_get_remote_prio (dir, remote_name);
              prio_as_string = g_strdup_printf ("%d", prio);
              xdg_app_table_printer_add_column (printer, prio_as_string);

              xdg_app_table_printer_add_column (printer, ""); /* Options */

              ostree_repo_remote_get_gpg_verify (xdg_app_dir_get_repo (dir), remote_name,
                                                 &gpg_verify, NULL);
              if (!gpg_verify)
                xdg_app_table_printer_append_with_comma (printer, "no-gpg-verify");

              if (xdg_app_dir_get_remote_noenumerate (dir, remote_name))
                xdg_app_table_printer_append_with_comma (printer, "no-enumerate");

              if (opt_user && opt_system)
                xdg_app_table_printer_append_with_comma (printer, dir == user_dir ? "user" : "system");
            }
          else
            xdg_app_table_printer_add_column (printer, remote_name);

          xdg_app_table_printer_finish_row (printer);
        }
    }

  xdg_app_table_printer_print (printer);
  xdg_app_table_printer_free (printer);

  return TRUE;
}
