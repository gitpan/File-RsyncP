/* 
   Copyright (C) Andrew Tridgell 1996
   Copyright (C) Paul Mackerras 1996
   Copyright (C) 2001, 2002 by Martin Pool <mbp@samba.org>
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

/** @file flist.c
 * Generate and receive file lists
 *
 * @todo Get rid of the string_area optimization.  Efficiently
 * allocating blocks is the responsibility of the system's malloc
 * library, not of rsync.
 *
 * @sa http://lists.samba.org/pipermail/rsync/2000-June/002351.html
 *
 **/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <math.h>

#include <XSUB.h>

#include "flist.h"

extern struct stats stats;

extern int verbose;
extern int do_progress;
extern int am_server;
extern int always_checksum;

extern int cvs_exclude;

extern int recurse;

extern int one_file_system;
extern int make_backups;
extern int relative_paths;
extern int copy_links;
extern int copy_unsafe_links;
extern int remote_version;
extern int io_error;
extern int sanitize_paths;

extern int read_batch;
extern int write_batch;

static struct file_struct null_file;

static int to_wire_mode(mode_t mode)
{
    if (S_ISLNK(mode) && (_S_IFLNK != 0120000)) {
        return (mode & ~(_S_IFMT)) | 0120000;
    }
    return (int) mode;
}

static mode_t from_wire_mode(int mode)
{
    if ((mode & (_S_IFMT)) == 0120000 && (_S_IFLNK != 0120000)) {
        return (mode & ~(_S_IFMT)) | _S_IFLNK;
    }
    return (mode_t) mode;
}

/* we need this function because of the silly way in which duplicate
   entries are handled in the file lists - we can't change this
   without breaking existing versions */
static int flist_up(struct file_list *flist, int i)
{
	while (!flist->files[i]->basename) i++;
	return i;
}

/**
 * Make sure @p flist is big enough to hold at least @p flist->count
 * entries.
 **/
void flist_expand(struct file_list *flist)
{
    if (flist->count >= flist->malloced) {
        size_t new_bytes;
        void *new_ptr;
        
        if (flist->malloced < 1000)
            flist->malloced += 1000;
        else
            flist->malloced *= 2;

        new_bytes = sizeof(flist->files[0]) * flist->malloced;
        
        if (flist->files)
            new_ptr = realloc(flist->files, new_bytes);
        else
            new_ptr = malloc(new_bytes);

        flist->files = (struct file_struct **) new_ptr;
    }
}

/* Like strncpy but does not 0 fill the buffer and always null 
 * terminates. bufsize is the size of the destination buffer.
 * 
 * Returns the index of the terminating byte. */
size_t strlcpy(char *d, const char *s, size_t bufsize)
{
        size_t len = strlen(s);
        size_t ret = len;
        if (bufsize <= 0) return 0;
        if (len >= bufsize) len = bufsize-1;
        memcpy(d, s, len);
        d[len] = 0;
        return ret;
}

/*
 * return the full filename of a flist entry
 *
 * This function is too expensive at the moment, because it copies
 * strings when often we only want to compare them.  In any case,
 * using strlcat is silly because it will walk the string repeatedly.
 */
char *f_name(struct file_struct *f)
{
    static char names[10][MAXPATHLEN];
    static int n;
    char *p = names[n];

    if (!f || !f->basename)
            return NULL;

    n = (n + 1) % 10;

    if (f->dirname) {
        int off;

        off = strlcpy(p, f->dirname, MAXPATHLEN);
        off += strlcpy(p + off, "/", MAXPATHLEN - off);
        off += strlcpy(p + off, f->basename, MAXPATHLEN - off);
    } else {
        strlcpy(p, f->basename, MAXPATHLEN);
    }
    return p;
}

/* we need to supply our own strcmp function for file list comparisons
   to ensure that signed/unsigned usage is consistent between machines. */
static int u_strcmp(const char *cs1, const char *cs2)
{
    const uchar *s1 = (const uchar *)cs1;
    const uchar *s2 = (const uchar *)cs2;

    while (*s1 && *s2 && (*s1 == *s2)) {
        s1++; s2++;
    }

    return (int)*s1 - (int)*s2;
}

