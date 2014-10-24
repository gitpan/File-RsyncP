/* 
   Copyright (C) by Andrew Tridgell 1996, 2000
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


#define False 0
#define True 1

#define BLOCK_SIZE 700
#define RSYNC_RSH_ENV "RSYNC_RSH"

#define RSYNC_NAME "rsync"
#define RSYNCD_CONF "/etc/rsyncd.conf"

#define DEFAULT_LOCK_FILE "/var/run/rsyncd.lock"
#define URL_PREFIX "rsync://"

#define BACKUP_SUFFIX "~"

/* a non-zero CHAR_OFFSET makes the rolling sum stronger, but is
   incompatible with older versions :-( */
#define CHAR_OFFSET 0


#define FLAG_DELETE (1<<0)
#define SAME_MODE (1<<1)
#define SAME_RDEV (1<<2)
#define SAME_UID (1<<3)
#define SAME_GID (1<<4)
#define SAME_DIR (1<<5)
#define SAME_NAME SAME_DIR
#define LONG_NAME (1<<6)
#define SAME_TIME (1<<7)

/* update this if you make incompatible changes */
#define PROTOCOL_VERSION 26

/* We refuse to interoperate with versions that are not in this range.
 * Note that we assume we'll work with later versions: the onus is on
 * people writing them to make sure that they don't send us anything
 * we won't understand.
 *
 * There are two possible explanations for the limit at thirty: either
 * to allow new major-rev versions that do not interoperate with us,
 * and (more likely) so that we can detect an attempt to connect rsync
 * to a non-rsync server, which is unlikely to begin by sending a byte
 * between 15 and 30. */
#define MIN_PROTOCOL_VERSION 15
#define MAX_PROTOCOL_VERSION 30

#define RSYNC_PORT 873

#define SPARSE_WRITE_SIZE (1024)
#define WRITE_SIZE (32*1024)
#define CHUNK_SIZE (32*1024)
#define MAX_MAP_SIZE (256*1024)
#define IO_BUFFER_SIZE (4092)

#define MAX_ARGS 1000

#define BOOL int

#ifndef uchar
#define uchar unsigned char
#endif

#if HAVE_UNSIGNED_CHAR
#define schar signed char
#else
#define schar char
#endif

#ifndef int32
#if (SIZEOF_INT == 4)
#define int32 int
#elif (SIZEOF_LONG == 4)
#define int32 long
#elif (SIZEOF_SHORT == 4)
#define int32 short
#else
/* I hope this works */
#define int32 int
#define LARGE_INT32
#endif
#endif

#ifndef uint32
#define uint32 unsigned int32
#endif

#if HAVE_OFF64_T
#define OFF_T off64_t
#define STRUCT_STAT struct stat64
#else
#define OFF_T off_t
#define STRUCT_STAT struct stat
#endif

#if HAVE_OFF64_T
#define int64 off64_t
#elif (SIZEOF_LONG == 8) 
#define int64 long
#elif (SIZEOF_INT == 8) 
#define int64 int
#elif HAVE_LONGLONG
#define int64 long long
#else
/* As long as it gets... */
#define int64 off_t
#define NO_INT64
#endif

/* Starting from protocol version 26, we always use 64-bit
 * ino_t and dev_t internally, even if this platform does not
 * allow files to have 64-bit inums.  That's because the
 * receiver needs to find duplicate (dev,ino) tuples to detect
 * hardlinks, and it might have files coming from a platform
 * that has 64-bit inums.
 *
 * The only exception is if we're on a platform with no 64-bit type at
 * all.
 *
 * Because we use read_longint() to get these off the wire, if you
 * transfer devices or hardlinks with dev or inum > 2**32 to a machine
 * with no 64-bit types then you will get an overflow error.  Probably
 * not many people have that combination of machines, and you can
 * avoid it by not preserving hardlinks or not transferring device
 * nodes.  It's not clear that any other behaviour is better.
 *
 * Note that if you transfer devices from a 64-bit-devt machine (say,
 * Solaris) to a 32-bit-devt machine (say, Linux-2.2/x86) then the
 * device numbers will be truncated.  But it's a kind of silly thing
 * to do anyhow.
 *
 * FIXME: In future, we should probable split the device number into
 * major/minor, and transfer the two parts as 32-bit ints.  That gives
 * you somewhat more of a chance that they'll come from a big machine
 * to a little one in a useful way.
 *
 * FIXME: Really we need an unsigned type, and we perhaps ought to
 * cope with platforms on which this is an unsigned int or even a
 * struct.  Later.
 */ 
