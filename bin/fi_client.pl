#!/usr/bin/env perl

# Copyright (c) 2014 Fastly, Inc.
# All rights reserved.
#
# Author: Devon H. O'Dell <dho@fastly.com>
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.

use strict;
use warnings;

use Getopt::Long;
use IO::File;
use IO::Socket::INET;
use Pod::Usage;

my $infile = 'out.fi';
my $addr = '127.0.0.1:28806';
my $help = 0;
my $man = 0;
my $verbose = 0;

GetOptions(
	'infile=s'	=> \$infile,
	'addr=s'	=> \$addr,
	'help|?'	=> \$help,
	'man'		=> \$man,
	'verbose'	=> \$verbose,
) or pod2usage(2);
pod2usage(1) if $help;
pod2usage(-exitval => 0, -verbose => 2) if $man;

local $/;
my $infd = new IO::File "< $infile";
$infd->binmode();
my $data = <$infd>;

my $i = 0;
test:
sleep 1;
my $s = IO::Socket::INET->new(
	PeerAddr	=> $addr,
	Proto		=> 'tcp',
);
goto test if !defined $s and $i++ < 10;
die "Couldn't connect" if !defined $s;

# Say hello.
print $s "hei\x00\x00";
$s->flush();

my $joo = "";
$s->recv($joo, 2);
die "Hei -> ei" if ($joo eq "ei");
$s->recv($joo, 1);

print $s "ota se";
print $s pack("NN", length($data), 0);
$s->flush();
print $s $data;
$s->flush();
print $s "loppu";
$s->flush();

$s->recv($joo, 3);
die "Bad joo" if ($joo ne "joo");

print $s "hei hei";
my $r;
$s->recv($r, 7);
$s->close();

__END__

=head1 NAME

fi_client - Finnish-speaking client for Räikkönen

=head1 SYNOPSIS

fi_client [options]

 Options:
   --addr, -a		Address of Räikkönen scheduler
   --infile, -i		Bytecode to send
   --help		Short help message
   --man		Full documentation

=head1 OPTIONS

=over 8

=item B<--addr>, B<-a>

TCP address of the listener for the server. Specified in full IP:Port form.

=item B<--infile>, B<-i>

Path to Finnish bytecode to send to the server. Defaults to a file called
'out.fi' in the current directory.

=item B<--help> and B<--man>

If you need help for these options, you need more help for other things.

=back

=head1 DESCRIPTION

Finnish isn't meant to be understood by humans. That's why you're using this
program.

=head1 AUTHOR

Devon H. O'Dell <dho@fastly.com>

=cut
