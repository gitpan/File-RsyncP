#!/bin/perl

BEGIN {print "1..6\n";}
END {print "not ok 1\n" unless $loaded;}
use File::RsyncP::FileList;
$loaded = 1;
print "ok 1\n";

my $fList = File::RsyncP::FileList->new({
                preserve_uid        => 1,       # --owner
                preserve_gid        => 1,       # --group
                preserve_links      => 1,       # --links
                preserve_devices    => 1,       # --devices
                preserve_hard_links => 1,       # --hard-links
                always_checksum     => 0,       # --checksum
                remote_version      => 26,      # remote protocol version
            });

my @TestFiles = (
    {
        name  => "xxx/yyy/aaa1",
        dev   => 123,
        inode => 123456,
        mode  => 0100755,
        uid   => 987,
        gid   => 654,
        rdev  => 0x1234,
        size  => 654321,
        mtime => time,
    },
    {
        name  => "xxx/yyy/aaa2",
        dev   => 123,
        inode => 123456,
        mode  => 0100755,
        uid   => 987,
        gid   => 654,
        rdev  => 0x1234,
        size  => 654321,
        mtime => time,
    },
    {
        name  => "xxx/zzz/bbb1",
        dev   => 9123,
        inode => 9123456,
        mode  => 0100666,
        uid   => 9876,
        gid   => 6543,
        rdev  => 0x12345,
        size  => 65432,
        mtime => time + 1,
    },
    {
        name  => "xxx/yyy/aaa3",
        dev   => 9123,
        inode => 9123456,
        mode  => 0100666,
        uid   => 9876,
        gid   => 6543,
        rdev  => 0x12345,
        size  => 65432,
        mtime => time + 1,
    },
    {
        name  => "xxx/zzz/bbb2",
        dev   => 9123,
        inode => 9123456,
        mode  => 0100666,
        uid   => 9876,
        gid   => 6543,
        rdev  => 0x12345,
        size  => 65432,
        mtime => time + 1,
    },
    {
        name  => "xxx/zzz/bbb6",
        dev   => (1 << 31) * 123 + (5432 << 18),
        inode => (1 << 31) * 12  + (6543 << 17),,
        mode  => 0100666,
        uid   => 9876,
        gid   => 6543,
        rdev  => 0x12345,
        size  => (1 << 31) * 3 + (1 << 29), 
        mtime => time + 1,
    },
);

for ( my $i = 0 ; $i < @TestFiles ; $i++ ) {
    $fList->encode($TestFiles[$i]);
}
if ( $fList->count == 6 ) {
    print("ok 2\n");
} else {
    print("not ok 2\n");
}

my $ok = 1;
for ( my $i = 0 ; $i < @TestFiles ; $i++ ) {
    my $f = $fList->get($i);
    foreach my $k ( keys(%{$TestFiles[$i]}) ) {
        if ( $TestFiles[$i]{$k} ne $f->{$k} ) {
            #print("$i.$k: $TestFiles[$i]{$k} vs $f->{$k}\n");
            $ok = 0
        }
    }
}
if ( $ok ) {
    print("ok 3\n");
} else {
    print("not ok 3\n");
}

my $data = $fList->encodeData . pack("C", 0);
#print("data = ", unpack("H*", $data), "\n");
my $fList2 = File::RsyncP::FileList->new({
                preserve_uid        => 1,       # --owner
                preserve_gid        => 1,       # --group
                preserve_links      => 1,       # --links
                preserve_devices    => 1,       # --devices
                preserve_hard_links => 1,       # --hard-links
                always_checksum     => 0,       # --checksum
                remote_version      => 26,      # remote protocol version
            });
my $bytesDone = $fList2->decode($data);

if ( $bytesDone == length($data) ) {
    print("ok 4\n");
} else {
    print("not ok 4\n");
}
$ok = 1;
for ( my $i = 0 ; $i < @TestFiles ; $i++ ) {
    my $f = $fList2->get($i);
    foreach my $k ( keys(%{$TestFiles[$i]}) ) {
        next if ( $k eq "rdev" );
        if ( $TestFiles[$i]{$k} ne $f->{$k} ) {
            #print("$i.$k: $TestFiles[$i]{$k} vs $f->{$k}\n");
            $ok = 0
        }
    }
}
if ( $ok ) {
    print("ok 5\n");
} else {
    print("not ok 5\n");
}

$fList->clean;
$fList2->clean;
$ok = 1;
for ( my $i = 0 ; $i < $fList2->count ; $i++ ) {
    my $f2 = $fList2->get($i);
    my $f = $fList->get($i);
    foreach my $k ( keys(%$f2) ) {
        next if ( $k eq "rdev" );
        if ( $f2->{$k} ne $f->{$k} ) {
            #print("$i.$k: $f2->{$k} vs $f->{$k}\n");
            $ok = 0
        }
    }
}
if ( $ok ) {
    print("ok 6\n");
} else {
    print("not ok 6\n");
}