#define INO64_T int64
#define DEV64_T int64

#ifndef MIN
#define MIN(a,b) ((a)<(b)?(a):(b))
#endif

#ifndef MAX
#define MAX(a,b) ((a)>(b)?(a):(b))
#endif

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 256
#endif

/* the length of the md4 checksum */
#define MD4_SUM_LENGTH 16
#define SUM_LENGTH 16

#ifndef MAXPATHLEN
#define MAXPATHLEN 1024
#endif

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

//typedef double time_t;
//typedef double OFF_T;
//typedef uint32 mode_t;
//typedef uint32 uid_t;
//typedef uint32 gid_t;

#define STRUCT_STAT struct stat

struct file_struct {
	unsigned flags;
	time_t modtime;
	OFF_T length;
	mode_t mode;

	INO64_T inode;
	/** Device this file lives upon */
	DEV64_T dev;

	/** If this is a device node, the device number. */
	DEV64_T rdev;
	uid_t uid;
	gid_t gid;
	char *basename;
	char *dirname;
	char *basedir;
	char *link;
	char *sum;
        int dirnameAlloc;
};


#define ARENA_SIZE	(32 * 1024)

struct string_area {
	char *base;
	char *end;
	char *current;
	struct string_area *next;
};

struct file_list {
	int count;
	int malloced;
	struct file_struct **files;
        /*
         * Added for perl interface.
         */
        /* 
         * command-line options
         */
        int always_checksum;
        int remote_version;
        int preserve_uid;
        int preserve_gid;
        int preserve_devices;
        int preserve_links;
        int preserve_hard_links;
        /* 
         * incoming (decoded) string being processed
         */
        unsigned char *inBuf;
        uint32 inLen;
        uint32 inPosn;
        uint32 inFileStart;
        int inError;
        int decodeDone;
        int fatalError;
        /*
         * outgoing (encoded) string being generated
         */
        unsigned char *outBuf;
        uint32 outLen;
        uint32 outPosn;
        /*
         * state variables
         */
	time_t last_time;
	mode_t last_mode;
	DEV64_T last_rdev;
	uid_t last_uid;
	gid_t last_gid;
        char *lastdir;
        /*
         * Warning: perl.h might define MAXPATHLEN differently from
         * flist.h, so keep this at the end of the struct...
         */
	char lastname[MAXPATHLEN];
};

struct sum_buf {
	OFF_T offset;		/* offset in file of this chunk */
	int len;		/* length of chunk of file */
	int i;			/* index of this chunk */
	uint32 sum1;	        /* simple checksum */
	char sum2[SUM_LENGTH];	/* checksum  */
};

struct sum_struct {
	OFF_T flength;		/* total file length */
	size_t count;		/* how many chunks */
	size_t remainder;	/* flength % block_length */
	size_t n;		/* block_length */
	struct sum_buf *sums;	/* points to info for each chunk */
};

struct map_struct {
	char *p;
	int fd,p_size,p_len;
	OFF_T file_size, p_offset, p_fd_offset;
};

struct exclude_struct {
	char *pattern;
	int regular_exp;
	int fnmatch_flags;
	int include;
	int directory;
	int local;
};

struct stats {
	int64 total_size;
	int64 total_transferred_size;
	int64 total_written;
	int64 total_read;
	int64 literal_data;
	int64 matched_data;
	int flist_size;
	int num_files;
	int num_transferred_files;
};


/* we need this function because of the silly way in which duplicate
   entries are handled in the file lists - we can't change this
   without breaking existing versions */
static inline int flist_up(struct file_list *flist, int i)
{
	while (!flist->files[i]->basename) i++;
	return i;
}

#define SUPPORT_LINKS HAVE_READLINK
#define SUPPORT_HARD_LINKS HAVE_LINK

/* This could be bad on systems which have no lchown and where chown
 * follows symbollic links.  On such systems it might be better not to
 * try to chown symlinks at all. */
#ifndef HAVE_LCHOWN
#define lchown chown
#endif

#define SIGNAL_CAST (RETSIGTYPE (*)())

#ifndef EWOULDBLOCK
#define EWOULDBLOCK EAGAIN
#endif

#ifndef STDIN_FILENO
#define STDIN_FILENO 0
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO 1
#endif

#ifndef STDERR_FILENO
#define STDERR_FILENO 2
#endif

#ifndef S_IWUSR
#define S_IWUSR 0200
#endif

#ifndef _S_IFMT
#define _S_IFMT        0170000
#endif

