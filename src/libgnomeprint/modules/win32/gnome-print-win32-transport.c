/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  gnome-print-win32-transport.c: WIN32 transport destination
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public License
 *  as published by the Free Software Foundation; either version 2 of
 *  the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public
 *  License along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Authors:
 *    Ivan, Wong Yat Cheung <email@ivanwong.info>
 *
 *  Since GDI already handles the transport stuffs very well. This
 *  transport class is dummy and doesn't nothing. Even if we want
 *  to print to file, StartDoc() allows us to specify an so called
 *  output file.
 *
 *  Copyright (C) 2005 Novell Inc. and authors
 *
 */

#include <config.h>
#include <gmodule.h>
#include <libgnomeprint/gnome-print.h>
#include <libgnomeprint/gnome-print-transport.h>
#include <libgnomeprint/gnome-print-config.h>

#define GP_TYPE_TRANSPORT_WIN32         (gp_transport_win32_get_type ())
#define GP_TRANSPORT_WIN32(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GP_TYPE_TRANSPORT_WIN32, GPTransportWin32))
#define GP_TRANSPORT_WIN32_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GP_TYPE_TRANSPORT_WIN32, GPTransportWin32Class))
#define GP_IS_TRANSPORT_WIN32(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GP_TYPE_TRANSPORT_WIN32))
#define GP_IS_TRANSPORT_WIN32_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GP_TYPE_TRANSPORT_WIN32))
#define GP_TRANSPORT_WIN32_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GP_TYPE_TRANSPORT_WIN32, GPTransportWin32Class))

typedef struct _GPTransportWin32 GPTransportWin32;
typedef struct _GPTransportWin32Class GPTransportWin32Class;

struct _GPTransportWin32 {
	GnomePrintTransport transport;
};

struct _GPTransportWin32Class {
	GnomePrintTransportClass parent_class;
};

static void gp_transport_win32_class_init (GPTransportWin32Class *klass);
static void gp_transport_win32_init (GPTransportWin32 *transport);
static void gp_transport_win32_finalize (GObject *object);
static gint gp_transport_win32_construct (GnomePrintTransport *transport);
static gint gp_transport_win32_open (GnomePrintTransport *transport);
static gint gp_transport_win32_close (GnomePrintTransport *transport);
static gint gp_transport_win32_write (GnomePrintTransport *transport, const guchar *buf, gint len);
static gint gp_transport_win32_print_file (GnomePrintTransport *transport, const guchar *filename);

GType gp_transport_win32_get_type (void);

static GnomePrintTransportClass *parent_class = NULL;

GType
gp_transport_win32_get_type (void)
{
	static GType type = 0;
	if (!type) {
		static const GTypeInfo info = {
			sizeof (GPTransportWin32Class),
			NULL, NULL,
			(GClassInitFunc) gp_transport_win32_class_init,
			NULL, NULL,
			sizeof (GPTransportWin32),
			0,
			(GInstanceInitFunc) gp_transport_win32_init
		};
		type = g_type_register_static (GNOME_TYPE_PRINT_TRANSPORT, "GPTransportWin32", &info, 0);
	}
	return type;
}

static void
gp_transport_win32_class_init (GPTransportWin32Class *klass)
{
	GnomePrintTransportClass *transport_class;
	GObjectClass *object_class;

	object_class                = (GObjectClass *) klass;
	transport_class             = (GnomePrintTransportClass *) klass;

	parent_class                = g_type_class_peek_parent (klass);

	object_class->finalize      = gp_transport_win32_finalize;

	transport_class->construct  = gp_transport_win32_construct;
	transport_class->open       = gp_transport_win32_open;
	transport_class->close      = gp_transport_win32_close;
	transport_class->write      = gp_transport_win32_write;

	transport_class->print_file = gp_transport_win32_print_file;
}

static void
gp_transport_win32_init (GPTransportWin32 *transport)
{
}

static void
gp_transport_win32_finalize (GObject *object)
{
	G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gint
gp_transport_win32_construct (GnomePrintTransport *gp_transport)
{
	return GNOME_PRINT_OK;
}

static gint
gp_transport_win32_open (GnomePrintTransport *gp_transport)
{
	return GNOME_PRINT_OK;
}

static gint
gp_transport_win32_close (GnomePrintTransport *gp_transport)
{
	return GNOME_PRINT_OK;
}

static gint
gp_transport_win32_write (GnomePrintTransport *gp_transport, const guchar *buf, gint len)
{
	return len;
}


static gint
gp_transport_win32_print_file (GnomePrintTransport *gp_transport, const guchar *filename)
{
	return GNOME_PRINT_OK;
}

G_MODULE_EXPORT GType
gnome_print__transport_get_type (void)
{
	return GP_TYPE_TRANSPORT_WIN32;
}
