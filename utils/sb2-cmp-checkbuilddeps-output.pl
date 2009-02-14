#!/usr/bin/perl
#
# Copyright (c) 2009 Nokia Corporation. All rights reserved.
# Author: Lauri T. Aarnio;
#
# Licensed under GPL version 2
#
# -------------------
# "Mechanics" of the new dpkg-checkbuilddeps dependency checker.
#
# This takes results from two runs of dpkg-checkbuilddeps (once
# against the target's db, and once tools package db) and
# compares the results.
#
# FIXME: Hopefully this will be developed to a full-featured
# replacement for sb2's old dpkg-checkbuilddeps wrapper (which
# is a shell script). Currently that wrapper just feeds all needed
# variables to us...

use strict;

my $target_missing_deps=$ARGV[0];
my $host_missing_deps=$ARGV[1];
my $both_required=$ARGV[2];
my $accepted_from_tools=$ARGV[3];
my $ignored_from_tools=$ARGV[4];

my $verbose_messages_env_var=$ENV{'SBOX_CHECKBUILDDEPS_VERBOSE'};
my $verbose_messages = 0;
if (defined $verbose_messages_env_var && $verbose_messages_env_var ne '') {
	$verbose_messages = 1;
}

my @both_required = split(/\s/,$both_required);

my @accepted_from_tools = split(/\s/,$accepted_from_tools);
my %accepted_from_tools;
my $n;
foreach $n (@accepted_from_tools) {
	$accepted_from_tools{$n} = 1;
}

my @ignored_from_tools = split(/\s/,$ignored_from_tools);
my %ignored_from_tools;
my $n;
foreach $n (@ignored_from_tools) {
	$ignored_from_tools{$n} = 1;
}

my $debug = 0;

if ($debug) {
	print "T: $target_missing_deps\n";
	print "H: $host_missing_deps\n";
	print "B: $both_required\n";
}

sub is_accepted_from_tools {
	my $r_host_missing;
	my $name = shift;
	my $vers = shift;

	if ($accepted_from_tools{$name}) {
		if (defined $r_host_missing->{$name}) {
			return(0, "$name $vers can be used from tools, but is is not installed there");
		}
		return(1, "$name $vers found from tools");
	}

	# not allowed from tools
	if (defined $r_host_missing->{$name}) {
		return(0, "$name $vers can not be used from tools (not found and not allowed)");
	}
	return(0, "$name $vers can not be used from tools (installed, but not allowed)");
}

# Parse one required package name from output of "dpkg-checkbuilddeps"
# returns ($pkg_name, $version, $leftovers)
sub parse_requirement {
	my $list = shift;
	my $debug_prefix = shift;

	if ($debug) {
		print "$debug_prefix"."parse_requirement($list)\n";
	}
	if ($list =~ s/^\s*(\S+)\s+(\([^)]*\))\s*//) {
		if ($debug) {
			print "$debug_prefix=> VERS $1 : $2\n";
		}
		return($1, $2, $list);	
	} elsif ($list =~ s/^\s*(\S+)\s*//) {
		if ($debug) {
			print "$debug_prefix=> SINGLE $1\n";
		}
		return($1, undef, $list);	
	}
	
	# else failed to recognize enything from $list
	print "dpkg-checkbuilddeps: Parse error in '$list'\n";
	return(undef, undef, undef);	
}