/*
 * XXX: This is currently the hottest function while building the file
 * list, because building f_name()s every time is expensive.
 **/
int file_compare(struct file_struct **f1, struct file_struct **f2)
{
    if (!(*f1)->basename && !(*f2)->basename)
        return 0;
    if (!(*f1)->basename)
        return -1;
    if (!(*f2)->basename)
        return 1;
    if ((*f1)->dirname == (*f2)->dirname)
        return u_strcmp((*f1)->basename, (*f2)->basename);
    return u_strcmp(f_name(*f1), f_name(*f2));
}

int flist_find(struct file_list *flist, struct file_struct *f)
{
    int low = 0, high = flist->count - 1;

    if (flist->count <= 0)
        return -1;

    while (low != high) {
        int mid = (low + high) / 2;
        int ret =
            file_compare(&flist->files[flist_up(flist, mid)], &f);
        if (ret == 0)
            return flist_up(flist, mid);
        if (ret > 0) {
            high = mid;
        } else {
            low = mid + 1;
        }
    }

    if (file_compare(&flist->files[flist_up(flist, low)], &f) == 0)
            return flist_up(flist, low);
    return -1;
}

/*
 * free up one file
 */
void free_file(struct file_struct *file)
{
    if (!file)
        return;
    if (file->basename)
        free(file->basename);
    if ( file->dirnameAlloc )
        free(file->dirname);
    if (file->link)
        free(file->link);
    if (file->sum)
        free(file->sum);
    *file = null_file;
}

/*
 * allocate a new file list
 */
struct file_list *flist_new(void)
{
    struct file_list *flist;

    flist = (struct file_list *) malloc(sizeof(*flist));
    memset(flist, 0, sizeof(*flist));
    flist->count = 0;
    flist->malloced = 0;
    flist->files = NULL;
    return flist;
}

/*
 * free up all elements in a flist
 */
void flist_free(struct file_list *flist)
{
    int i;
    for ( i = 0; i < flist->count; i++ ) {
        free_file(flist->files[i]);
        free(flist->files[i]);
    }
    /* FIXME: I don't think we generally need to blank the flist
     * since it's about to be freed.  This will just cause more
     * memory traffic.  If you want a freed-memory debugger, you
     * know where to get it. */
    memset((char *) flist->files, 0, sizeof(flist->files[0]) * flist->count);
    free(flist->files);
    if ( flist->outBuf )
        free(flist->outBuf);
    memset((char *) flist, 0, sizeof(*flist));
    free(flist);
}

/*
 * This routine ensures we don't have any duplicate names in our file list.
 * duplicate names can cause corruption because of the pipelining 
 */
void clean_flist(struct file_list *flist, int strip_root)
{
    int i;

    if (!flist || flist->count == 0)
        return;

    qsort(flist->files, flist->count,
          sizeof(flist->files[0]), (int (*)()) file_compare);

    for (i = 1; i < flist->count; i++) {
        if (flist->files[i]->basename &&
            flist->files[i - 1]->basename &&
            strcmp(f_name(flist->files[i]),
                   f_name(flist->files[i - 1])) == 0) {
                /* it's not great that the flist knows the semantics of the
                 * file memory usage, but i'd rather not add a flag byte
                 * to that struct. XXX can i use a bit in the flags field? */
                free_file(flist->files[i]);
        }
    }

    /* FIXME: There is a bug here when filenames are repeated more
     * than once, because we don't handle freed files when doing
     * the comparison. */

    if (strip_root) {
            /* we need to strip off the root directory in the case
               of relative paths, but this must be done _after_
               the sorting phase */
        for (i = 0; i < flist->count; i++) {
            if (flist->files[i]->dirname &&
                flist->files[i]->dirname[0] == '/') {
                    memmove(&flist->files[i]->dirname[0],
                            &flist->files[i]->dirname[1],
                            strlen(flist->files[i]->dirname));
            }

            if (flist->files[i]->dirname &&
                !flist->files[i]->dirname[0]) {
                    flist->files[i]->dirname = NULL;
            }
        }
    }
}

