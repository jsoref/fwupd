/*
 * Copyright (C) 2021 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#define G_LOG_DOMAIN				"FuI2cDevice"

#include "config.h"

#include <glib/gstdio.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif

#include "fu-i2c-device.h"

/**
 * FuI2cDevice
 *
 * A I²C device with an assigned bus number.
 *
 * See also: #FuUdevDevice
 */

typedef struct
{
	FuUdevDevice		*udev_device;
	guint			 bus_number;
} FuI2cDevicePrivate;

G_DEFINE_TYPE_WITH_PRIVATE (FuI2cDevice, fu_i2c_device, FU_TYPE_UDEV_DEVICE)

enum {
	PROP_0,
	PROP_BUS_NUMBER,
	PROP_LAST
};

#define GET_PRIVATE(o) (fu_i2c_device_get_instance_private (o))

static void
fu_i2c_device_to_string (FuDevice *device, guint idt, GString *str)
{
	FuI2cDevice *self = FU_I2C_DEVICE (device);
	FuI2cDevicePrivate *priv = GET_PRIVATE (self);
	fu_common_string_append_kx (str, idt, "BusNumber", priv->bus_number);
}

static void
fu_i2c_device_get_property (GObject *object, guint prop_id,
			    GValue *value, GParamSpec *pspec)
{
	FuI2cDevice *self = FU_I2C_DEVICE (object);
	FuI2cDevicePrivate *priv = GET_PRIVATE (self);
	switch (prop_id) {
	case PROP_BUS_NUMBER:
		g_value_set_uint (value, priv->bus_number);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
fu_i2c_device_set_property (GObject *object, guint prop_id,
			    const GValue *value, GParamSpec *pspec)
{
	FuI2cDevice *self = FU_I2C_DEVICE (object);
	FuI2cDevicePrivate *priv = GET_PRIVATE (self);
	switch (prop_id) {
	case PROP_BUS_NUMBER:
		priv->bus_number = g_value_get_uint (value);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static gboolean
fu_i2c_device_open (FuDevice *device, GError **error)
{
#ifdef HAVE_GUDEV
	FuI2cDevice *self = FU_I2C_DEVICE (device);
	FuI2cDevicePrivate *priv = GET_PRIVATE (self);
	gint bus_fd;
	g_autofree gchar *bus_path = NULL;

	/* open the bus, not the device represented by self */
	bus_path = g_strdup_printf ("/dev/i2c-%u", priv->bus_number);
	if ((bus_fd = g_open (bus_path, O_RDWR, 0)) == -1) {
		g_set_error (error,
			     G_IO_ERROR,
#ifdef HAVE_ERRNO_H
			     g_io_error_from_errno (errno),
#else
			     G_IO_ERROR_FAILED,
#endif
			     "failed to open %s read-write",
			     bus_path);
		return FALSE;
	}
	fu_udev_device_set_fd (FU_UDEV_DEVICE (self), bus_fd);
	fu_udev_device_set_flags (FU_UDEV_DEVICE (self), FU_UDEV_DEVICE_FLAG_NONE);
#endif

	/* FuUdevDevice->open */
	return FU_DEVICE_CLASS (fu_i2c_device_parent_class)->open (device, error);
}

static gboolean
fu_i2c_device_probe (FuDevice *device, GError **error)
{
#ifdef HAVE_GUDEV
	FuI2cDevice *self = FU_I2C_DEVICE (device);
	FuI2cDevicePrivate *priv = GET_PRIVATE (self);
	GUdevDevice *udev_device = fu_udev_device_get_dev (FU_UDEV_DEVICE (device));
	const gchar *tmp;
	g_autoptr(GMatchInfo) info = NULL;
	g_autoptr(GRegex) regex = NULL;
#endif

	/* FuUdevDevice->probe */
	if (!FU_DEVICE_CLASS (fu_i2c_device_parent_class)->probe (device, error))
		return FALSE;

#ifdef HAVE_GUDEV
	/* i2c devices all expose a name */
	tmp = g_udev_device_get_sysfs_attr (udev_device, "name");
	if (tmp != NULL) {
		g_autofree gchar *devid = NULL;
		g_autofree gchar *name_safe = g_strdup (tmp);
		g_strdelimit (name_safe, " /\\\"", '-');
		devid = g_strdup_printf ("I2C\\NAME_%s", name_safe);
		fu_device_add_instance_id (FU_DEVICE (self), devid);
	}

	/* get bus number out of sysfs path */
	regex = g_regex_new ("/i2c-([0-9]+)/", 0, 0, error);
	if (regex == NULL)
		return FALSE;
	tmp = g_udev_device_get_sysfs_path (udev_device);
	if (!g_regex_match_full (regex, tmp, -1, 0, 0, &info, error))
		return FALSE;
	priv->bus_number = g_ascii_strtoll (g_match_info_fetch (info, 1),
					    NULL, 10);
#endif

	/* success */
	return TRUE;
}

/**
 * fu_i2c_device_get_bus_number:
 * @self: a #FuI2cDevice
 *
 * Gets the I²C bus number.
 *
 * Returns: integer
 *
 * Since: 1.6.1
 **/
guint
fu_i2c_device_get_bus_number (FuI2cDevice *self)
{
	FuI2cDevicePrivate *priv = GET_PRIVATE (self);
	g_return_val_if_fail (FU_IS_I2C_DEVICE (self), G_MAXUINT);
	return priv->bus_number;
}

/**
 * fu_i2c_device_set_bus_number:
 * @self: a #FuI2cDevice
 * @bus_number: integer, typically the output of g_udev_device_get_number()
 *
 * Sets the I²C bus number.
 *
 * Since: 1.6.2
 **/
void
fu_i2c_device_set_bus_number (FuI2cDevice *self, guint bus_number)
{
	FuI2cDevicePrivate *priv = GET_PRIVATE (self);
	g_return_if_fail (FU_IS_I2C_DEVICE (self));
	priv->bus_number = bus_number;
}

/**
 * fu_i2c_device_write:
 * @self: a #FuI2cDevice
 * @data: value
 * @error: (nullable): optional return location for an error
 *
 * Write a single byte to the I²C device.
 *
 * Returns: %TRUE for success
 *
 * Since: 1.6.1
 **/
gboolean
fu_i2c_device_write (FuI2cDevice *self, guint8 data, GError **error)
{
	return fu_udev_device_pwrite_full (FU_UDEV_DEVICE (self), 0x0, &data, 0x01, error);
}

/**
 * fu_i2c_device_read:
 * @self: a #FuI2cDevice
 * @data: (out): value
 * @error: (nullable): optional return location for an error
 *
 * Read a single byte from the I²C device.
 *
 * Returns: %TRUE for success
 *
 * Since: 1.6.1
 **/
gboolean
fu_i2c_device_read (FuI2cDevice *self, guint8 *data, GError **error)
{
	return fu_udev_device_pread_full (FU_UDEV_DEVICE (self), 0x0, data, 0x1, error);
}

static void
fu_i2c_device_init (FuI2cDevice *self)
{
}

static void
fu_i2c_device_class_init (FuI2cDeviceClass *klass)
{
	FuDeviceClass *klass_device = FU_DEVICE_CLASS (klass);
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GParamSpec *pspec;

	object_class->get_property = fu_i2c_device_get_property;
	object_class->set_property = fu_i2c_device_set_property;
	klass_device->open = fu_i2c_device_open;
	klass_device->probe = fu_i2c_device_probe;
	klass_device->to_string = fu_i2c_device_to_string;

	pspec = g_param_spec_uint ("bus-number", NULL, NULL,
				   0x0, G_MAXUINT, 0x0,
				   G_PARAM_READWRITE |
				   G_PARAM_STATIC_NAME);
	g_object_class_install_property (object_class, PROP_BUS_NUMBER, pspec);
}
