#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"
#include "string.h"
#include "flist.h"

typedef struct file_list *File__RsyncP__FileList;

/*
 * Pick an integer setting out of the hash ref opts.  If the argument
 * isn't a hash, or doesn't contain param, then returns def.
 */
static int getHashInt(SV *opts, char *param, int def)
{
    SV **vp;

    if ( !opts || !SvROK(opts)
               || SvTYPE(SvRV(opts)) != SVt_PVHV
               || !(vp = hv_fetch((HV*)SvRV(opts), param, strlen(param), 0))
               || !*vp ) {
        return def;
    }
    return SvIV(*vp);
}

/*
 * Pick an unsigned integer setting out of the hash ref opts.  If the
 * argument isn't a hash, or doesn't contain param, then returns def.
 */
static unsigned int getHashUInt(SV *opts, char *param, int def)
{
    SV **vp;

    if ( !opts || !SvROK(opts)
               || SvTYPE(SvRV(opts)) != SVt_PVHV
               || !(vp = hv_fetch((HV*)SvRV(opts), param, strlen(param), 0))
               || !*vp ) {
        return def;
    }
    return SvUV(*vp);
}

/*
 * Pick a string setting out of the hash ref opts.  If the argument
 * isn't a hash, or doesn't contain param, then returns def.
 */
static int getHashString(SV *opts, char *param, char *def,
                          char *result, int maxLen)
{
    SV **vp;
    char *str;
    int len;

    if ( !opts || !SvROK(opts)
               || SvTYPE(SvRV(opts)) != SVt_PVHV
               || !(vp = hv_fetch((HV*)SvRV(opts), param, strlen(param), 0))
               || !*vp ) {
        if ( !def )
            return -1;
        strcpy(result, def);
        return 0;
    } else {
        str = (char*)SvPV(*vp, len);
        if ( len >= maxLen ) {
            return -1;
        }
        memcpy(result, str, len);
        result[len] = '\0';
    }
    return 0;
}

/*
 * Pick a double setting out of the hash ref opts.  If the argument
 * isn't a hash, or doesn't contain param, then returns def.
 */
static double getHashDouble(SV *opts, char *param, double def)
{
    SV **vp;

    if ( !opts || !SvROK(opts)
               || SvTYPE(SvRV(opts)) != SVt_PVHV
               || !(vp = hv_fetch((HV*)SvRV(opts), param, strlen(param), 0))
               || !*vp ) {
        return def;
    }
    return SvNV(*vp);
}


MODULE = File::RsyncP::FileList		PACKAGE = File::RsyncP::FileList		

PROTOTYPES: DISABLE

File::RsyncP::FileList
new(packname = "File::RsyncP::FileList", SV* opts = NULL)
	char *packname
    CODE:
    {
        RETVAL = flist_new();
        RETVAL->preserve_links   = getHashInt(opts, "preserve_links", 1);
        RETVAL->preserve_uid     = getHashInt(opts, "preserve_uid",   1);
        RETVAL->preserve_gid     = getHashInt(opts, "preserve_gid",   1);
        RETVAL->preserve_devices = getHashInt(opts, "preserve_devices", 0);
        RETVAL->always_checksum  = getHashInt(opts, "always_checksum", 0);
        RETVAL->preserve_hard_links = getHashInt(opts, "preserve_hard_links",
                                                       0);
        RETVAL->remote_version   = getHashInt(opts, "remote_version", 26);
    }
    OUTPUT:
	RETVAL

void
DESTROY(flist)
	File::RsyncP::FileList	flist
    CODE:
    {
        flist_free(flist);
    }

unsigned int
count(flist)
	File::RsyncP::FileList	flist
    CODE:
    {
        RETVAL = flist->count;
    }
    OUTPUT:
        RETVAL

unsigned int
fatalError(flist)
	File::RsyncP::FileList	flist
    CODE:
    {
        RETVAL = flist->fatalError;
    }
    OUTPUT:
        RETVAL

unsigned int
decodeDone(flist)
	File::RsyncP::FileList	flist
    CODE:
    {
        RETVAL = flist->decodeDone;
    }
    OUTPUT:
        RETVAL

int
decode(flist, bytesSV)
    PREINIT:
	STRLEN nBytes;
    INPUT:
	File::RsyncP::FileList	flist
	SV *bytesSV
	unsigned char *bytes = (unsigned char *)SvPV(bytesSV, nBytes);
    CODE:
    {
        RETVAL = flistDecodeBytes(flist, bytes, nBytes);
    }
    OUTPUT:
        RETVAL

