/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*-
 * 
 * Copyright (C) 2011 Colin Walters <walters@verbum.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General
 * Public License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place, Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include "xdg-app-chain-input-stream.h"

enum {
  PROP_0,
  PROP_STREAMS
};

G_DEFINE_TYPE (XdgAppChainInputStream, xdg_app_chain_input_stream, G_TYPE_INPUT_STREAM)

struct _XdgAppChainInputStreamPrivate {
  GPtrArray *streams;
  guint index;
};

static void     xdg_app_chain_input_stream_set_property (GObject              *object,
                                                           guint                 prop_id,
                                                           const GValue         *value,
                                                           GParamSpec           *pspec);
static void     xdg_app_chain_input_stream_get_property (GObject              *object,
                                                           guint                 prop_id,
                                                           GValue               *value,
                                                           GParamSpec           *pspec);
static void     xdg_app_chain_input_stream_finalize     (GObject *object);
static gssize   xdg_app_chain_input_stream_read         (GInputStream         *stream,
                                                        void                 *buffer,
                                                        gsize                 count,
                                                        GCancellable         *cancellable,
                                                        GError              **error);
static gboolean xdg_app_chain_input_stream_close        (GInputStream         *stream,
                                                        GCancellable         *cancellable,
                                                        GError              **error);

static void
xdg_app_chain_input_stream_class_init (XdgAppChainInputStreamClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GInputStreamClass *stream_class = G_INPUT_STREAM_CLASS (klass);
  
  g_type_class_add_private (klass, sizeof (XdgAppChainInputStreamPrivate));

  gobject_class->get_property = xdg_app_chain_input_stream_get_property;
  gobject_class->set_property = xdg_app_chain_input_stream_set_property;
  gobject_class->finalize     = xdg_app_chain_input_stream_finalize;

  stream_class->read_fn = xdg_app_chain_input_stream_read;
  stream_class->close_fn = xdg_app_chain_input_stream_close;

  /*
   * XdgAppChainInputStream:streams: (element-type GInputStream)
   *
   * Chain of input streams read in order.
   */
  g_object_class_install_property (gobject_class,
				   PROP_STREAMS,
				   g_param_spec_pointer ("streams",
							 "", "",
							 G_PARAM_READWRITE |
							 G_PARAM_CONSTRUCT_ONLY |
							 G_PARAM_STATIC_STRINGS));

}

static void
xdg_app_chain_input_stream_set_property (GObject         *object,
					     guint            prop_id,
					     const GValue    *value,
					     GParamSpec      *pspec)
{
  XdgAppChainInputStream *self;
  
  self = XDG_APP_CHAIN_INPUT_STREAM (object);

  switch (prop_id)
    {
    case PROP_STREAMS:
      self->priv->streams = g_ptr_array_ref (g_value_get_pointer (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
xdg_app_chain_input_stream_get_property (GObject    *object,
					     guint       prop_id,
					     GValue     *value,
					     GParamSpec *pspec)
{
  XdgAppChainInputStream *self;

  self = XDG_APP_CHAIN_INPUT_STREAM (object);

  switch (prop_id)
    {
    case PROP_STREAMS:
      g_value_set_pointer (value, self->priv->streams);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}

static void
xdg_app_chain_input_stream_finalize (GObject *object)
{
  XdgAppChainInputStream *stream;

  stream = (XdgAppChainInputStream*)(object);

  g_ptr_array_unref (stream->priv->streams);

  G_OBJECT_CLASS (xdg_app_chain_input_stream_parent_class)->finalize (object);
}

static void
xdg_app_chain_input_stream_init (XdgAppChainInputStream *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self,
					    XDG_APP_TYPE_CHAIN_INPUT_STREAM,
					    XdgAppChainInputStreamPrivate);

}

XdgAppChainInputStream *
xdg_app_chain_input_stream_new (GPtrArray   *streams)
{
  XdgAppChainInputStream *stream;

  stream = g_object_new (XDG_APP_TYPE_CHAIN_INPUT_STREAM,
			 "streams", streams,
			 NULL);

  return (XdgAppChainInputStream*) (stream);
}

static gssize
xdg_app_chain_input_stream_read (GInputStream  *stream,
                                void          *buffer,
                                gsize          count,
                                GCancellable  *cancellable,
                                GError       **error)
{
  XdgAppChainInputStream *self = (XdgAppChainInputStream*) stream;
  GInputStream *child;
  gssize res = -1;

  if (g_cancellable_set_error_if_cancelled (cancellable, error))
    return -1;
  
  if (self->priv->index >= self->priv->streams->len)
    return 0;

  res = 0;
  while (res == 0 && self->priv->index < self->priv->streams->len)
    {
      child = self->priv->streams->pdata[self->priv->index];
      res = g_input_stream_read (child,
                                 buffer,
                                 count,
                                 cancellable,
                                 error);
      if (res == 0)
        self->priv->index++;
    }

  return res;
}

static gboolean
xdg_app_chain_input_stream_close (GInputStream         *stream,
                                 GCancellable         *cancellable,
                                 GError              **error)
{
  gboolean ret = FALSE;
  XdgAppChainInputStream *self = (gpointer)stream;
  guint i;

  for (i = 0; i < self->priv->streams->len; i++)
    {
      GInputStream *child = self->priv->streams->pdata[i];
      if (!g_input_stream_close (child, cancellable, error))
        goto out;
    }

  ret = TRUE;
 out:
  return ret;
}
