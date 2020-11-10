/*
 * Copyright (C) 2019 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2020 Synaptics Inc
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#pragma once

#include "fu-synaptics-rmi-common.h"
#include "fu-udev-device.h"

#define FU_TYPE_SYNAPTICS_RMI_PS2_DEVICE (fu_synaptics_rmi_ps2_device_get_type ())
G_DECLARE_DERIVABLE_TYPE (FuSynapticsRmiPs2Device, fu_synaptics_rmi_ps2_device, FU, SYNAPTICS_RMI_PS2_DEVICE, FuUdevDevice)

struct _FuSynapticsRmiPs2DeviceClass
{
	FuUdevDeviceClass	parent_class;
	gboolean		 (*setup)			(FuSynapticsRmiPs2Device	*self,
								 GError			**error);
	gboolean		 (*query_status)		(FuSynapticsRmiPs2Device	*self,
								 GError			**error);
};