/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details:
 *
 * Copyright (C) 2008 - 2009 Novell, Inc.
 * Copyright (C) 2009 - 2012 Red Hat, Inc.
 * Copyright (C) 2012 Lanedo GmbH
 * Copyright (C) 2015 Aleksander Morgado <aleksander@aleksander.es>
 */

#include <stdlib.h>
#include <gmodule.h>

#define _LIBMM_INSIDE_MM
#include <libmm-glib.h>

#include "mm-log.h"
#include "mm-plugin-sierra.h"
#include "mm-broadband-modem.h"

#if defined WITH_QMI
#include "mm-broadband-modem-qmi.h"
#endif

#if defined WITH_MBIM
#include "mm-broadband-modem-mbim.h"
#endif

G_DEFINE_TYPE (MMPluginSierra, mm_plugin_sierra, MM_TYPE_PLUGIN)

MM_PLUGIN_DEFINE_MAJOR_VERSION
MM_PLUGIN_DEFINE_MINOR_VERSION

/*****************************************************************************/

static MMBaseModem *
create_modem (MMPlugin *self,
              const gchar *uid,
              const gchar **drivers,
              guint16 vendor,
              guint16 product,
              GList *probes,
              GError **error)
{
#if defined WITH_QMI
    if (mm_port_probe_list_has_qmi_port (probes)) {
        mm_dbg ("QMI-powered Sierra modem found...");
        return MM_BASE_MODEM (mm_broadband_modem_qmi_new (uid,
                                                          drivers,
                                                          mm_plugin_get_name (self),
                                                          vendor,
                                                          product));
    }
#endif

#if defined WITH_MBIM
    if (mm_port_probe_list_has_mbim_port (probes)) {
        mm_dbg ("MBIM-powered Sierra modem found...");
        return MM_BASE_MODEM (mm_broadband_modem_mbim_new (uid,
                                                           drivers,
                                                           mm_plugin_get_name (self),
                                                           vendor,
                                                           product));
    }
#endif

    /* Fallback to default modem in the worst case */
    return MM_BASE_MODEM (mm_broadband_modem_new (uid,
                                                  drivers,
                                                  mm_plugin_get_name (self),
                                                  vendor,
                                                  product));
}

gboolean
grab_port (MMPlugin *self,
           MMBaseModem *modem,
           MMPortProbe *probe,
           GError **error)
{
    MMPortSerialAtFlag pflags = MM_PORT_SERIAL_AT_FLAG_NONE;
    GUdevDevice *port;
    const gchar *subsys;
    const gchar *name;

    port = mm_port_probe_peek_port (probe);
    subsys = mm_port_probe_get_port_subsys (probe);
    name = mm_port_probe_get_port_name (probe);

    if (g_strcmp0 (g_udev_device_get_subsystem (port), "net") == 0 &&
        g_strcmp0 (g_udev_device_get_property (port, "DEVTYPE"), "wwan") != 0 &&
        g_strcmp0 (mm_device_utils_get_port_driver (port), "qmi_wwan") != 0) {
        g_set_error (error,
                     MM_CORE_ERROR,
                     MM_CORE_ERROR_UNSUPPORTED,
                     "Cannot add port '%s/%s', not the QMI data interface",
                     subsys,
                     name);
        return FALSE;
    }

    return mm_base_modem_grab_port (modem,
                                    subsys,
                                    name,
                                    mm_port_probe_get_parent_path (probe),
                                    mm_port_probe_get_port_type (probe),
                                    pflags,
                                    error);
}

/*****************************************************************************/

G_MODULE_EXPORT MMPlugin *
mm_plugin_create (void)
{
    static const gchar *subsystems[] = { "tty", "net", "usb", NULL };
    static const guint16 vendor_ids[] = { 0x1199, 0 };
    static const gchar *drivers[] = { "qmi_wwan", "cdc_mbim", NULL };

    return MM_PLUGIN (
        g_object_new (MM_TYPE_PLUGIN_SIERRA,
                      MM_PLUGIN_NAME,               "Sierra",
                      MM_PLUGIN_ALLOWED_SUBSYSTEMS, subsystems,
                      MM_PLUGIN_ALLOWED_VENDOR_IDS, vendor_ids,
                      MM_PLUGIN_ALLOWED_DRIVERS,    drivers,
                      MM_PLUGIN_ALLOWED_AT,         TRUE,
                      MM_PLUGIN_ALLOWED_QCDM,       TRUE,
                      MM_PLUGIN_ALLOWED_QMI,        TRUE,
                      MM_PLUGIN_ALLOWED_MBIM,       TRUE,
                      NULL));
}

static void
mm_plugin_sierra_init (MMPluginSierra *self)
{
}

static void
mm_plugin_sierra_class_init (MMPluginSierraClass *klass)
{
    MMPluginClass *plugin_class = MM_PLUGIN_CLASS (klass);

    plugin_class->create_modem = create_modem;
    plugin_class->grab_port = grab_port;
}
