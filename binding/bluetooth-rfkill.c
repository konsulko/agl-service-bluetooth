/*
 * Copyright 2018 Konsulko Group
 * Author: Matt Ranostay <matt.ranostay@konsulko.com>
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>

#include <linux/rfkill.h>

#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#define AFB_BINDING_VERSION 3
#include <afb/afb-binding.h>

#define HCIDEVUP	_IOW('H', 201, int)
#define BTPROTO_HCI	1



static int hci_interface_enable(int hdev)
{
    int ctl = socket(AF_BLUETOOTH, SOCK_RAW, BTPROTO_HCI);
    int ret;

    if (ctl < 0)
        return ctl;

    ret = ioctl(ctl, HCIDEVUP, hdev);

    close(ctl);

    return ret;
}

static gboolean bluetooth_rfkill_event(GIOChannel *chan, GIOCondition cond, gpointer ptr)
{
    struct rfkill_event event;
    gchar *name, buf[8];
    ssize_t len;
    int fd;

    fd = g_io_channel_unix_get_fd(chan);
    len = read(fd, &event, sizeof(struct rfkill_event));

    if (len != sizeof(struct rfkill_event))
        return TRUE;

    if (event.type != RFKILL_TYPE_BLUETOOTH)
        return TRUE;

    if (event.op == RFKILL_OP_DEL)
        return TRUE;

    name = g_strdup_printf("/sys/class/rfkill/rfkill%u/soft", event.idx);

    fd = g_open(name, O_WRONLY);
    len = write(fd, "0", 1);
    g_close(fd, NULL);

    g_free(name);

    memset(&buf, 0, sizeof(buf));

    name = g_strdup_printf("/sys/class/rfkill/rfkill%u/name", event.idx);
    fd = g_open(name, O_RDONLY);
    len = read(fd, &buf, sizeof(buf) - 1);

    if (len > 0)
    {
        int idx = 0;
        sscanf(buf, "hci%d", &idx);

        /*
         * 50 millisecond delay to allow time for rfkill to unblock before
         * attempting to bring up HCI interface
         */
        g_usleep(50000);

        hci_interface_enable(idx);
    }

    g_free(name);

    return TRUE;
}

int bluetooth_monitor_init(void)
{
    int fd = g_open("/dev/rfkill", O_RDWR);
    GIOChannel *chan = NULL;
    int ret = -EINVAL;

    if (fd < 0)
    {
        AFB_ERROR("Cannot open /dev/rfkill");
        return ret;
    }

    chan = g_io_channel_unix_new(fd);
    g_io_channel_set_close_on_unref(chan, TRUE);

    ret = g_io_add_watch(chan, G_IO_IN, bluetooth_rfkill_event, NULL);

    g_io_channel_unref(chan);

    return 0;
}
