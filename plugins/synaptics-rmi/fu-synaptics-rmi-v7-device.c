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

#include "fu-common.h"
#include "fu-chunk.h"
#include "fu-synaptics-rmi-common.h"
#include "fu-synaptics-rmi-device.h"
#include "fu-synaptics-rmi-v7-device.h"

#include "fwupd-error.h"

gboolean
fu_synaptics_rmi_v7_device_detach (FuDevice *device, GError **error)
{
	FuSynapticsRmiDevice *self = FU_SYNAPTICS_RMI_DEVICE (device);
	g_autoptr(GByteArray) enable_req = g_byte_array_new ();
	const guint8 *bootloader_id = fu_synaptics_rmi_device_get_bootloader_id (self);
	FuSynapticsRmiFunction *f34;

	/* f34 */
	f34 = fu_synaptics_rmi_device_get_function (self, 0x34, error);
	if (f34 == NULL)
		return FALSE;

	/* disable interrupts */
	if (!fu_synaptics_rmi_device_disable_irqs (self, error))
		return FALSE;

	/* enter BL */
	fu_byte_array_append_uint8 (enable_req, BOOTLOADER_PARTITION);
	fu_byte_array_append_uint32 (enable_req, 0x0, G_LITTLE_ENDIAN);
	fu_byte_array_append_uint8 (enable_req, CMD_V7_ENTER_BL);
	fu_byte_array_append_uint8 (enable_req, bootloader_id[0]);
	fu_byte_array_append_uint8 (enable_req, bootloader_id[1]);
	if (!fu_synaptics_rmi_device_write (self,
					    f34->data_base + 1,
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
fu_synaptics_rmi_v7_device_erase_all (FuSynapticsRmiDevice *self, GError **error)
{
	FuSynapticsRmiFunction *f34;
	const guint8 *bootloader_id = fu_synaptics_rmi_device_get_bootloader_id (self);
	g_autoptr(GByteArray) erase_cmd = g_byte_array_new ();

	/* f34 */
	f34 = fu_synaptics_rmi_device_get_function (self, 0x34, error);
	if (f34 == NULL)
		return FALSE;

	fu_byte_array_append_uint8 (erase_cmd, CORE_CODE_PARTITION);
	fu_byte_array_append_uint32 (erase_cmd, 0x0, G_LITTLE_ENDIAN);
	if (bootloader_id[1] == 8) {
		/* For bootloader v8 */
		fu_byte_array_append_uint8 (erase_cmd, CMD_V7_ERASE_AP);
	} else {
		/* For bootloader v7 */
		fu_byte_array_append_uint8 (erase_cmd, CMD_V7_ERASE);
	}
	fu_byte_array_append_uint8 (erase_cmd, bootloader_id[0]);
	fu_byte_array_append_uint8 (erase_cmd, bootloader_id[1]);
	/* for BL8 device, we need hold 1 seconds after querying F34 status to
	 * avoid not get attention by following giving erase command */
	if (bootloader_id[1] == 8)
		g_usleep (1000 * 1000);
	if (!fu_synaptics_rmi_device_write (self,
					    f34->data_base + 1,
					    erase_cmd,
					    error)) {
		g_prefix_error (error, "failed to unlock erasing: ");
		return FALSE;
	}
	g_usleep (1000 * 100);
	if (bootloader_id[1] == 8){
		/* wait for ATTN */
		if (!fu_synaptics_rmi_device_wait_for_idle (self, RMI_F34_ERASE_V8_WAIT_MS, error)) {
			g_prefix_error (error, "failed to wait for idle: ");
			return FALSE;
		}
	}
	if (!fu_synaptics_rmi_device_poll_wait (self, error)) {
		g_prefix_error (error, "failed to get flash success: ");
		return FALSE;
	}

	/* for BL7, we need erase config partition */
	if (bootloader_id[1] == 7) {
		g_autoptr(GByteArray) erase_config_cmd = g_byte_array_new ();

		fu_byte_array_append_uint8 (erase_config_cmd, CORE_CONFIG_PARTITION);
		fu_byte_array_append_uint32 (erase_config_cmd, 0x0, G_LITTLE_ENDIAN);
		fu_byte_array_append_uint8 (erase_config_cmd, CMD_V7_ERASE);

		g_usleep (1000 * 100);
		if (!fu_synaptics_rmi_device_write (self,
						    f34->data_base + 1,
						    erase_config_cmd,
						    error)) {
			g_prefix_error (error, "failed to erase core config: ");
			return FALSE;
		}

		/* wait for ATTN */
		g_usleep (1000 * 100);
		if (!fu_synaptics_rmi_device_wait_for_idle (self, RMI_F34_ERASE_V8_WAIT_MS, error)) {
			g_prefix_error (error, "failed to wait for idle: ");
			return FALSE;
		}
		if (!fu_synaptics_rmi_device_poll_wait (self, error)) {
			g_prefix_error (error, "failed to get flash success: ");
			return FALSE;
		}
	}
	return TRUE;
}

static gboolean
fu_synaptics_rmi_device_write_block (FuSynapticsRmiDevice *self,
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
fu_synaptics_rmi_v7_device_write_firmware (FuDevice *device,
					   FuFirmware *firmware,
					   FwupdInstallFlags flags,
					   GError **error)
{
	FuSynapticsRmiDevice *self = FU_SYNAPTICS_RMI_DEVICE (device);
	FuSynapticsRmiFunction *f34;
	guint32 address = RMI_F34_BLOCK_DATA_V1_OFFSET;	//FIXME: is this right for v7???
	g_autoptr(GBytes) bytes_bin = NULL;
	g_autoptr(GBytes) bytes_cfg = NULL;
	g_autoptr(GBytes) bytes_flashcfg = NULL;
	g_autoptr(GPtrArray) chunks_bin = NULL;
	g_autoptr(GPtrArray) chunks_cfg = NULL;
	guint16 block_size = fu_synaptics_rmi_device_get_block_size (self);
	const guint8 *bootloader_id = fu_synaptics_rmi_device_get_bootloader_id (self);

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
	if (bootloader_id[1] == 8) {
		bytes_flashcfg = fu_firmware_get_image_by_id_bytes (firmware, "flash-config", error);
		if (bytes_flashcfg == NULL)
			return FALSE;
	}

	/* disable powersaving */
	if (!fu_synaptics_rmi_device_disable_sleep (self, error))
		return FALSE;

	/* erase all */
	if (!fu_synaptics_rmi_v7_device_erase_all (self, error)) {
		g_prefix_error (error, "failed to erase all: ");
		return FALSE;
	}

	/* write flash config for v8 */
	if (bytes_flashcfg != NULL) {
		g_autoptr(GByteArray) partition_id = g_byte_array_new ();
		g_autoptr(GByteArray) zero = g_byte_array_new ();
		g_autoptr(GPtrArray) chunks_flashcfg = NULL;

		/* write config id */
		fu_byte_array_append_uint16 (zero, 0x0, G_LITTLE_ENDIAN);
		fu_byte_array_append_uint8 (partition_id, FLASH_CONFIG_PARTITION);
		if (!fu_synaptics_rmi_device_write (self,
						    f34->data_base,
						    partition_id,
						    error)) {
			g_prefix_error (error, "failed to write flash config id v8: ");
			return FALSE;
		}
		if (!fu_synaptics_rmi_device_write (self,
						    f34->data_base + 2,
						    zero,
						    error)) {
			g_prefix_error (error, "failed to write initial zero: ");
			return FALSE;
		}

		/* write flash config */
		chunks_flashcfg = fu_chunk_array_new_from_bytes (bytes_flashcfg,
								 0x00,	/* start addr */
								 0x00,	/* page_sz */
								 block_size);
		for (guint i = 0; i < chunks_flashcfg->len; i++) {
			FuChunk *chk = g_ptr_array_index (chunks_flashcfg, i);
			if (!fu_synaptics_rmi_device_write_block (self,
								  RMI_F34_WRITE_FW_BLOCK,
								  address,
								  chk->data,
								  chk->data_sz,
								  error))
				return FALSE;
			fu_device_set_progress_full (device, (gsize) i,
						     (gsize) chunks_flashcfg->len + chunks_flashcfg->len);
		}
	}

	/* FIXME: write core code */
	chunks_bin = fu_chunk_array_new_from_bytes (bytes_bin,
						    0x00,	/* start addr */
						    0x00,	/* page_sz */
						    block_size);
	/* FIXME: write config */
	chunks_cfg = fu_chunk_array_new_from_bytes (bytes_cfg,
						    0x00,	/* start addr */
						    0x00,	/* page_sz */
						    block_size);
	return TRUE;
}
