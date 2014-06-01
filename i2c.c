/*
  stm32flash - Open Source ST STM32 flash program for *nix
  Copyright (C) 2014 Antonio Borneo <borneo.antonio@gmail.com>

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/


#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "serial.h"
#include "port.h"


#if defined(__WIN32__) || defined(__CYGWIN__)

static port_err_t i2c_open(struct port_interface *port,
			   struct port_options *ops)
{
	return PORT_ERR_NOT_RECOGNIZED;
}

struct port_interface port_i2c = {
	.name	= "i2c",
	.open	= i2c_open,
};

#else

#include <linux/i2c-dev.h>
#include <sys/ioctl.h>

struct i2c_priv {
	int fd;
	int addr;
};

static port_err_t i2c_open(struct port_interface *port,
			   struct port_options *ops)
{
	struct i2c_priv *h;
	int fd, addr, ret;

	/* 1. check device name match */
	if (strncmp(ops->device, "/dev/i2c-", strlen("/dev/i2c-")))
		return PORT_ERR_NOT_RECOGNIZED;

	/* 2. check options */
	addr = ops->bus_addr;
	if (addr < 0x03 || addr > 0x77) {
		fprintf(stderr, "I2C address out of range [0x03-0x77]\n");
		return PORT_ERR_UNKNOWN;
	}

	/* 3. open it */
	h = calloc(sizeof(*h), 1);
	if (h == NULL) {
		fprintf(stderr, "End of memory\n");
		return PORT_ERR_UNKNOWN;
	}
	fd = open(ops->device, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "Unable to open special file \"%s\"\n",
			ops->device);
		free(h);
		return PORT_ERR_UNKNOWN;
	}

	/* 4. set options */
	ret = ioctl(fd, I2C_SLAVE, addr);
	if (ret < 0) {
		fprintf(stderr, "I2C ioctl error %d\n", errno);
		close(fd);
		free(h);
		return PORT_ERR_UNKNOWN;
	}

	h->fd = fd;
	h->addr = addr;
	port->private = h;
	return PORT_ERR_OK;
}

static port_err_t i2c_close(struct port_interface *port)
{
	struct i2c_priv *h;

	h = (struct i2c_priv *)port->private;
	if (h == NULL)
		return PORT_ERR_UNKNOWN;
	close(h->fd);
	free(h);
	port->private = NULL;
	return PORT_ERR_OK;
}

static port_err_t i2c_read(struct port_interface *port, void *buf,
			   size_t nbyte)
{
	struct i2c_priv *h;
	int ret;

	h = (struct i2c_priv *)port->private;
	if (h == NULL)
		return PORT_ERR_UNKNOWN;
	ret = read(h->fd, buf, nbyte);
	if (ret != nbyte)
		return PORT_ERR_UNKNOWN;
	return PORT_ERR_OK;
}

static port_err_t i2c_write(struct port_interface *port, void *buf,
			    size_t nbyte)
{
	struct i2c_priv *h;
	int ret;

	h = (struct i2c_priv *)port->private;
	if (h == NULL)
		return PORT_ERR_UNKNOWN;
	ret = write(h->fd, buf, nbyte);
	if (ret != nbyte)
		return PORT_ERR_UNKNOWN;
	return PORT_ERR_OK;
}

static port_err_t i2c_gpio(struct port_interface *port, serial_gpio_t n,
			   int level)
{
	return PORT_ERR_OK;
}

static const char *i2c_get_cfg_str(struct port_interface *port)
{
	struct i2c_priv *h;
	static char str[11];

	h = (struct i2c_priv *)port->private;
	if (h == NULL)
		return "INVALID";
	snprintf(str, sizeof(str), "addr 0x%2x", h->addr);
	return str;
}

struct port_interface port_i2c = {
	.name	= "i2c",
	.flags	= 0,
	.open	= i2c_open,
	.close	= i2c_close,
	.read	= i2c_read,
	.write	= i2c_write,
	.gpio	= i2c_gpio,
	.get_cfg_str	= i2c_get_cfg_str,
};

#endif
