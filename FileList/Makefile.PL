use ExtUtils::MakeMaker;
# See lib/ExtUtils/MakeMaker.pm for details of how to influence
# the contents of the Makefile that is written.
WriteMakefile(
    'NAME'	    => 'File::RsyncP::FileList',
    'VERSION_FROM'  => 'FileList.pm', # finds $VERSION
    'LIBS'	    => ['-lm'], # e.g., '-lm'
    'DEFINE'	    => '-DPERL_BYTEORDER=$(BYTEORDER)',
    'INC'	    => '',     # e.g., '-I/usr/include/other' 
    'OBJECT'	    => q[FileList$(OBJ_EXT) flist$(OBJ_EXT)],
);
