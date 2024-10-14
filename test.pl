#!/usr/bin/env perl
#
# input list of files
# -r <filename> to record the output of the file
#
# files expected to be:
# params: ...
# exitcode: ...
# code:
# ...
#
# optionally the output can be provided at the end
# with the 'output:' field or it can be recorded
#
# TODO: collect and compare stderr
# TODO: not require the exitcode field in the unrecorded file

use strict;
use warnings;
use autodie;

my $tests = 0;
my @fails = ();
my $i     = 0;
while ( $i <= $#ARGV ) {
    $tests += 1;
    $_ = $ARGV[$i];

    my $record = 0;
    if ( $_ eq "-r" ) {
        $record = 1;
        $i += 1;
        $_ = $ARGV[$i];
    }

    print "TEST $_ ... ";
    open( my $file, $_ );

    my $params = <$file>;
    unless ( $params =~ s/^params:\s*(.*)\s*$/$1/ ) {
        print "\nERROR:$_:1: expected params";
        exit 1;
    }
    my $exitcode = <$file>;
    unless ( $exitcode =~ s/^exitcode:\s*(.*)\s*$/$1/ ) {
        print "\nERROR:$_:2: expected exitcode";
        exit 1;
    }
    unless ( <$file> =~ m/^code:\s*$/ ) {
        print "\nERROR:$_:3: expected code";
        exit 1;
    }
    my $code = "";
    my $line = "";
    until ( eof($file) or ( $line = <$file> ) =~ m/^output:\s*$/ ) {
        $code .= $line;
    }
    my $expected = "";
    while ( $line = <$file> ) {
        $expected .= $line;
    }
    close $file;

    my $escapedcode = $code;
    $escapedcode =~ s/\"/\\"/g;
    my $output         = `./simpleC $params -e "$escapedcode" 2>&1`;
    my $actualexitcode = $?;

    if ($record) {
        open( my $file, ">", $_ );
        print $file "params: $params\n";
        print $file "exitcode: $actualexitcode\n";
        print $file "code:\n";
        print $file $code;
        print $file "output:\n";
        print $file $output;
        close $file;
        print "RECORDED\n";
    }
    else {
        my $ok = 1;
        unless ( $expected eq $output ) {
            $ok = 0;
            print "\nOUTPUTS DIFFER\n";
            open( my $file, ">", $_ . ".expected" );
            print $file $expected;
            close $file;
            open( $file, ">", $_ . ".output" );
            print $file $output;
            close $file;
            system( "diff", "-y", "-d", "$_.expected", "$_.output" );
            `rm $_.expected $_.output`;
        }
        unless ( $exitcode eq $actualexitcode ) {
            $ok = 0;
            print "\nEXPECTED EXIT CODE $exitcode, FOUND $actualexitcode\n";
        }
        if ($ok) {
            print "OK\n";
        }
        else {
            push( @fails, $_ );
        }
    }

    $i += 1;
}

print "TESTED $tests FILES, " . ( $#fails + 1 ) . " FAILED\n";
for (@fails) {
    print "FAILED: $_\n";
}