# Parse output of "dpkg-checkbuilddeps"
# returns a reference to a has containing missing packages
sub parse_list_of_missing_packages {
	my $a = shift;

	my %pkgs;

	# While $a is not empty or just whitespace..
	while($a =~ m/\S+/) {

		my $pkg_name;
		my $version;
		my $leftovers;
		($pkg_name, $version, $leftovers) = parse_requirement($a, "\t");
		
		if (!defined $pkg_name) {
			print "dpkg-checkbuilddeps: FAILED to parse '$a'\n";
			exit(1);
		}

		$pkgs{$pkg_name} = {
			'version' => $version,
			'alternatives' => [],
		};

		if ($debug) {
			if (defined $version) {
				print "=> $pkg_name : $version\n";
			} else {
				print "=> $pkg_name\n";
			}
		}

		# See if there are alternatives:

		my $alternatives = "";
		while ($leftovers =~ m/^\|/) {
			if ($debug) {
				print "\tParsing alternatives $leftovers\n";
			}

			$leftovers =~ s/^\|\s*//;

			my $alt_pkg_name;
			my $alt_version;
			($alt_pkg_name, $alt_version, $leftovers) = parse_requirement($leftovers, "\t\t");

			if (!defined $alt_pkg_name) {
				print "dpkg-checkbuilddeps: FAILED to process alternatives ($leftovers)\n";
				exit(1);
			}
			$alternatives .= " ".$alt_pkg_name;

			$pkgs{$alt_pkg_name} = {
				'version' => $alt_version,
				'primary' => $pkg_name,
			};
			push(@{$pkgs{$pkg_name}->{'alternatives'}}, $alt_pkg_name);
		}
		if ($debug) {
			if ($alternatives ne "") {
				print "\talternatives for $pkg_name=[".
					join(", ", @{$pkgs{$pkg_name}->{'alternatives'}}).
					"] == $alternatives\n";
			}
		}

		$a = $leftovers;
	}
	return(\%pkgs);
}

# a debugging routine:
sub show_missing {
	my $r_missing = shift;

	my @missing_pkgs_keys = sort(keys(%$r_missing));
	print "missing: ".join("; ", @missing_pkgs_keys)."\n";

	my $n;
	foreach $n (@missing_pkgs_keys) {
		my $alt = $r_missing->{$n}->{'alternatives'};
		if (defined $alt && @$alt > 0) {
			print "alternatives for $n: ".join(", ", @$alt)."\n";
		}
		my $primary = $r_missing->{$n}->{'primary'};
		if (defined $primary) {
			print "primary for $n: $primary\n";
		}
	}
}

# XXXXXXXX host-ymaristossa on kiinnostavaa vain ne paketit
# XXXXXXXX joista myos target valittaa!!!!

my $r_host_missing = parse_list_of_missing_packages($host_missing_deps);

my @host_listed = sort(keys(%$r_host_missing));

if ($debug) {
	print "\n\nHost is missing: ".join("; ", @host_listed)."\n";
	show_missing($r_host_missing);
	print "\n\n";
}

my $r_target_missing = parse_list_of_missing_packages($target_missing_deps);

my @target_listed = sort(keys(%$r_target_missing));

if ($debug) {
	print "\n\nTarget is missing: ".join("; ", @target_listed)."\n";
	show_missing($r_target_missing);
	print "\n\n";
}

my $build_deps_ok = 1;	# assume everything is ok.
my $std_missing_pkg_msg = "";	# list of missing packages in "standard" format.

