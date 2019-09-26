/*
 * Copyright (C) 2019 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "fu-synaptics-rmi-common.h"
#include "fu-udev-device.h"

G_BEGIN_DECLS

#define FU_TYPE_SYNAPTICS_RMI_DEVICE (fu_synaptics_rmi_device_get_type ())
G_DECLARE_DERIVABLE_TYPE (FuSynapticsRmiDevice, fu_synaptics_rmi_device, FU, SYNAPTICS_RMI_DEVICE, FuUdevDevice)

struct _FuSynapticsRmiDeviceClass
{
	FuUdevDeviceClass	parent_class;
};

FuSynapticsRmiDevice	*fu_synaptics_rmi_device_new		(FuUdevDevice		*device);
gboolean		 fu_synaptics_rmi_device_write_bootloader_id	(FuSynapticsRmiDevice	*self,
								 GError			**error);
gboolean		 fu_synaptics_rmi_device_disable_irqs	(FuSynapticsRmiDevice	*self,
								 GError			**error);
GByteArray		*fu_synaptics_rmi_device_read		(FuSynapticsRmiDevice	*self,
								 guint16		 addr,
								 gsize			 req_sz,
								 GError			**error);
gboolean		 fu_synaptics_rmi_device_write		(FuSynapticsRmiDevice	*self,
								 guint16		 addr,
								 GByteArray		*req,
								 GError			**error);
gboolean		 fu_synaptics_rmi_device_reset		(FuSynapticsRmiDevice	*self,
								 GError			**error);
gboolean		 fu_synaptics_rmi_device_wait_for_idle	(FuSynapticsRmiDevice	*self,
								 guint			 timeout_ms,
								 GError			**error);
gboolean		 fu_synaptics_rmi_device_disable_sleep	(FuSynapticsRmiDevice	*self,
								 GError			**error);
guint8			 fu_synaptics_rmi_device_get_f34_status_addr	(FuSynapticsRmiDevice	*self);
guint16			 fu_synaptics_rmi_device_get_block_size	(FuSynapticsRmiDevice	*self);
const guint8		*fu_synaptics_rmi_device_get_bootloader_id	(FuSynapticsRmiDevice	*self);
FuSynapticsRmiFunction	*fu_synaptics_rmi_device_get_function	(FuSynapticsRmiDevice	*self,
								 guint8			 function_number,
								 GError			**error);
gboolean		 fu_synaptics_rmi_device_rebind_driver	(FuSynapticsRmiDevice	*self,
								 GError			**error);
gboolean
fu_synaptics_rmi_device_poll_wait (FuSynapticsRmiDevice *self, GError **error);

G_END_DECLS
