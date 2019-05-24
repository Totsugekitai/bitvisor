/*
 * Copyright (c) 2007, 2008 University of Tsukuba
 * Copyright (c) 2015 Igel Co., Ltd
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 * 3. Neither the name of the University of Tsukuba nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <core.h>
#include <core/ap.h>
#include <core/initfunc.h>
#include <core/list.h>
#include <core/mmio.h>
#include <net/netapi.h>
#include "pci.h"
#include "virtio_net.h"

/* virtio-net virtual driver

   Provides a virtual virtio-net device.  MSI-X is required.  The
   "net" parameter is similar to para pass-through network drivers
   except that a guest operating system is connected to a *physical*
   side of the netapi.  The following example creates a virtual
   virtio-net device which is connected to a TCP/IP stack:

   vmm.driver.pci_virtual=driver=virtio-net, net=ip
 */

struct data {
	struct netdata *nethandle;
	void *virtio_net;
	void *mmio_handle;
	bool mmio_registered;
	bool membase_emul;
	u32 membase;
	u8 macaddr[6];
};

static void
getinfo_virtnic (void *handle, struct nicinfo *info)
{
	struct data *d = handle;

	info->mtu = 1500;
	info->media_speed = 1000000000;
	memcpy (info->mac_address, d->macaddr, sizeof d->macaddr);
}

static void
send_virtnic (void *handle, unsigned int num_packets, void **packets,
	      unsigned int *packet_sizes, bool print_ok)
{
}

static void
setrecv_virtnic (void *handle, net_recv_callback_t *callback, void *param)
{
}

static void
poll_virtnic (void *handle)
{
}

static struct nicfunc virt_func = {
	.get_nic_info = getinfo_virtnic,
	.send = send_virtnic,
	.set_recv_callback = setrecv_virtnic,
	.poll = poll_virtnic,
};

static void
virtual_virtio_net_intr_clear (void *param)
{
}

static void
virtual_virtio_net_intr_set (void *param)
{
	struct data *d = param;
	int intnum = virtio_intr (d->virtio_net);

	if (intnum >= 0x20 && intnum <= 0xFF)
		self_ipi (intnum);
}

static void
virtual_virtio_net_intr_disable (void *param)
{
}

static void
virtual_virtio_net_intr_enable (void *param)
{
}

static void
virtual_virtio_net_msix_disable (void *param)
{
}

static void
virtual_virtio_net_msix_enable (void *param)
{
}

static void
virtual_virtio_net_new (struct pci_virtual_device *dev)
{
	char *option_net = dev->driver_options[1];
	bool option_tty = false;
	struct data *d = alloc (sizeof *d);
	struct nicfunc *virtio_net_func;
	static u8 devcount;

	dev->host = d;
	if (dev->driver_options[0] &&
	    pci_driver_option_get_bool (dev->driver_options[0], NULL))
		option_tty = true;
	d->nethandle = net_new_nic (option_net, option_tty);
	d->macaddr[0] = 0x02;
	d->macaddr[1] = 0x48;
	d->macaddr[2] = 0x84;
	d->macaddr[3] = 0x76;
	d->macaddr[4] = 0x70;
	d->macaddr[5] = devcount++;
	d->membase = 0xFFFFF000;
	d->membase_emul = false;
	d->mmio_registered = false;
	d->mmio_handle = NULL;
	d->virtio_net = virtio_net_init (&virtio_net_func,
					 d->macaddr,
					 virtual_virtio_net_intr_clear,
					 virtual_virtio_net_intr_set,
					 virtual_virtio_net_intr_disable,
					 virtual_virtio_net_intr_enable, d);
	if (d->virtio_net) {
		/* BAR5 for MSI-X tables. */
		virtio_net_set_msix (d->virtio_net, 0x5,
				     virtual_virtio_net_msix_disable,
				     virtual_virtio_net_msix_enable, d);
		/* Use virtio_net_func for phys_func. */
		net_init (d->nethandle, d->virtio_net, virtio_net_func, d,
			  &virt_func);
		net_start (d->nethandle);
	}
}

static void
replace (u8 iosize, u16 offset, void *data, int target_offset,
	 int target_size, u32 target_value)
{
	u8 *p = data;

	if (offset < target_offset) {
		if (target_offset - offset >= iosize)
			return;
		p += target_offset - offset;
		iosize -= target_offset - offset;
	} else if (offset > target_offset) {
		if (offset - target_offset >= target_size)
			return;
		target_value >>= (offset - target_offset) * 8;
		target_size -= offset - target_offset;
	}
	if (iosize >= target_size)
		memcpy (p, &target_value, target_size);
	else
		memcpy (p, &target_value, iosize);
}

static void
virtual_virtio_net_config_read (struct pci_virtual_device *dev, u8 iosize,
				u16 offset, union mem *data)
{
	struct data *d = dev->host;

	if (!d->virtio_net) {
		memset (data, 0xFF, iosize);
		return;
	}
	memset (data, 0x00, iosize);
	replace (iosize, offset, data, 0x6, 2, 0x0010); /* Status */
	replace (iosize, offset, data, 0x8, 4, 0x02000000); /* Class */
	replace (iosize, offset, data, 0x3C, 4, 0x000001FF); /* Interrupt */
	virtio_net_config_read (d->virtio_net, iosize, offset, data);
	replace (iosize, offset, data, 0x24, 4,
		 d->membase_emul ? 0xFFFFF000 : d->membase);
}

static int
mmhandler (void *data, phys_t gphys, bool wr, void *buf, uint len, u32 flags)
{
	struct data *d = data;

	if (!wr)
		memset (buf, 0, len);
	virtio_net_msix (d->virtio_net, wr, len, gphys - d->membase, buf);
	return 1;
}

static void
virtual_virtio_net_config_write (struct pci_virtual_device *dev, u8 iosize,
				 u16 offset, union mem *data)
{
	struct data *d = dev->host;

	if (!d->virtio_net)
		return;
	virtio_net_config_write (d->virtio_net, iosize, offset, data);
	if (offset == 0x24) {
		if ((data->dword & PCI_CONFIG_BASE_ADDRESS_MEMMASK) ==
		    PCI_CONFIG_BASE_ADDRESS_MEMMASK) {
			d->membase_emul = true;
		} else {
			d->membase_emul = false;
			u32 old = d->membase;
			memcpy (&d->membase, data, iosize);
			d->membase &= ~0xFFF;
			if (old != d->membase) {
				if (d->mmio_registered) {
					d->mmio_registered = false;
					mmio_unregister (d->mmio_handle);
				}
				d->mmio_handle = mmio_register (d->membase,
								0x1000,
								mmhandler, d);
				d->mmio_registered = true;
			}
		}
	}
}

static struct pci_virtual_driver virtual_virtio_net_driver = {
	.name		= "virtio-net",
	.longname	= "virtio-net virtual driver",
	.driver_options	= "tty,net",
	.new		= virtual_virtio_net_new,
	.config_read	= virtual_virtio_net_config_read,
	.config_write	= virtual_virtio_net_config_write,
};

static void
virtual_virtio_net_init (void)
{
	pci_register_virtual_driver (&virtual_virtio_net_driver);
}

PCI_DRIVER_INIT (virtual_virtio_net_init);