void clean_fname(char *name)
{
    char *p;
    int l;
    int modified = 1;

    if (!name) return;

    while (modified) {
        modified = 0;

        if ((p=strstr(name,"/./"))) {
            modified = 1;
            while (*p) {
                p[0] = p[2];
                p++;
            }
        }

        if ((p=strstr(name,"//"))) {
            modified = 1;
            while (*p) {
                p[0] = p[1];
                p++;
            }
        }

        if (strncmp(p=name,"./",2) == 0) {      
            modified = 1;
            do {
                p[0] = p[2];
            } while (*p++);
        }

        l = strlen(p=name);
        if (l > 1 && p[l-1] == '/') {
            modified = 1;
            p[l-1] = 0;
        }
    }
}

static void readfd(struct file_list *f, unsigned char *buffer, size_t N)
{
    if ( f->inError || f->inPosn + N > f->inLen ) {
        memset(buffer, 0, N);
        f->inError = 1;
        return;
    }
    memcpy(buffer, f->inBuf + f->inPosn, N);
    f->inPosn += N;
}

int32 read_int(struct file_list *f)
{
    unsigned char b[4];
    int32 ret;

    readfd(f,b,4);
    ret = b[0] | b[1] << 8 | b[2] << 16 | b[3] << 24;
    if (ret == (int32)0xffffffff) return -1;
    return ret;
}

double read_longint(struct file_list *f)
{
    char b[8];
    int32 ret = read_int(f);
    double d;

    if (ret != (int32)0xffffffff) {
        return ret;
    }
    d  = (uint32)read_int(f);
    d += ((uint32)read_int(f)) * 65536.0 * 65536.0;
    return d;
}

void read_buf(struct file_list *f,char *buf,size_t len)
{
    readfd(f, buf, len);
}

void read_sbuf(struct file_list *f,char *buf,size_t len)
{
    read_buf(f,buf,len);
    buf[len] = 0;
}

unsigned char read_byte(struct file_list *f)
{
    unsigned char c;
    read_buf (f, (char *)&c, 1);
    return c;
}

