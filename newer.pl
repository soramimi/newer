#!/usr/bin/env perl
use strict;
use warnings;
use Getopt::Long;
use File::Temp ();
use File::Basename qw(dirname);
use File::Spec;

my ($verbose, $dry_run, $atomic, $help);
GetOptions(
    'v|verbose' => \$verbose,
    'n|dry-run' => \$dry_run,
    'a|atomic'  => \$atomic,
    'h|help'    => \$help,
) or do { usage(); exit 1 };

if ($help) {
    usage();
    exit 0;
}

if (@ARGV != 2) {
    usage();
    exit 1;
}

my ($left, $right) = @ARGV;

my @st1 = stat($left);
my @st2 = stat($right);
my $exists1 = scalar @st1 != 0;
my $exists2 = scalar @st2 != 0;

if (!$exists1 && !$exists2) {
    print STDERR "error: both files do not exist\n";
    exit 1;
}

if ($exists1 && $exists2 && $st1[0] == $st2[0] && $st1[1] == $st2[1]) {
    print STDERR "error: same file\n";
    exit 1;
}

my ($src, $dst, $src_stat);
if (!$exists1) {
    $src = $right;
    $src_stat = \@st2;
    $dst = $left;
    notice("$src -> $dst (left missing)\n");
} elsif (!$exists2) {
    $src = $left;
    $src_stat = \@st1;
    $dst = $right;
    notice("$src -> $dst (right missing)\n");
} elsif ($st1[9] > $st2[9]) {
    $src = $left;
    $src_stat = \@st1;
    $dst = $right;
    notice("$src -> $dst (newer)\n");
} elsif ($st1[9] < $st2[9]) {
    $src = $right;
    $src_stat = \@st2;
    $dst = $left;
    notice("$src -> $dst (newer)\n");
} else {
    notice("same timestamp, nothing to do\n") if $verbose;
    exit 0;
}

exit 0 if $dry_run;

if ($atomic) {
    atomic_copy($src, $dst, $src_stat);
} else {
    simple_copy($src, $dst, $src_stat);
}

exit 0;

sub usage {
    print <<"EOF";
usage: $0 [options] <left> <right>
Options:
  -v, --verbose    Print detailed progress
  -n, --dry-run    Show what would happen without copying
  -a, --atomic     Use atomic write (write to temp file, then rename)
  -h, --help       Show this help message
EOF
}

sub notice {
    my ($msg) = @_;
    return unless $verbose || $dry_run;
    print $msg;
}

sub do_copy {
    my ($src_path, $dst_fh) = @_;
    open my $src_fh, '<', $src_path or die "open $src_path: $!";
    binmode $src_fh;
    binmode $dst_fh;
    while (sysread($src_fh, my $buf, 65536)) {
        syswrite($dst_fh, $buf) or die "write: $!";
    }
    close $src_fh;
}

sub set_metadata {
    my ($path_or_fh, $st) = @_;
    my $mode = $st->[2] & 07777;
    my $uid  = $st->[4];
    my $gid  = $st->[5];
    my $atime = $st->[8];
    my $mtime = $st->[9];

    chmod $mode, $path_or_fh or warn "chmod: $!";
    chown $uid, $gid, $path_or_fh;
    # utime on a handle requires perl 5.8.9+ and a file name works on all.
    # We use the path here for broad compatibility.
    my $p = ref($path_or_fh) eq 'GLOB' ? undef : $path_or_fh;
    if (defined $p) {
        utime $atime, $mtime, $p or warn "utime: $!";
    }
}

sub simple_copy {
    my ($src_path, $dst_path, $st) = @_;
    open my $dst_fh, '>', $dst_path or die "open $dst_path: $!";
    do_copy($src_path, $dst_fh);
    close $dst_fh;
    set_metadata($dst_path, $st);
}

sub atomic_copy {
    my ($src_path, $dst_path, $st) = @_;
    my $dir = File::Spec->rel2abs(dirname($dst_path));
    my $tmp = File::Temp->new(
        TEMPLATE => File::Spec->catfile($dir, ".newer_XXXXXX"),
        UNLINK   => 0,
    );
    do_copy($src_path, $tmp);
    close $tmp or die "close temp: $!";

    # set metadata by path before rename (safer if rename fails after chmod)
    set_metadata($tmp->filename, $st);

    rename $tmp->filename, $dst_path or die "rename $dst_path: $!";
    $tmp->unlink_on_destroy(0);
}
