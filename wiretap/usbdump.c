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
#include "file_wrappers.h"
#include "usbdump.h"

#define roundup2(x, y) (((x)+((y)-1))&(~((y)-1)))

#define MAGIC_SIZE      4

static const guint8 usbdump_magic[MAGIC_SIZE] = {
        0x0e, 0x00, 0x90, 0x9a
};

struct usbcap_filehdr {
    guint32      magic;
    guint8       major;
    guint8       minor;
    guint8       reserved[26];
} __attribute__ ((packed));

struct usbdump_hdr {
    guint32      ts_sec;
    guint32      ts_usec;
    guint32      caplen;
    guint32      datalen;
    guint8       hdrlen;
    guint8       align;
} __attribute__ ((packed));

struct usbdump_records_mark {
    gint64      offset;
    guint32      length;
};

static gboolean usbdump_read(wtap *wth, int *err, gchar **err_info,
    gint64 *data_offset);

static gboolean usbdump_seek_read(wtap *wth, gint64 seek_off,
    struct wtap_pkthdr *phdr, Buffer *buf, int *err, gchar **err_info);

static gboolean usbdump_read_record(wtap *wth, FILE_T fh, struct wtap_pkthdr *phdr, Buffer *buf, gint64 data_offset, int *err, gchar **err_info);

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

    //wth->file_encap = WTAP_ENCAP_USB_FREEBSD;
    //wth->snapshot_length = 0;
    wth->file_tsprec = WTAP_TSPREC_USEC; /* usbdump use tv_sec & tv_usec to express the capture timestamp */

    return WTAP_OPEN_MINE;
}

static gboolean usbdump_read(wtap *wth, int *err, gchar **err_info, gint64 *data_offset)
{
    gint64 cur_offset = file_tell(wth->fh);
    struct usbdump_records_mark *urm = (struct usbdump_records_mark*)wth->priv;
    if ((cur_offset - urm->offset) < urm->length) {
        *data_offset = cur_offset;
    }
    else {
        if (wtap_read_bytes(wth->fh, &urm->length, sizeof(guint32), err, err_info) == -1)
            return FALSE;
        urm->offset = file_tell(wth->fh);
        *data_offset = urm->offset;
    }

    return usbdump_read_record(wth, wth->fh, &wth->phdr, wth->frame_buffer, *data_offset, err, err_info);
}

static gboolean usbdump_seek_read(wtap *wth, gint64 seek_off, struct wtap_pkthdr *phdr, Buffer *buf, int *err, gchar **err_info)
{
    if (file_seek(wth->random_fh, seek_off, SEEK_SET, err) == -1)
        return FALSE;

    return usbdump_read_record(wth, wth->random_fh, phdr, buf, seek_off, err, err_info);
}

static gboolean usbdump_read_record(wtap *wth, FILE_T fh, struct wtap_pkthdr *phdr, Buffer *buf, gint64 data_offset, int *err, gchar **err_info)
{
    struct usbdump_hdr uhdr;
    guint32 ts_sec;
    guint32 ts_usec;
    guint32 caplen;
    guint32 datalen;
    guint8 stride;

    if (!wtap_read_bytes_or_eof(fh, &uhdr, sizeof(uhdr), err, err_info))
        return FALSE;
    ts_sec = GUINT32_FROM_LE(uhdr.ts_sec);
    ts_usec = GUINT32_FROM_LE(uhdr.ts_usec);
    caplen = GUINT32_FROM_LE(uhdr.caplen);
    datalen = GUINT32_FROM_LE(uhdr.datalen);

    stride = (guint8) roundup2(uhdr.hdrlen + uhdr.caplen, uhdr.align);

    phdr->rec_type = REC_TYPE_PACKET;
    phdr->presence_flags = WTAP_HAS_CAP_LEN | WTAP_HAS_TS;
    phdr->ts.secs = ts_sec;
    phdr->ts.nsecs = ts_usec * 1000;
    phdr->caplen = caplen;
    phdr->len = datalen;

    //FIXME
    if (wth) {}

    if (file_seek(fh, (gint64) (data_offset + uhdr.hdrlen), SEEK_SET, err) == -1)
        return FALSE;
    if (!wtap_read_packet_bytes(fh, buf, phdr->caplen, err, err_info))
        return FALSE;	/* Read error */
    if (file_seek(fh, (gint64) (data_offset + stride), SEEK_SET, err) == -1)
        return FALSE;
    return TRUE;
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