void receive_file_entry(struct file_list *f, struct file_struct **fptr,
			       unsigned flags)
{
    char thisname[MAXPATHLEN];
    char lastname[MAXPATHLEN];
    unsigned int l1 = 0, l2 = 0;
    char *p;
    struct file_struct file, *filePtr;

    memset((char *) &file, 0, sizeof(file));
    if (flags & SAME_NAME)
        l1 = read_byte(f);

    if (flags & LONG_NAME)
        l2 = read_int(f);
    else
        l2 = read_byte(f);

    if (l2 >= MAXPATHLEN - l1) {
        printf("overflow: flags=0x%x l1=%d l2=%d, lastname=%s\n",
                            flags, l1, l2, f->lastname);
        f->fatalError = 1;
        return;
    }

    strlcpy(thisname, f->lastname, l1 + 1);
    read_sbuf(f, &thisname[l1], l2);
    thisname[l1 + l2] = 0;

    strlcpy(lastname, thisname, MAXPATHLEN);
    lastname[MAXPATHLEN - 1] = 0;

    clean_fname(thisname);

#if 0
    if (sanitize_paths) {
            sanitize_path(thisname, NULL);
    }
#endif

    if ((p = strrchr(thisname, '/'))) {
        *p = 0;
        if ( f->lastdir && strcmp(thisname, f->lastdir) == 0) {
            file.dirname = f->lastdir;
            file.dirnameAlloc = 0;
        } else {
            file.dirname = strdup(thisname);
            f->lastdir = file.dirname;
            file.dirnameAlloc = 1;
        }
        file.basename = strdup(p + 1);
    } else {
        file.dirname = NULL;
        file.basename = strdup(thisname);
    }

    if (!file.basename) {
        printf("out of memory on basename\n");
        free_file(&file);
        f->fatalError = 1;
        return;
    }

    file.flags = flags;
    file.length = read_longint(f);
    file.modtime = (flags & SAME_TIME) ? f->last_time : (time_t) read_int(f);
    file.mode = (flags & SAME_MODE) ? f->last_mode
                                    : from_wire_mode(read_int(f));
    if (f->preserve_uid)
        file.uid = (flags & SAME_UID) ? f->last_uid : (uid_t) read_int(f);
    if (f->preserve_gid)
        file.gid = (flags & SAME_GID) ? f->last_gid : (gid_t) read_int(f);
    if (f->preserve_devices && IS_DEVICE(file.mode))
        file.rdev = (flags & SAME_RDEV) ? f->last_rdev : (dev_t) read_int(f);

    if (f->preserve_links && S_ISLNK(file.mode)) {
        int l = read_int(f);
        if (l < 0) {
            printf("overflow on symlink: l=%d\n", l);
            f->fatalError = 1;
            free_file(&file);
            return;
        }
        file.link = (char *) malloc(l + 1);
        read_sbuf(f, file.link, l);
#if 0
        if (sanitize_paths) {
            sanitize_path(file.link, file.dirname);
        }
#endif
    }

    if (f->preserve_hard_links && S_ISREG(file.mode)) {
        if (f->remote_version < 26) {
            file.dev = read_int(f);
            file.inode = read_int(f);
        } else {
            file.dev = read_longint(f);
            file.inode = read_longint(f);
        }
    }

    if ( f->always_checksum ) {
        file.sum = (char *) malloc(MD4_SUM_LENGTH);
        if ( f->remote_version < 21 ) {
            read_buf(f, file.sum, 2);
        } else {
            read_buf(f, file.sum, MD4_SUM_LENGTH);
        }
    }

    /*
     * It's important that we don't update anything in f before
     * this point.  If we ran out of input bytes then we need to
     * resume after the caller appends more bytes.
     */
    if ( f->inError ) {
        free_file(&file);
	return;
    }

    strlcpy(f->lastname, lastname, MAXPATHLEN);
    f->lastname[MAXPATHLEN - 1] = 0;
    f->last_mode = file.mode;
    f->last_rdev = file.rdev;
    f->last_uid  = file.uid;
    f->last_gid  = file.gid;
    f->last_time = file.modtime;

    filePtr = (struct file_struct *) malloc(sizeof(file));
    memcpy(filePtr, &file, sizeof(file));
    *fptr = filePtr;
}

static void writefd(struct file_list *f, char *buf, size_t len)
{
    if ( !f->outBuf ) {
        f->outLen = 32768 + len;
        f->outBuf = malloc(f->outLen);
    } else if ( f->outPosn + len > f->outLen ) {
        f->outLen = 32768 + f->outPosn + len;
        f->outBuf = realloc(f->outBuf, f->outLen);
    }
    memcpy(f->outBuf + f->outPosn, buf, len);
    f->outPosn += len;
}

static void write_int(struct file_list *f,int32 x)
{
    char b[4];
    b[0] = x >> 0;
    b[1] = x >> 8;
    b[2] = x >> 16;
    b[3] = x >> 24;
    writefd(f,b,4);
}

/*
 * Note: int64 may actually be a 32-bit type if ./configure couldn't find any
 * 64-bit types on this platform.
 */
static void write_longint(struct file_list *f, double x)
{
    if (f->remote_version < 16 || x <= 0x7FFFFFFF) {
        write_int(f, (int)x);
        return;
    }
    write_int(f, (int32)0xFFFFFFFF);
    write_int(f, (uint32)(fmod(x, 65536.0 * 65536.0)));
    write_int(f, (uint32)(x / (65536.0 * 65536.0)));
}

static void write_buf(struct file_list *f,char *buf,size_t len)
{
    writefd(f,buf,len);
}

/* write a string to the connection */
static void write_sbuf(struct file_list *f,char *buf)
{
    write_buf(f, buf, strlen(buf));
}

static void write_byte(struct file_list *f,unsigned char c)
{
    write_buf(f,(char *)&c,1);
}