# See if the packages that target is missing can be found from tools:
my $n;
foreach $n (@target_listed) {
	if (defined $r_target_missing->{$n}->{'primary'}) {
		if ($debug) {
			print "(skipping alternative $n)\n";
		}
	} elsif ($ignored_from_tools{$n}) {
		if ($verbose_messages) {
			print "\t$n: Missing from tools, but dependency has been ignored by configuration\n";
		}
	} elsif (defined $r_host_missing->{$n}) {
		my $target_vers_req = $r_target_missing->{$n}->{'version'};
		my $tools_vers_req = $r_host_missing->{$n}->{'version'};

		if ($verbose_messages) {
			print "\t$n: Missing from tools $tools_vers_req and target $target_vers_req\n";
		}

		my $alt = $r_target_missing->{$n}->{'alternatives'};
		if (defined $alt && (@$alt > 0)) {
			# there are alternatives: see if any of those exists on the host
			
			if ($verbose_messages) {
				print "\t   There are alternatives: ".join(", ", @$alt)."\n";
			}

			my $no_alternatives_found = 1;
			my $k;
			foreach $k (@$alt) {
				if (defined $r_host_missing->{$k}) {
					if ($verbose_messages) {
						print "\t   ($k not found from tools)\n";
					}
				} elsif ($ignored_from_tools{$k}) {
					if ($verbose_messages) {
						print "\t   $k: Missing from tools, ignored by configuration, good..\n";
					}
					$no_alternatives_found = 0;
				} else {
					my ($found, $message);
					($found, $message) = is_accepted_from_tools(
						$r_host_missing, $n, $target_vers_req);
					if ($found) {
						if ($verbose_messages) {
							print "\t   $n: found from tools, good..\n";
						}
						$no_alternatives_found = 0;
					} else {
						if ($verbose_messages) {
							print "\t   $n: $message\n";
						}
					}
				}
			}

			if ($no_alternatives_found) {
				if ($verbose_messages) {
					print "\t$n: none of the alternatives exist, requirement failed.\n";
				}
				$build_deps_ok = 0;
				$std_missing_pkg_msg .= 
					" ".$n." | ".join(" | ", @$alt);
			}
		} else {
			if ($verbose_messages) {
				print "\t$n: not found, requirement failed.\n";
			}
			$build_deps_ok = 0;
			$std_missing_pkg_msg .= " ".$n;
		}
	} else {
		my ($found, $message);
		($found, $message) = is_accepted_from_tools($n);
		if ($verbose_messages) {
			if ($found) {
				print "\t$n: found from tools, good..\n";
			} else {
				print "\t$n: $message\n";
			}
		}
		if ($found == 0) {
			$build_deps_ok = 0;
			$std_missing_pkg_msg .= " ".$n;
			my $alt = $r_target_missing->{$n}->{'alternatives'};
			if (defined $alt && (@$alt > 0)) {
				$std_missing_pkg_msg .= " | ".join(" | ", @$alt);
			}
		}
	}
}

# See if there are packages that are required at both locations,
# but the host does not have:
my $n;
my @failures_in_dual_requirements;
my @failed_pkgs_in_dual_requirements;
foreach $n (@both_required) {
	if (defined $r_host_missing->{$n}) {
		# Host does not have this...
		my $tools_vers_req = $r_host_missing->{$n}->{'version'};
		my $msg = "$n: ";

		# check if it exists on the host, but fails
		# during the version check
		my $prev_mode=$ENV{'SBOX_SESSION_MODE'};
		$ENV{'SBOX_SESSION_MODE'}="tools";
		my $tools_stat=`dpkg-query -W -f=\'\${Version}\' $n 2>&1`;
		$ENV{'SBOX_SESSION_MODE'}=$prev_mode;
		chomp($tools_stat);

		if (defined $r_target_missing->{$n}) {
			# neither has it. 
			$msg .= "missing from both $tools_vers_req".
				" (tools: $tools_stat)";
		} else {
			$msg .= "only target has it $tools_vers_req".
				" (tools: $tools_stat)";

		}
		push(@failures_in_dual_requirements, $msg);
		push(@failed_pkgs_in_dual_requirements, $n);
	} elsif (defined $r_target_missing->{$n}) {
		my $target_vers_req = $r_target_missing->{$n}->{'version'};
		my $msg = "$n: only tools has it $target_vers_req\n";
		push(@failures_in_dual_requirements, $msg);
		push(@failed_pkgs_in_dual_requirements, $n);
	} else {
		if ($debug) {
			print "B: $n, both have it\n";
		}
	}
}
if (@failures_in_dual_requirements > 0) {
	if ($verbose_messages) {
		print "Some packages must exist in target and tools:\n";
		print "\t".join("\n\t", @failures_in_dual_requirements)."\n";
	} else {
		print "dpkg-checkbuilddeps: Some packages must exist in target and tools.\n";
	}
	$build_deps_ok = 0;
	$std_missing_pkg_msg .= " ".join(" ", @failed_pkgs_in_dual_requirements);
}

if ($build_deps_ok) {
	if ($verbose_messages) {
		print "Dependency checks ok\n";
	}
	exit(0);
}

if ($verbose_messages) {
	print "Dependency checks failed\n";
}
print "dpkg-checkbuilddeps: Unmet build dependencies:$std_missing_pkg_msg\n";
exit(1);

