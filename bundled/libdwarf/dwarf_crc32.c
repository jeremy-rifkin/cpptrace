/*
   Copyright (C) 2020 David Anderson. All Rights Reserved.

   This program is free software; you can redistribute it
   and/or modify it under the terms of version 2.1 of the
   GNU Lesser General Public License as published by the Free
   Software Foundation.

   This program is distributed in the hope that it would be
   useful, but WITHOUT ANY WARRANTY; without even the implied
   warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
   PURPOSE.

   Further, this software is distributed without any warranty
   that it is free of the rightful claim of any third person
   regarding infringement or the like.  Any license provided
   herein, whether implied or otherwise, applies only to this
   software file.  Patent licenses, if any, provided herein
   do not apply to combinations of this program with other
   software, or any other product whatsoever.

   You should have received a copy of the GNU Lesser General
   Public License along with this program; if not, write
   the Free Software Foundation, Inc., 51 Franklin Street -
   Fifth Floor, Boston MA 02110-1301, USA.
*/

#include <config.h>

#include <stddef.h> /* size_t */
#include <stdio.h>  /* SEEK_END SEEK_SET */
#include <stdlib.h> /* free() malloc() */
#include <string.h> /* memcpy() */

#ifdef _WIN32
#ifdef HAVE_STDAFX_H
#include "stdafx.h"
#endif /* HAVE_STDAFX_H */
#include <io.h> /* lseek() off_t ssize_t */
#elif defined HAVE_UNISTD_H
#include <unistd.h> /* lseek() off_t */
#endif /* _WIN32 */

#include "dwarf.h"
#include "libdwarf.h"
#include "libdwarf_private.h"
#include "dwarf_base_types.h"
#include "dwarf_opaque.h"
#include "dwarf_error.h"

/*  Returns DW_DLV_OK DW_DLV_NO_ENTRY or DW_DLV_ERROR
    crc32 used for debuglink crc calculation.
    Caller passes pointer to an
    uninitialized array of 4 unsigned char
    and if this returns DW_DLV_OK that is filled in.
    The crc is calculated based on reading
    the entire current open
    Dwarf_Debug dbg object file and all bytes in
    the file are read to create  the crc.  */
int
dwarf_crc32 (Dwarf_Debug dbg,unsigned char *crcbuf,
    Dwarf_Error *error)
{
    /*  off_t is signed,    defined by POSIX */
    /*  ssize_t is signed,  defined in POSIX */
    /*  size_t is unsigned, defined in C89. */
    off_t   size_left = 0;
    off_t   fsize = 0;
    off_t   lsval = 0;
    /*  Named with u to remind the reader that this is
        an unsigned value. */
    size_t         readlenu = 1000;
    unsigned char *readbuf = 0;
    unsigned int   tcrc = 0;
    unsigned int   init = 0;
    int            fd = -1;

    if (!dbg) {
        _dwarf_error_string(dbg,error,DW_DLE_DBG_NULL,
            "DW_DLE_DBG_NULL: Bad call to dwarf_crc32");
        return DW_DLV_ERROR;
    }
    if (!crcbuf) {
        return DW_DLV_NO_ENTRY;
    }
    if (!dbg->de_owns_fd) {
        return DW_DLV_NO_ENTRY;
    }
    fd = dbg->de_fd;
    if (fd < 0) {
        return DW_DLV_NO_ENTRY;
    }
    fd = dbg->de_fd;
    if (dbg->de_filesize) {
        fsize = size_left = (off_t)dbg->de_filesize;
    } else {
        fsize = size_left = lseek(fd,0L,SEEK_END);
        if (fsize   == (off_t)-1) {
            _dwarf_error_string(dbg,error,DW_DLE_SEEK_ERROR,
                "DW_DLE_SEEK_ERROR: dwarf_crc32 seek "
                "to end fails");
            return DW_DLV_ERROR;
        }
    }
    if (fsize <= (off_t)500) {
        /*  Not a real object file.
            A random length check. */
        return DW_DLV_NO_ENTRY;
    }
    lsval  = lseek(fd,0L,SEEK_SET);
    if (lsval < 0) {
        _dwarf_error_string(dbg,error,DW_DLE_SEEK_ERROR,
            "DW_DLE_SEEK_ERROR: dwarf_crc32 seek "
            "to start fails");
        return DW_DLV_ERROR;
    }
    readbuf = (unsigned char *)malloc(readlenu);
    if (!readbuf) {
        _dwarf_error_string(dbg,error,DW_DLE_ALLOC_FAIL,
            "DW_DLE_ALLOC_FAIL: dwarf_crc32 read buffer"
            " alloc fails");
        return DW_DLV_ERROR;
    }
    while (size_left > 0) {
        ssize_t readreturnv = 0;

        if (size_left < (off_t)readlenu) {
            readlenu = (size_t)size_left;
        }
        /*  Fix warning on Windows: read()'s
            3rd parameter is a unsigned const */
#ifdef _WIN32
        readreturnv = read(fd,readbuf,(unsigned const)readlenu);
#else
        readreturnv = read(fd,readbuf,readlenu);
#endif
        if (readreturnv != (ssize_t)readlenu) {
            _dwarf_error_string(dbg,error,DW_DLE_READ_ERROR,
                "DW_DLE_READ_ERROR: dwarf_crc32 read fails ");
            free(readbuf);
            return DW_DLV_ERROR;
        }
        /*  Call the public API function so it gets tested too. */
        tcrc = dwarf_basic_crc32(readbuf,readlenu,
            (unsigned long)init);
        init = tcrc;
        size_left -= (off_t)readlenu;
    }
    /*  endianness issues?  */
    free(readbuf);
    memcpy(crcbuf,(void *)&tcrc,4);
    return DW_DLV_OK;
}
