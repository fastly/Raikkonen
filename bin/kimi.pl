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
use Pod::Usage;

use constant {
	STATE_FIND_TIMESLICE	=> 0,
	STATE_FIND_COMMAND	=> 1,
	STATE_WHEN_BODY		=> 2,

	N_VALUE			=> 4294967295,
};

my $infile = 'in.km';
my $outfile = 'out.fi';

my $help = 0;
my $man = 0;
my $verbose = 0;

GetOptions(
	'infile=s'	=> \$infile,
	'outfile=s'	=> \$outfile,
	'help|?'	=> \$help,
	'man'		=> \$man,
	'verbose'	=> \$verbose,
) or pod2usage(2);
pod2usage(1) if $help;
pod2usage(-exitval => 0, -verbose => 2) if $man;

sub parse_file {
	my $parse_state = STATE_FIND_TIMESLICE;
	my $states = {};

	my $infd = new IO::File "< $infile";
	die "Could not open $infile" if (!defined $infd);

	my $outfd = new IO::File "> $outfile";
	die "Could not open $outfile" if (!defined $outfd);

	my $lineno = 0;

	# This refers to the state described by the Kimi script.
	my $curstate = undef;

	while (<$infd>) {
		$lineno++;

		print $_ if $verbose;

		# Skip blank lines and comment-only lines
		if (m/^\s*(#|$)/) {
			print " --> Handling: skip blank / comment\n" if $verbose;
			next;
		}

		if ($parse_state == STATE_FIND_TIMESLICE || $parse_state == STATE_FIND_COMMAND) {
			if (m/^\s*t(n)?\[(\d+)\]\s*(#|$)/) {
				print " --> Handling: new timeslice\n" if $verbose;
				write_timeslice($outfd, $1, $2);
				$parse_state = STATE_FIND_COMMAND;
				next;
			} elsif (m/^\s*define\s+(\w+)\s+(\d+)\s*(#|$)/) {
				print " --> Handling: define state / callback\n" if $verbose;
				$states->{$1} = {
					id	=> $2,
					maxtid	=> 0,
					ranges	=> {},
				};
				next;
			} else {
				die "Invalid input '$_' looking for timeslice at line $lineno" if ($parse_state == STATE_FIND_TIMESLICE);
			}
		} 
		
		if ($parse_state == STATE_FIND_COMMAND) {
			if (m/^\s*define\s+(\w+)\s+(\d+)\s*(#|$)/) {
				print " --> Handling: define state / callback\n" if $verbose;
				$states->{$1} = {
					id	=> $2,
					maxtid	=> 0,
					ranges	=> {},
				};
				next;
			} elsif (m/^\s*resume\s+(\w+)\[(\d+-\d+|\d+|N)\]\s*(#|$)/) {
				die "Undefined state: $1 on line $lineno" if (!defined $states->{$1});

				my ($start, $end) = get_range($states->{$1}, $2);
				die "Invalid range: $2 on line $lineno" if (!defined $states->{$1}->{'ranges'}->{"$start-$end"});

				print " --> Handling: resume command\n" if $verbose;
				write_resume($outfd, $states->{$1}->{'id'}, $start, $end);
				next;
			} elsif (m/^\s*timeout\s+(\d+)(s|ms|μs|us|ns)?\s*(#|$)/) {
				# Default to seconds if no time specification was there.
				my $spec = $2 || "s";

				print " --> Handling: timeout command\n" if $verbose;
				write_timeout($outfd, $1, $spec);
				next;
			} elsif (m/^\s*waitstate\s*(#|$)/) {
				print " --> Handling: waitstate\n" if $verbose;
				write_waitstate($outfd);
				next;
			} elsif (m/^\s*when\s+(\w+)\s*(#|$)/) {
				die "Undefined state: $1 on line $lineno" if (!defined $states->{$1}->{'id'});
				print " --> Handling: when prologue\n" if $verbose;
				write_when_prologue($outfd, $states->{$1}->{'id'});
				$curstate = $states->{$1};
				$parse_state = STATE_WHEN_BODY;
				next;
			} else {
				die "Could not understand '$_' on line $lineno";
			}
		}
		
		if ($parse_state == STATE_WHEN_BODY) {
			die "State machine error: no when state found in when body on line $lineno" if !defined $curstate;
			if (m/^\s*(N|\d+-\d+|\d+):\s*(callback|continue|panic|sleep|wait)\s+(.*?)\s*(#|$)/) {
				# If we already saw a range to N, we must find an "end" marker next.
				die "Invalid range specification '$_' on line $lineno" if ($curstate->{'maxtid'} == N_VALUE);

				my ($start, $end) = get_range($curstate, $1);
				die "Invalid range specification $start-$end at line $lineno" if $start > $end;

				if ($curstate->{'maxtid'} == 0) {
					$curstate->{'maxtid'} = $end;
				} else {
					# We only care to check this if this isn't the first range we're looking at
					die "Range begins / ends before current max value at line $lineno" if
						($start <= $curstate->{'maxtid'}) or ($end <= $curstate->{'maxtid'});
				}

				$curstate->{'maxtid'} = $end;
				$curstate->{'ranges'}->{"$start-$end"} = parse_when_command($states, $2, $3);

				# So we don't have to parse them again when we write the body.
				$curstate->{'ranges'}->{"$start-$end"}->{'start'} = $start;
				$curstate->{'ranges'}->{"$start-$end"}->{'end'} = $end;

				print " --> Handling: when body created new range\n" if $verbose;
				next;
			} elsif (m/^\s*end\s*(#|$)/) {
				print " --> Handling: writing when body\n" if $verbose;
				write_when_body($outfd, $curstate);
				$parse_state = STATE_FIND_COMMAND;
				next;
			} else {
				die "Invalid command '$_' found in 'when' body on line $lineno";
			}
		}

		die "Unhandled parse state '$parse_state' at line $lineno";
	}

	# Close out final timeslice.
	print $outfd "\xde\xad\x76\x00";
}

sub get_range {
	my ($state, $range) = @_;

	my $start;
	my $end;

	if ($range eq 'N') {
		$start = $state->{'maxtid'} + 1;
		$end = N_VALUE;
	} elsif ($range =~ m/'(\d+)-(\d+)'/) {
		$start = $1;
		$end = $2;
	} else {
		$start = $range;
		$end = $range;
	}

	return (int($start), int($end));
}

sub parse_when_command {
	my ($state_table, $command, $arg) = @_;

	my $bc_command = "";
	my $bc_arg = "";
	if ($command eq 'callback') {
		die "Invalid callback: $arg" if !defined $state_table->{$arg};
		$bc_command = pack('n', 0);
		$bc_arg = $state_table->{$arg}->{'id'};
	} elsif ($command eq 'continue') {
		$bc_command = pack('n', 1);
		$bc_arg = "";
	} elsif ($command eq 'panic') {
		$bc_command = pack('n', 2);
		$bc_arg = "";
	} elsif ($command eq 'sleep') {
		$arg =~ m/(\d+)(s|ms|μs|us|ns)?/;
		# Default to seconds if no time specification was there.
		my $spec = $2 || 's';

		# Unit specifier
		if ($spec eq 's') {
			$spec = 0;
		} elsif ($spec eq 'ms') {
			$spec = 1;
		} elsif ($spec eq 'us' || $spec eq 'μs') {
			$spec = 2;
		} elsif ($spec eq 'ns') {
			$spec = 3;
		}

		$bc_command = pack('n', 4);
		$bc_arg = pack("Cn", $spec, $1);
	} elsif ($command eq 'wait') {
		$bc_command = pack('n', 8);
		$bc_arg = "";
	} else {
		die "Invalid command $command";
	}

	return {
		command	=> $bc_command,
		arg	=> $bc_arg,
	};
}

sub write_resume {
	my ($fd, $state_id, $start, $end) = @_;

	# Prologue
	print $fd "\x6a\x04\x61\x00";
	
	# Big-endian state id, start thread, and end thread
	print $fd pack("NNN", $state_id, $start, $end);
}

sub write_timeout {
	my ($fd, $timeout, $spec) = @_;

	# Prologue
	print $fd "\x75\x6e\x69\x00";

	# Unit specifier
	if ($spec eq 's') {
		print $fd "\x00";
	} elsif ($spec eq 'ms') {
		print $fd "\x01";
	} elsif ($spec eq 'us' || $spec eq 'μs') {
		print $fd "\x02";
	} elsif ($spec eq 'ns') {
		print $fd "\x03";
	}

	# Value
	print $fd pack("N", $timeout);
}

sub write_timeslice {
	my ($fd, $notify, $slice_id) = @_;

	# Close the preceding timeslice, if it's not the first one.
	if ($slice_id > 0) {
		print $fd "\xde\xad\x76\x00";
	}

	# Prologue
	print $fd "\x76\x04\x6c\x00";
	# Slice ID, big endian
	print $fd pack("N", $slice_id);

	if (!defined $notify) {
		print $fd "\x00"
	} else {
		print $fd "\x01"
	}
}

sub write_waitstate {
	my $fd = shift;

	print $fd "\x6f\x05\x61\x00";
}

sub write_when_prologue {
	my ($fd, $state_id) = @_;

	# Prologue
	print $fd "\x6a\x6f\x73\x00";
	# State ID, big endian
	print $fd pack("N", $state_id);
	# Epilogue
	print $fd "\x00";
}

sub write_when_body {
	my ($fd, $state) = @_;

	foreach my $range (sort keys %{ $state->{'ranges'} }) {
		my $hr = $state->{'ranges'}->{$range};
		print $fd pack("NN", $hr->{'start'}, $hr->{'end'});
		print $fd $hr->{'command'};
		print $fd $hr->{'arg'};
	}

	print $fd "\xde\xad\x6a\x00";
}

parse_file();

__END__

=head1 NAME

kimi - Kimi language to bytecode compiler

=head1 SYNOPSIS

kimi [options]

 Options:
   --infile, -i		Kimi script to compile
   --outfile, -o	Kimi bytecode output file
   --help		Short help message
   --man		Full documentation

=head1 OPTIONS

=over 8

=item B<--infile>, B<-i>

Path to Kimi script for input. This script will be compiled to Finnish
bytecode. Defaults to a file called 'in.km' in the current directory.

=item B<--outfile>, B<-o>

Output file name to store Finnish bytecode. Defaults to a file called
'out.fi' in the current directory.

=item B<--help> and B<--man>

If you need help for these options, you need more help for other things.

=back

=head1 DESCRIPTION

Kimi is a DSL for specifying program state transitions and behavior when
states are achieved in particular time slices. It is intended for use with
the interpreter in the Räikkönen race tester. Information on Kimi,
Räikkönen, Finnish, Kimi Räikkonen, and Finnish can be found in the
README of this source code's repository.

=head1 AUTHOR

Devon H. O'Dell <dho@fastly.com>

=cut
