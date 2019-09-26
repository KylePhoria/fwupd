/*
 * Copyright (C) 2012-2014 Andrew Duggan
 * Copyright (C) 2012-2014 Synaptics Inc.
 * Copyright (C) 2019 Richard Hughes <richard@hughsie.com>
 *
 * SPDX-License-Identifier: LGPL-2.1+
 */

#include "config.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <gio/gio.h>

#include "fu-chunk.h"
#include "fu-common.h"
#include "fu-synaptics-rmi-common.h"
#include "fu-synaptics-rmi-device.h"
#include "fu-synaptics-rmi-v5-device.h"

#include "fwupd-error.h"

gboolean
fu_synaptics_rmi_v5_device_detach (FuDevice *device, GError **error)
{
	FuSynapticsRmiDevice *self = FU_SYNAPTICS_RMI_DEVICE (device);
	g_autoptr(GByteArray) enable_req = g_byte_array_new ();

	/* disable interrupts */
	if (!fu_synaptics_rmi_device_disable_irqs (self, error))
		return FALSE;

	/* unlock bootloader and rebind kernel driver */
	if (!fu_synaptics_rmi_device_write_bootloader_id (self, error))
		return FALSE;
	fu_byte_array_append_uint8 (enable_req, RMI_F34_ENABLE_FLASH_PROG);
	if (!fu_synaptics_rmi_device_write (self,
					    fu_synaptics_rmi_device_get_f34_status_addr (self),
					    enable_req,
					    error)) {
		g_prefix_error (error, "failed to enable programming: ");
		return FALSE;
	}

	fu_device_set_status (device, FWUPD_STATUS_DEVICE_RESTART);
	g_usleep (1000 * RMI_F34_ENABLE_WAIT_MS);
	return fu_synaptics_rmi_device_rebind_driver (self, error);
}

static gboolean
fu_synaptics_rmi_v5_device_erase_all (FuSynapticsRmiDevice *self, GError **error)
{
	FuSynapticsRmiFunction *f34;
	g_autoptr(GByteArray) erase_cmd = g_byte_array_new ();

	/* f34 */
	f34 = fu_synaptics_rmi_device_get_function (self, 0x34, error);
	if (f34 == NULL)
		return FALSE;

	/* all other versions */
	fu_byte_array_append_uint8 (erase_cmd, RMI_F34_ERASE_ALL);
	if (!fu_synaptics_rmi_device_write (self,
					    fu_synaptics_rmi_device_get_f34_status_addr (self),
					    erase_cmd,
					    error)) {
		g_prefix_error (error, "failed to erase core config: ");
		return FALSE;
	}
	g_usleep (1000 * RMI_F34_ENABLE_WAIT_MS);
	return TRUE;
}

static gboolean
fu_synaptics_rmi_v5_device_write_block (FuSynapticsRmiDevice *self,
					guint8 cmd,
					guint32 address,
					const guint8 *data,
					gsize datasz,
					GError **error)
{
	g_autoptr(GByteArray) req = g_byte_array_new ();

	g_byte_array_append (req, data, datasz);
	fu_byte_array_append_uint8 (req, cmd);
	if (!fu_synaptics_rmi_device_write (self, address, req, error)) {
		g_prefix_error (error, "failed to write block @0x%x: ", address);
		return FALSE;
	}
	if (!fu_synaptics_rmi_device_wait_for_idle (self, RMI_F34_IDLE_WAIT_MS, error)) {
		g_prefix_error (error, "failed to wait for idle @0x%x: ", address);
		return FALSE;
	}
	return TRUE;
}

gboolean
fu_synaptics_rmi_v5_device_write_firmware (FuDevice *device,
					   FuFirmware *firmware,
					   FwupdInstallFlags flags,
					   GError **error)
{

	FuSynapticsRmiDevice *self = FU_SYNAPTICS_RMI_DEVICE (device);
	FuSynapticsRmiFunction *f34;
	guint32 address;
	guint16 block_size = fu_synaptics_rmi_device_get_block_size (self);
	g_autoptr(GBytes) bytes_bin = NULL;
	g_autoptr(GBytes) bytes_cfg = NULL;
	g_autoptr(GPtrArray) chunks_bin = NULL;
	g_autoptr(GPtrArray) chunks_cfg = NULL;
	g_autoptr(GByteArray) zero = g_byte_array_new ();

	/* f34 */
	f34 = fu_synaptics_rmi_device_get_function (self, 0x34, error);
	if (f34 == NULL)
		return FALSE;

	/* get both images */
	bytes_bin = fu_firmware_get_image_by_id_bytes (firmware, "ui", error);
	if (bytes_bin == NULL)
		return FALSE;
	bytes_cfg = fu_firmware_get_image_by_id_bytes (firmware, "config", error);
	if (bytes_cfg == NULL)
		return FALSE;

	/* disable powersaving */
	if (!fu_synaptics_rmi_device_disable_sleep (self, error))
		return FALSE;

	/* erase all */
	if (!fu_synaptics_rmi_v5_device_erase_all (self, error)) {
		g_prefix_error (error, "failed to erase all: ");
		return FALSE;
	}

	/* write initial zero */
	fu_byte_array_append_uint16 (zero, 0x0, G_LITTLE_ENDIAN);
	if (f34->function_version == 0x01)
		address = f34->data_base + RMI_F34_BLOCK_DATA_V1_OFFSET;
	else
		address = f34->data_base + RMI_F34_BLOCK_DATA_OFFSET;
	if (!fu_synaptics_rmi_device_write (self,
					    address,
					    zero,
					    error)) {
		g_prefix_error (error, "failed to write initial zero: ");
		return FALSE;
	}

	/* write each block */
	chunks_bin = fu_chunk_array_new_from_bytes (bytes_bin,
						    0x00,	/* start addr */
						    0x00,	/* page_sz */
						    block_size);
	fu_device_set_status (device, FWUPD_STATUS_DEVICE_WRITE);
	for (guint i = 0; i < chunks_bin->len; i++) {
		FuChunk *chk = g_ptr_array_index (chunks_bin, i);
		if (!fu_synaptics_rmi_v5_device_write_block (self,
							     RMI_F34_WRITE_FW_BLOCK,
							     address,
							     chk->data,
							     chk->data_sz,
							     error))
			return FALSE;
		fu_device_set_progress_full (device, (gsize) i,
					     (gsize) chunks_bin->len + chunks_cfg->len);
	}

	/* program the configuration image */
	chunks_cfg = fu_chunk_array_new_from_bytes (bytes_cfg,
						    0x00,	/* start addr */
						    0x00,	/* page_sz */
						    block_size);
	for (guint i = 0; i < chunks_cfg->len; i++) {
		FuChunk *chk = g_ptr_array_index (chunks_cfg, i);
		if (!fu_synaptics_rmi_v5_device_write_block (self,
							     RMI_F34_WRITE_CONFIG_BLOCK,
							     address,
							     chk->data,
							     chk->data_sz,
							     error))
			return FALSE;
		fu_device_set_progress_full (device,
					     (gsize) chunks_cfg->len + i,
					     (gsize) chunks_cfg->len + chunks_cfg->len);
	}

	/* success */
	return TRUE;
}
