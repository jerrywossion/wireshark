/* usbdump.c
 *
 * Copyright 2017, Jie Weng <jerrywossion@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "config.h"
#include <string.h>
#include "wtap-int.h"
#include "usbdump.h"
#include "file_wrappers.h"

#define MAGIC_SIZE      4

static const guint8 usbdump_magic[MAGIC_SIZE] = {
        0x0e, 0x00, 0x90, 0x9a
};

struct usbcap_filehdr {
    gint32      magic;
    gint8       major;
    gint8       minor;
    gint8       reserved[26];
} __attribute__ ((packed));

struct usbdump_hdr {
    gint32      ts_sec;
    gint32      ts_usec;
    gint32      caplen;
    gint32      datalen;
    gint8       hdrlen;
    gint8       align;
} __attribute__ ((packed));

struct usbdump_records_mark {
    gint64      offset;
    gint32      length;
};

static gboolean usbdump_read(wtap *wth, int *err, gchar **err_info,
    gint64 *data_offset);

static gboolean usbdump_seek_read(wtap *wth, gint64 seek_off,
    struct wtap_pkthdr *phdr, Buffer *buf, int *err, gchar **err_info);

static gboolean usbdump_read_record(wtap *wth, FILE_T fh, struct wtap_pkthdr *phdr, Buffer *buf, int *err, gchar **err_info);

wtap_open_return_val usbdump_open(wtap *wth, int *err, gchar **err_info)
{
    struct usbcap_filehdr hdr;

    /* Read the magic at the start of a usbdump file */
    if (!wtap_read_bytes(wth->fh, (char *)&hdr, sizeof(hdr.magic), err, err_info)) {
        if (*err != WTAP_ERR_SHORT_READ)
            return WTAP_OPEN_ERROR;
        return WTAP_OPEN_NOT_MINE;
    }

    if (memcmp(&hdr.magic, usbdump_magic, sizeof(usbdump_magic)) != 0)
        return WTAP_OPEN_NOT_MINE;

    /* Read the rest of the file header */
    if (!wtap_read_bytes(wth->fh, (char *)&hdr + sizeof(hdr.magic), sizeof(hdr) - sizeof(hdr.magic), err, err_info))
        return WTAP_OPEN_ERROR;
    wth->file_type_subtype = WTAP_FILE_TYPE_SUBTYPE_USBDUMP;
    wth->subtype_read = usbdump_read;
    wth->subtype_seek_read = usbdump_seek_read;

    struct usbdump_records_mark *urm;
    urm = (struct usbdump_records_mark*)g_malloc(sizeof(struct usbdump_records_mark));
    /* Read length of the first buntch of records */
    if (!wtap_read_bytes(wth->fh, &urm->length, sizeof(gint32), err, err_info))
        return WTAP_OPEN_ERROR;
    urm->offset = file_tell(wth->fh);
    wth->priv = (void*)urm;

    //TODO: should I add a new file encapsulation type for usbdump?
    // wth->file_encap = XXX
    wth->snapshot_length = 0;
    wth->file_tsprec = WTAP_TSPREC_USEC; /* usbdump use tv_sec & tv_usec to express the capture timestamp */

    return WTAP_OPEN_MINE;
}

static gboolean usbdump_read(wtap *wth, int *err, gchar **err_info, gint64 *data_offset)
{
    gint64 tmp_offset = file_tell(wth->fh);
    struct usbdump_records_mark *urm = (struct usbdump_records_mark*)wth->priv;
    if ((tmp_offset - urm->offset) < urm->length) {
        *data_offset = tmp_offset;
    }
    else {
        if (!wtap_read_bytes(wth->fh, &urm->length, sizeof(gint32), err, err_info))
            return FALSE;
        urm->offset = file_tell(wth->fh);
        *data_offset = urm->offset;
    }

    return usbdump_read_record(wth, wth->fh, &wth->phdr, wth->frame_buffer, err, err_info);
}

static gboolean usbdump_seek_read(wtap *wth, gint64 seek_off, struct wtap_pkthdr *phdr, Buffer *buf, int *err, gchar **err_info)
{
    if (file_seek(wth->random_fh, seek_off, SEEK_SET, err) == -1)
        return FALSE;

    return usbdump_read_record(wth, wth->random_fh, &wth->phdr, wth->frame_buffer, err, err_info);
}

static gboolean usbdump_read_record(wtap *wth, FILE_T fh, struct wtap_pkthdr *phdr, Buffer *buf, int *err, gchar **err_info)
{
}

/*
 * Editor modelines  -  http://www.wireshark.org/tools/modelines.html
 *
 * Local variables:
 * c-basic-offset: 4
 * tab-width: 8
 * indent-tabs-mode: nil
 * End:
 *
 * vi: set shiftwidth=4 tabstop=8 expandtab:
 * :indentSize=4:tabSize=8:noTabs=true:
 */
