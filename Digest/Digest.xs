/*
**	Perl Extension for the
**
**	RSA Data Security Inc. MD4 Message-Digest Algorithm
**
**	This module originally by Neil Winton (N.Winton@axion.bt.co.uk)
**      Adapted by Mike McCauley mikem@open.com.au
**	$Id: MD4.xs,v 1.3 2001/07/30 23:39:57 mikem Exp $	
**
**	This extension may be distributed under the same terms
**	as Perl. The MD4 code is covered by separate copyright and
**	licence, but this does not prohibit distribution under the
**	GNU or Artistic licences. See the file md4c.c or MD4.pm
**	for more details.
*/

#ifdef __cplusplus
extern "C" {
#endif
#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "global.h"
#include "md4.h"

typedef MD4_CTX	*File__RsyncP__Digest;

#ifdef __cplusplus
}
#endif


MODULE = File::RsyncP::Digest		PACKAGE = File::RsyncP::Digest

PROTOTYPES: DISABLE

File::RsyncP::Digest
new(packname = "File::RsyncP::Digest")
	char *		packname
    CODE:
	{
	    RETVAL = (MD4_CTX *)safemalloc(sizeof(MD4_CTX));
	    MD4Init(RETVAL);
	}
    OUTPUT:
	RETVAL

void
DESTROY(context)
	File::RsyncP::Digest	context
    CODE:
	{
	    safefree((char *)context);
	}


File::RsyncP::Digest
reset(context)
	File::RsyncP::Digest	context
    CODE:
	{
	    MD4Init(context);
	    RETVAL = context;
	}

File::RsyncP::Digest
add(context, ...)
	File::RsyncP::Digest	context
    CODE:
	{
	    SV *svdata;
	    STRLEN len;
	    unsigned char *data;
	    int i;

	    for (i = 1; i < items; i++)
	    {
		data = (unsigned char *)(SvPV(ST(i), len));
		MD4Update(context, data, len);
	    }
	    RETVAL = context;
	}

SV *
digest(context)
	File::RsyncP::Digest	context
    CODE:
	{
	    unsigned char digeststr[16];

	    MD4FinalRsync(digeststr, context);
	    ST(0) = sv_2mortal(newSVpv((char *)digeststr, 16));
	}

SV *
blockDigest(context, dataV, blockSize=700, md4DigestLen=16, seed=0)
    PREINIT:
	STRLEN len;
    INPUT:
	File::RsyncP::Digest	context
	SV *dataV
	unsigned char *data = SvPV(dataV, len);
	size_t blockSize
	int md4DigestLen
	unsigned int seed
    CODE:
	{
	    UINT4 digestSize;
	    unsigned char *digest;
	    extern rsync_checksum(char *buf, UINT4 len, UINT4 blockSize,
		    UINT4 seed, unsigned char *digest, int md4DigestLen);

	    if ( blockSize == 0 ) blockSize = 700;
            if ( md4DigestLen < 0 ) {
                /* 
                 * special case: save the entire MD4 state, so it can
                 * be cached.  That's 4+16=20 bytes per block, plus the
                 * used part of the 64 byte buffer.
                 */
                int nBlocks = (len + blockSize - 1) / blockSize;
                digestSize = 20 * nBlocks
                           + (nBlocks > 1 ? (blockSize % 64) * (nBlocks - 1)
                                          : 0)
                           + ((len % blockSize) % 64);
            } else {
                digestSize = (4 + (md4DigestLen > 16 ? 16 : md4DigestLen))
				* ((len + blockSize - 1) / blockSize);
            }
	    digest = safemalloc(1 + digestSize);
	    rsync_checksum(data, len, blockSize, seed, digest, md4DigestLen);
	    ST(0) = sv_2mortal(newSVpv((char *)digest, digestSize));
	    safefree(digest);
	}

SV *
blockDigestUpdate(context, dataV, blockSize=700, blockLastLen=0, \
		    md4DigestLen=16, seed=0)
    PREINIT:
	STRLEN len;
    INPUT:
	File::RsyncP::Digest	context
	SV *dataV
	unsigned char *data = SvPV(dataV, len);
	size_t blockSize
	size_t blockLastLen
	int md4DigestLen
	unsigned int seed
    CODE:
	{
	    UINT4 digestSize, blockCnt;
	    unsigned char *digest;
	    extern void rsync_checksum_update(char *digestIn, UINT4 blockCnt,
	        UINT4 blockSize, UINT4 blockLastLen,
		UINT4 seed, unsigned char *digestOut, int md4DigestLen);

	    if ( blockSize == 0 ) blockSize = 700;
            /*
             * There are 20 + (blockSize % 64) bytes per block,
             * plus 20 + (blockLastLen % 64) bytes for the last block.
             */
	    blockCnt = 1 + (len - (20 + (blockLastLen % 64)))
                                    / (20 + (blockSize % 64));
            if ( len == 0 || len != 20 * blockCnt
                           + (blockCnt > 1 ? (blockSize % 64) * (blockCnt - 1)
                                          : 0)
                           + (blockLastLen % 64) ) {
                /* TODO: provide a decent error message */
                printf("len = %d is wrong\n", len);
                blockCnt = 0;
            }
	    if ( md4DigestLen > 16 || md4DigestLen < 0 ) md4DigestLen = 16;
	    digestSize = (4 + md4DigestLen) * blockCnt;
	    digest = safemalloc(1 + digestSize);
	    rsync_checksum_update(data, blockCnt, blockSize, 
		    blockLastLen, seed, digest, md4DigestLen);
	    ST(0) = sv_2mortal(newSVpv((char *)digest, digestSize));
	    safefree(digest);
	}

SV *
blockDigestExtract(context, dataV, md4DigestLen=16)
    PREINIT:
	STRLEN len;
    INPUT:
	File::RsyncP::Digest	context
	SV *dataV
	unsigned char *data = SvPV(dataV, len);
	int md4DigestLen
    CODE:
	{
	    unsigned char *digest, *p;
            UINT4 blockCnt = len / 20;
            UINT4 digestSize;

            if ( md4DigestLen < 0 || md4DigestLen > 16 ) {
                md4DigestLen = 16;
            }
	    digestSize = (4 + md4DigestLen) * blockCnt;
	    p = digest = safemalloc(1 + digestSize);
            while ( blockCnt-- > 0 ) {
                memcpy(p, data, 4);
                p += 4;
                data += 4;
                memcpy(p, data, md4DigestLen);
                p += md4DigestLen;
                data += 16;
            }
	    ST(0) = sv_2mortal(newSVpv((char *)digest, digestSize));
	    safefree(digest);
	}