SV*
get(flist, index)
    INPUT:
	File::RsyncP::FileList	flist
        unsigned int index
    CODE:
    {
        HV *rh;
        struct file_struct *file;

        if ( index >= flist->count ) {
            XSRETURN_UNDEF; 
        }
        file = flist->files[index];
        rh = (HV *)sv_2mortal((SV *)newHV());
        if ( file->basename )
            hv_store(rh, "basename", 8, newSVpv(file->basename, 0), 0);
        if ( file->dirname )
            hv_store(rh, "dirname",  7, newSVpv(file->dirname, 0), 0);
        if ( file->link )
            hv_store(rh, "link",     4, newSVpv(file->link, 0), 0);
        if ( file->sum )
            hv_store(rh, "sum",      3, newSVpv(file->sum, 0), 0);
        hv_store(rh, "name",    4, newSVpv(f_name(file), 0), 0);
        hv_store(rh, "uid",     3, newSVuv(file->uid), 0);
        hv_store(rh, "gid",     3, newSVuv(file->gid), 0);
        hv_store(rh, "mode",    4, newSVuv(file->mode), 0);
        hv_store(rh, "mtime",   5, newSVuv(file->modtime), 0);
        hv_store(rh, "size",    4, newSVnv(file->length), 0);
        hv_store(rh, "dev",     3, newSVnv(file->dev), 0);
        hv_store(rh, "inode",   5, newSVnv(file->inode), 0);
        hv_store(rh, "rdev",    4, newSVuv(file->rdev), 0);
        RETVAL = newRV((SV*)rh);
    }
    OUTPUT:
        RETVAL

void
clean(flist)
    INPUT:
	File::RsyncP::FileList	flist
    CODE:
    {
        clean_flist(flist, 0);
    }

void
encode(flist, SV* data)
    INPUT:
	File::RsyncP::FileList	flist
    CODE:
    {
        struct file_struct file, *fileCopy;
        char name[MAXPATHLEN];
        char linkbuf[MAXPATHLEN];
        char *p;
        int gotLink = 0;

        memset(&file, 0, sizeof(file));
        if ( getHashString(data, "name", NULL, name, MAXPATHLEN-1) ) {
            printf("flist encode: empty or too long name\n");
            return;
        }
        clean_fname(name);
        if ( !getHashString(data, "link", NULL, linkbuf, MAXPATHLEN-1) ) {
            gotLink = 1;
        }
	if ((p = strrchr(name, '/'))) {
            *p = 0;
            if ( flist->lastdir && strcmp(name, flist->lastdir) == 0 ) {
                file.dirname = flist->lastdir;
                file.dirnameAlloc = 0;
            } else {
                file.dirname = strdup(name);
                flist->lastdir = file.dirname;
                file.dirnameAlloc = 1;
            }
            file.basename = strdup(p + 1);
            *p = '/';
	} else {
            file.dirname = NULL;
            file.basename = strdup(name);
	}

	file.modtime = getHashUInt(data, "mtime", 0);
	file.length  = getHashDouble(data, "size", 0.0);
	file.mode    = getHashUInt(data, "mode", 0);
	file.uid     = getHashUInt(data, "uid", 0);
	file.gid     = getHashUInt(data, "gid", 0);
	file.dev     = getHashDouble(data, "dev", 0.0);
	file.inode   = getHashDouble(data, "inode", 0.0);
	file.rdev    = getHashUInt(data, "rdev", 0);
	if ( gotLink ) {
            file.link = strdup(linkbuf);
	}
	if ( flist->always_checksum ) {
            char sum[MAXPATHLEN];
            if ( !getHashString(data, "sum", NULL, sum, MAXPATHLEN-1) ) {
                printf("flist encode: missing sum with always_checksum\n");
                return;
            }
            file.sum = (char *) malloc(MD4_SUM_LENGTH);
            memcpy(file.sum, sum, MD4_SUM_LENGTH);
	}
        flist_expand(flist);
        if ( strcmp(file.basename, "") ) {
            fileCopy = (struct file_struct*)malloc(sizeof(*fileCopy));
            *fileCopy = file;
            flist->files[flist->count++] = fileCopy;
            send_file_entry(flist, fileCopy);
        }
    }

SV*
encodeData(flist)
    INPUT:
	File::RsyncP::FileList	flist
    CODE:
    {
        if ( !flist->outBuf || flist->outPosn == 0 ) {
            ST(0) = sv_2mortal(newSVpv("", 0));
        } else {
            ST(0) = sv_2mortal(newSVpv((char*)flist->outBuf, flist->outPosn));
            flist->outPosn = 0;
        }
    }
