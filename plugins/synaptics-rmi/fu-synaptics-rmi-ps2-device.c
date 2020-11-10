/*
 * Copyright (C) 2012-2014 Andrew Duggan
 * Copyright (C) 2012-2019 Synaptics Inc.
 * Copyright (C) 2019 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2020 Synaptics Inc.
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <sys/ioctl.h>

#include "fu-io-channel.h"

#include "fu-synaptics-rmi-common.h"
#include "fu-synaptics-rmi-firmware.h"
#include "fu-synaptics-rmi-v5-device.h"
#include "fu-synaptics-rmi-v6-device.h"
#include "fu-synaptics-rmi-v7-device.h"

enum EPS2DataPortCommand {
	edpAuxFullRMIBackDoor         = 0x7F,
	edpAuxAccessModeByte1         = 0xE0,
	edpAuxAccessModeByte2         = 0xE1,
	edpAuxSetScaling1To1          = 0xE6,
	edpAuxSetScaling2To1          = 0xE7,
	edpAuxSetResolution           = 0xE8,
	edpAuxStatusRequest           = 0xE9,
	edpAuxSetStreamMode           = 0xEA,
	edpAuxReadData                = 0xEB,
	edpAuxResetWrapMode           = 0xEC,
	edpAuxSetWrapMode             = 0xEE,
	edpAuxSetRemoteMode           = 0xF0,
	edpAuxReadDeviceType          = 0xF2,
	edpAuxSetSampleRate           = 0xF3,
	edpAuxEnable                  = 0xF4,
	edpAuxDisable                 = 0xF5,
	edpAuxSetDefault              = 0xF6,
	edpAuxResend                  = 0xFE,
	edpAuxReset                   = 0xFF,
};

enum ESynapticsDeviceResponse {
	esdrTouchPad = 0x47,
	esdrStyk = 0x46,
	esdrControlBar = 0x44,
	esdrRGBControlBar = 0x43,
};

enum EStatusRequestSequence {
	esrIdentifySynaptics          = 0x00,
	esrReadTouchPadModes          = 0x01,
	esrReadModeByte               = 0x01,
	esrReadEdgeMargins            = 0x02,
	esrReadCapabilities           = 0x02,
	esrReadModelID                = 0x03,
	esrReadCompilationDate        = 0x04,
	esrReadSerialNumberPrefix     = 0x06,
	esrReadSerialNumberSuffix     = 0x07,
	esrReadResolutions            = 0x08,
	esrReadExtraCapabilities1     = 0x09,
	esrReadExtraCapabilities2     = 0x0A,
	esrReadExtraCapabilities3     = 0x0B,
	esrReadExtraCapabilities4     = 0x0C,
	esrReadExtraCapabilities5     = 0x0D,
	esrReadCoordinates            = 0x0D,
	esrReadExtraCapabilities6     = 0x0E,
	esrReadExtraCapabilities7     = 0x0F,
};

enum EPS2DataPortStatus {
	edpsAcknowledge               = 0xFA,
	edpsError                     = 0xFC,
	edpsResend                    = 0xFE,
	edpsTimeOut                   = 0x100
};

enum ESetSampleRateSequence {
	essrSetModeByte1              = 0x0A,
	essrSetModeByte2              = 0x14,
	essrSetModeByte3              = 0x28,
	essrSetModeByte4              = 0x3C,
	essrSetDeluxeModeByte1        = 0x0A,
	essrSetDeluxeModeByte2        = 0x3C,
	essrSetDeluxeModeByte3        = 0xC8,
	essrFastRecalibrate           = 0x50,
	essrPassThroughCommandTunnel  = 0x28
};

enum EDeviceType {
	edtUnknown,
	edtTouchPad,
};

static gboolean
fu_synaptics_rmi_ps2_device_probe (FuUdevDevice *device, GError **error)
{
    /* check is valid */
	if (g_strcmp0 (fu_udev_device_get_subsystem (device), "serio_raw") != 0) {
		g_set_error (error,
			     FWUPD_ERROR,
			     FWUPD_ERROR_NOT_SUPPORTED,
			     "is not correct subsystem=%s, expected serio_raw",
			     fu_udev_device_get_subsystem (device));
		return FALSE;
	}
	if (fu_udev_device_get_device_file (device) == NULL) {
		g_set_error_literal (error,
				     FWUPD_ERROR,
				     FWUPD_ERROR_NOT_SUPPORTED,
				     "no device file");
		return FALSE;
	}

	return fu_udev_device_set_physical_id (device, "serio", error);
}

static void
fu_synaptics_rmi_ps2_device_class_init (FuSynapticsRmiPs2DeviceClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	FuDeviceClass *klass_device = FU_DEVICE_CLASS (klass);
	FuUdevDeviceClass *klass_device_udev = FU_UDEV_DEVICE_CLASS (klass);
//	object_class->finalize = fu_synaptics_rmi_device_finalize;
//	klass_device_udev->to_string = fu_synaptics_rmi_device_to_string;
//	klass_device->prepare_firmware = fu_synaptics_rmi_device_prepare_firmware;
//	klass_device->attach = fu_synaptics_rmi_device_attach;
//	klass_device->setup = fu_synaptics_rmi_device_setup;
	klass_device_udev->probe = fu_synaptics_rmi_ps2_device_probe;
//	klass_device_udev->open = fu_synaptics_rmi_ps2_device_open;
//	klass_device_udev->close = fu_synaptics_rmi_ps2_device_close;
}