#ifndef _S_IFDIR
#define _S_IFDIR 0x4000  /* directory */
#define _S_IFIFO 0x1000  /* FIFO special */
#define _S_IFCHR 0x2000  /* character special */
#define _S_IFBLK 0x3000  /* block special */
#define _S_IFREG 0x8000  /* or just 0x0000, regular */
#define _S_IREAD 0x0100  /* owner may read */
#define _S_IWRITE 0x0080 /* owner may write */
#define _S_IEXEC 0x0040  /* owner may execute <directory search> */
#endif

#ifndef _S_IFLNK
#define _S_IFLNK  0120000
#endif

#ifndef S_ISLNK
#define S_ISLNK(mode) (((mode) & (_S_IFMT)) == (_S_IFLNK))
#endif

#ifndef S_ISBLK
#define S_ISBLK(mode) (((mode) & (_S_IFMT)) == (_S_IFBLK))
#endif

#ifndef S_ISCHR
#define S_ISCHR(mode) (((mode) & (_S_IFMT)) == (_S_IFCHR))
#endif

#ifndef S_ISSOCK
#ifdef _S_IFSOCK
#define S_ISSOCK(mode) (((mode) & (_S_IFMT)) == (_S_IFSOCK))
#else
#define S_ISSOCK(mode) (0)
#endif
#endif

#ifndef S_ISFIFO
#ifdef _S_IFIFO
#define S_ISFIFO(mode) (((mode) & (_S_IFMT)) == (_S_IFIFO))
#else
#define S_ISFIFO(mode) (0)
#endif
#endif

#ifndef S_ISDIR
#define S_ISDIR(mode) (((mode) & (_S_IFMT)) == (_S_IFDIR))
#endif

#ifndef S_ISREG
#define S_ISREG(mode) (((mode) & (_S_IFMT)) == (_S_IFREG))
#endif

/* work out what fcntl flag to use for non-blocking */
#ifdef O_NONBLOCK
# define NONBLOCK_FLAG O_NONBLOCK
#elif defined(SYSV)
# define NONBLOCK_FLAG O_NDELAY
#else 
# define NONBLOCK_FLAG FNDELAY
#endif

#ifndef INADDR_LOOPBACK
#define INADDR_LOOPBACK 0x7f000001
#endif

#ifndef INADDR_NONE
#define INADDR_NONE 0xffffffff
#endif

#define IS_DEVICE(mode) (S_ISCHR(mode) || S_ISBLK(mode) || S_ISSOCK(mode) || S_ISFIFO(mode))

#ifndef ACCESSPERMS
#define ACCESSPERMS 0777
#endif
/* Initial mask on permissions given to temporary files.  Mask off setuid
     bits and group access because of potential race-condition security
     holes, and mask other access because mode 707 is bizarre */
#define INITACCESSPERMS 0700

/* handler for null strings in printf format */
#define NS(s) ((s)?(s):"<NULL>")

#if !defined(__GNUC__) || defined(APPLE)
/* Apparently the OS X port of gcc gags on __attribute__.
 *
 * <http://www.opensource.apple.com/bugs/X/gcc/2512150.html> */
#define __attribute__(x) 

#endif

#ifndef HAVE_STRLCPY
size_t strlcpy(char *d, const char *s, size_t bufsize);
#endif

#ifndef HAVE_STRLCAT
size_t strlcat(char *d, const char *s, size_t bufsize);
#endif

#ifndef WEXITSTATUS
#define	WEXITSTATUS(stat)	((int)(((stat)>>8)&0xFF))
#endif

#define exit_cleanup(code) _exit_cleanup(code, __FILE__, __LINE__)


extern int verbose;

#ifndef HAVE_INET_NTOP
const char *                 
inet_ntop(int af, const void *src, char *dst, size_t size);
#endif /* !HAVE_INET_NTOP */

#ifndef HAVE_INET_PTON
int isc_net_pton(int af, const char *src, void *dst);
#endif

#define UNUSED(x) x __attribute__((__unused__))


extern struct file_list *flist_new(void);
extern void flist_free(struct file_list *flist);
extern void receive_file_entry(struct file_list *f, struct file_struct **fptr,
			       unsigned flags);
extern int flistDecodeBytes(struct file_list *f,
                            unsigned char *bytes, uint32 nBytes);
extern void clean_flist(struct file_list *flist, int strip_root);
extern void send_file_entry(struct file_list *f, struct file_struct *file);
extern char *f_name(struct file_struct *f);