void send_file_entry(struct file_list *f, struct file_struct *file)
{
    unsigned char flags;
    char *fname;
    int l1, l2;

    if ( !file ) {
        write_byte(f, 0);
        return;
    }
    fname = f_name(file);
    flags = 0;
    if (file->mode == f->last_mode)
        flags |= SAME_MODE;
    if (file->rdev == f->last_rdev)
        flags |= SAME_RDEV;
    if (file->uid == f->last_uid)
        flags |= SAME_UID;
    if (file->gid == f->last_gid)
        flags |= SAME_GID;
    if (file->modtime == f->last_time)
        flags |= SAME_TIME;

    for (l1 = 0;
         f->lastname[l1] && (fname[l1] == f->lastname[l1]) && (l1 < 255);
         l1++);
    l2 = strlen(fname) - l1;

    if (l1 > 0)
        flags |= SAME_NAME;
    if (l2 > 255)
        flags |= LONG_NAME;

    /* we must make sure we don't send a zero flags byte or the other
       end will terminate the flist transfer */
    if (flags == 0 && !S_ISDIR(file->mode))
        flags |= FLAG_DELETE;
    if (flags == 0)
        flags |= LONG_NAME;

    write_byte(f, flags);
    if (flags & SAME_NAME)
        write_byte(f, l1);
    if (flags & LONG_NAME)
        write_int(f, l2);
    else
        write_byte(f, l2);
    write_buf(f, fname + l1, l2);

    write_longint(f, file->length);
    if (!(flags & SAME_TIME))
        write_int(f, (int) file->modtime);
    if (!(flags & SAME_MODE))
        write_int(f, to_wire_mode(file->mode));
    if (f->preserve_uid && !(flags & SAME_UID)) {
        write_int(f, (int) file->uid);
    }
    if (f->preserve_gid && !(flags & SAME_GID)) {
        write_int(f, (int) file->gid);
    }
    if (f->preserve_devices
            && IS_DEVICE(file->mode)
            && !(flags & SAME_RDEV))
        write_int(f, (int) file->rdev);

    if (f->preserve_links && S_ISLNK(file->mode)) {
        write_int(f, strlen(file->link));
        write_buf(f, file->link, strlen(file->link));
    }

    if (f->preserve_hard_links && S_ISREG(file->mode)) {
        if (f->remote_version < 26) {
            /* 32-bit dev_t and ino_t */
            write_int(f, (int) file->dev);
            write_int(f, (int) file->inode);
        } else {
            /* 64-bit dev_t and ino_t */
            write_longint(f, file->dev);
            write_longint(f, file->inode);
        }
    }

    if (f->always_checksum) {
        if (f->remote_version < 21) {
            write_buf(f, file->sum, 2);
        } else {
            write_buf(f, file->sum, MD4_SUM_LENGTH);
        }
    }

    f->last_mode = file->mode;
    f->last_rdev = file->rdev;
    f->last_uid = file->uid;
    f->last_gid = file->gid;
    f->last_time = file->modtime;

    strlcpy(f->lastname, fname, MAXPATHLEN);
    f->lastname[MAXPATHLEN - 1] = 0;
}

int flistDecodeBytes(struct file_list *f, unsigned char *bytes, uint32 nBytes)
{
    unsigned char flags;

    f->inBuf = bytes;
    f->inLen = nBytes;
    f->inFileStart = f->inPosn = 0;
    f->inError = 0;
    f->fatalError = 0;
    f->decodeDone = 0;
    for ( flags = read_byte(f); flags; flags = read_byte(f) ) {
        int i = f->count;

        flist_expand(f);
        receive_file_entry(f, &f->files[i], flags);
        if ( f->inError ) {
            /* printf("Returning on input error, posn = %d\n", f->inPosn); */
            break;
        }
#if 0
        if (S_ISREG(f->files[i]->mode))
                stats.total_size += f->files[i]->length;
#endif

        f->count++;
        f->inFileStart = f->inPosn;
    }
    if ( f->fatalError ) {
        return -1;
    } else if ( f->inError ) {
        return f->inFileStart;
    } else {
        f->decodeDone = 1;
        return f->inPosn;
    }
}
