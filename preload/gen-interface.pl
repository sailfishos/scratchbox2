#!/usr/bin/perl
#
# gen-wrappers.pl -- an interface generator for scratchbox2 preload library
#
# Copyright (C) 2007 Lauri T. Aarnio
#
#
# This script is an interface generator for scratchbox 2. Based on
# an interface specification file (e.g. "interface.master"),
# it creates
#   - a header file containing prototypes of all exported functions
#   - list of exported symbols, to be used by "ld" when the shared
#     library is created.
#   - library function wrappers or gates (functions in C).
#
# The specification file consists of lines, with two or more fields:
#   - 1st field is a command (WRAP, GATE or EXPORT)
#   - 2nd field is a function definition (using 100% standard C syntax)
#   - 3rd (optional) field may contain modifiers for the command.
# Fields are separated by colons (:), and one logical line line can be
# split to several physical lines by using a backslash as the last character
# of a line.
#
# Command "WRAP" is used to generate wrapper functions. A wrapper performs
# specified parameter transformations (usually path remapping) and then
# calls the next function with same name. Note than wrappers
# for functions with variable number of arguments are not fully supported;
# only limited support for open()-like functions is available.
#
# Command "GATE" is otherwise like a wrapper, but it does not call directly
# the next function; instead it calls a "gate" function which may perform
# additional preparations before calling the next function.
#
# Following modifiers are available for "WRAP" and "GATE":
#   - "map(varname)" will map function's parameter "varname" using
#     the SBOX_MAP_PATH macro
#   - "map_at(fdname,varname)" will map function's parameter "varname" using
#     the SBOX_MAP_PATH_AT macro
#   - "hardcode_param(N,name)" will hardcode name of the Nth parameter
#     to "name" (this is typically needed only if the function definition uses
#     macros to build the parameter list, instead of specifying names of
#     all parameters in the definition itself):
#   - "optional_arg_is_create_mode" handles varargs for open() etc.,
#     where an optional 3rd arg is "mode".
# For "WRAP" only:
#   - "create_nomap_nolog_version" creates a direct interface function to the
#     next function (for internal use inside the preload library)
# For "GATE" only:
#   - "pass_va_list" is used for generic varargs processing: It passes a
#     "va_list" to the gate function.
#
# Command "EXPORT" is used to specify that a function needs to be exported
# from the scratchbox preload library. This does not create any wrapper
# functions, but still puts the prototype to the include file and name of
# the function to the export list.

use strict;

our($opt_d, $opt_W, $opt_E, $opt_L);
use Getopt::Std;

# Process options:
getopts("dW:E:L:");
my $debug = $opt_d;
my $wrappers_c_output_file = $opt_W;		# -W generated_c_filename
my $export_h_output_file = $opt_E;		# -E generated_h_filename
my $export_list_for_ld_output_file = $opt_L;	# -L generated_list_for_ld


my $num_errors = 0;

#============================================

sub write_output_file {
	my $filename = shift;
	my $contents = shift;

	open OF, ">$filename"
		|| die "Can't open output file $filename for writing\n";
	if($debug) {
		printf "Writing %s\n", $filename;
	}
	print OF $contents;
	close OF;
}

#============================================
# A minimal C declarator parser.
#
# The functions to be wrapped are specified with standard C syntax.
# Input to this simple parser is a string which declares a function
# (e.g. "int foo(long a, char *s, ...)") and output is a structure
# with following members:
#
#  - 'fn_return_type' = return type of the function ("int" for the prev.example)
#  - 'fn_name' = name of the function (e.g. "foo")
#  - 'num_parameters'
#  - array 'parameter_names' (e.g. "a", "s", "...")
#  - array 'parameter_types' (e.g. "long", "char *", undef)
#  - flag 'has_varargs' (e.g. 1 in this case)
#  - 'varargs_index' (e.g. 2 in this case; refers to the parameter arrays)
#  - 'last_named_var' = only if varargs are used: name of the last named
#			parameter (e.g. "s")
#
# The parser also keeps names of all functions in %all_function_names.
#
# (This parser is somewhat simple, does not even try to support all possible
# ways how types can be specified in C)

my %all_function_names;

# parser: pick type and name from a C declarator.
sub parser_separate_type_and_name {
	my $input = shift;

	my $type = "";
	my $name = "";

	if( ($input =~ m/^\s*(\S.*?\S)\s+(\w+)\s*$/) ||
	    ($input =~ m/^\s*(\S.*?\S\s*\*)\s*(\w+)\s*$/) ) {
		# Case 1: simple variable.
		$type = $1;
		$name = $2;

		if($debug) { print "type='$type', name='$name'\n"; }
	} elsif($input =~ m/^\s*(\S.*?\S)\s*(\w+)\s*\[(.*)\]$/) {
		# Case 2: an array.
		$type = $1."[".$3."]";
		$name = $2;

		if($debug) { print "Array: type='$type', name='$name'\n"; }
	} elsif($input =~ m/^\s*(\S.*?\S)\s*\(\s*\**\s*(\w+)\s*\)\s*\((.*)$/) {
		# Case 3: a function pointer.
		$type = $1."(*)(".$3;
		$name = $2;

		if($debug) { print "FunctPTR: type='$type', name='$name'\n"; }
	} else {
		printf "WARNING: failed to find type+name from '%s'\n",
			$input;

		$type = $input;
		$name = "";
	}

	return($type,$name);
}

# Split function definition to ($fn_type,$fn_name,$params)
# returns undef if error.
sub parser_split_function_definition_to_components {
	my $funct_def = shift;

	if($funct_def =~ m/([^\(]+)\((.*)\)\s*$/) {
		my $funct_and_type = $1;
		my $params = $2;

		if($debug) { print "Funct+type='$1', params='$2'\n"; }

		my $fn_type;
		my $fn_name;
		($fn_type,$fn_name) = parser_separate_type_and_name($funct_and_type);
		return($fn_type,$fn_name,$params);
	}

	# error
	return(undef);
}

# split a function's parameter list to components
# (input=a fragment of C (string), output=array)
# input is typically the 3rd return value from
# parser_split_function_definition_to_components()
sub parser_split_parameter_list_to_components {
	my $params = shift;

	if(!defined($params) || ($params eq 'void') || ($params =~ /^\s*$/)) {
		# no parameters.
		return;
	}

	# Split parameters to array @params_components.
	my @params_components;

	# This not pretty. Not at all. But we have to parse function
	# pointers as arguments, and those tend to have parameter
	# lists, too, so nested parameter lists have to be handled:
	#
	# So, if $params contained a function pointer with parameters,
	# then @split_list needs some adjusting. For example,
	# when $params is "int a, int (*fp)(int b, int c), int d)",
	# a simple split will produce four elements to @split_list.
	# That needs to be fixed.
	#
	# Warning/FIXME: This might be too simple, works for most
	# common cases, but may fail with very complex types (function
	# taking another function pointer as an argument, etc).
	# But we probably don't want to implement a full C type parser
	# here!
	my @split_list = split(/\s*,\s*/, $params);

	my $in_funct_count = 0;
	my @temp_buf;
	my $i = @split_list;
	while($i > 0) {
		$i--;
		if($split_list[$i] =~ /\)$/) {
			if($debug) {
				printf "found end of fp params:'%s'\n",
					$split_list[$i];
			}
			$in_funct_count++;
		}
		if($in_funct_count) {
			# prosessing parameters of an function
			# pointer parameter
			if($debug) {
				printf "'%s' => tmp_buf\n", $split_list[$i];
			}
			unshift(@temp_buf, $split_list[$i]);
		} else {
			# Processing a real paremeter
			if(@temp_buf > 0) {
				# the previous one was an fp..
				my $fnp_param = join(",", @temp_buf);
				if($debug) {
					printf "fnp_param = '%s'\n", $fnp_param;
				}
				unshift(@params_components, $fnp_param);
				@temp_buf = ();
			}
			if($debug) { printf "add param:'%s'\n", $split_list[$i]; }
			unshift(@params_components, $split_list[$i]);
		}
		if($split_list[$i] =~ /\(.*\)\s*\(/) {
			if($debug) {
				printf "found start of fp params:'%s'\n",
					$split_list[$i];
			}
			$in_funct_count--;
			if(($in_funct_count == 0) && (@temp_buf > 0)) {
				# the parameter was an fp..
				my $fnp_param = join(",", @temp_buf);
				if($debug) {
					printf "1st fnp_param = '%s'\n", $fnp_param;
				}
				unshift(@params_components, $fnp_param);
				@temp_buf = ();
			}
		}
	}
	return(@params_components);
}

sub add_function_name_to_symbol_table {
	my $fn_name = shift;

	if(defined($all_function_names{$fn_name})) {
		return;
	}
	$all_function_names{$fn_name} = 1;
}

# Input = string (a function declarator)
# output = reference to a "structure" containing parser results
sub minimal_function_declarator_parser {
	my $fn_declarator = shift;

	# this structure will be returned:
	my $res = {
		'fn_return_type' => undef,
		'fn_name' => undef,
		'fn_parameter_list' => "",
		'num_parameters' => 0,
		'parameter_names' => [],
		'parameter_types' => [],
		'all_params_with_types' => [],
		'has_varargs' => 0,
		'varargs_index' => -1,
		'last_named_var' => undef,
	};

	my $fn_type;
	my $fn_name;
	my $params;
	($fn_type,$fn_name,$params) =
		parser_split_function_definition_to_components($fn_declarator);

	if(! defined($fn_type)) {
		printf "ERROR: failed to parse function ".
			"definition '%s'\n", $fn_declarator;
		$num_errors++;
		return undef;
	}

	# return type and function name has been parsed.
	$res->{'fn_return_type'} = $fn_type;
	$res->{'fn_name'} = $fn_name;
	$res->{'fn_parameter_list'} = $params;
	add_function_name_to_symbol_table($fn_name);

	# Proceed to function parameters.
	if($debug) { print "Parameters:\n"; }

	@{$res->{'all_params_with_types'}} =
		parser_split_parameter_list_to_components($params);

	my $num_parameters = @{$res->{'all_params_with_types'}};
	$res->{'num_parameters'} = $num_parameters;

	# Now @all_params_with_types contains the parameters, types are still
	# attached to them. Separate names and types of all parameters.
	my $i;
	for($i=0; $i < $num_parameters; $i++) {
		my $param = $res->{'all_params_with_types'}->[$i];
		if($param eq '...') {
			if($debug) { print "varargs detected\n"; }
			$res->{'has_varargs'} = 1;
			$res->{'varargs_index'} = $i;
		} else {
			my $type;
			my $name;
			($type,$name) = parser_separate_type_and_name($param);

			$res->{'parameter_types'}->[$i] = $type;
			$res->{'parameter_names'}->[$i] = $name;
			$res->{'last_named_var'} = $name;
		}
	}

	return($res);
}

# End of the minimal C declarator parser.
#============================================

sub create_code_for_va_list_get_mode {
	my $condition = shift;
	my $last_named_var = shift;

	if($condition ne '') {
		$condition = "if($condition) ";
	}
	return( "\t${condition}{\n".
		"\t\tva_list arg;\n".
                "\t\tva_start (arg, $last_named_var);\n".
                "\t\tmode = va_arg (arg, int);\n".
                "\t\tva_end (arg);\n".
		"\t}\n");
}

# Process the modifier section coming from the original input line.
# This returns undef if failed, or a structure containing code fragments
# and other information for the actual code generation phase.
sub process_wrap_or_gate_modifiers {
	my $command = shift;
	my $fn = shift;		# structure: parser results
	my $all_modifiers = shift;

	my @modifiers = split(/\s+/, $all_modifiers);
	my $num_modifiers = @modifiers;

	# cache some fn parser results to local vars
	my $fn_name = $fn->{'fn_name'};
	my $varargs_index = $fn->{'varargs_index'};

	# This will be returned:
	my $mods = {
		'path_mapping_vars' => "",
		'path_mapping_code' => "",
		'free_path_mapping_vars_code' => "",
		'local_vars_for_varargs_handler' => "",
		'va_list_handler_code' => "",
		'va_list_end_code' => "",
		'mapped_params_by_orig_name' => {},

		# processing modifiers may change the parameter list
		# (but always we'll start with a copy of the original names)
		'parameter_names' => [@{$fn->{'parameter_names'}}],

		'make_nomap_function' => 0,		# flag
		'make_nomap_nolog_function' => 0,	# flag

		# name of the function pointer variable
		'real_fn_pointer_name' => "${fn_name}_next__",
	};

	my $r_param_names = $mods->{'parameter_names'};

	my $varargs_handled = 0;

	my $i;
	for($i=0; $i < $num_modifiers; $i++) {
		if($debug) { printf "\Modifier:'%s'\n", $modifiers[$i]; }
		if($modifiers[$i] =~ m/^map\((.*)\)$/) {
			my $param_to_be_mapped = $1;
			my $new_name = "mapped__".$param_to_be_mapped;
			$mods->{'mapped_params_by_orig_name'}->{$param_to_be_mapped} = $new_name;
			$mods->{'path_mapping_vars'} .= "\tchar *$new_name = NULL;\n";
			$mods->{'path_mapping_code'} .=
				"\tSBOX_MAP_PATH($param_to_be_mapped, ".
					"$new_name);\n";
			$mods->{'free_path_mapping_vars_code'} .=
				"\tif($new_name) free($new_name);\n";

			# Make a "..._nomap" version, because the main
			# wrapper has mappings.
			$mods->{'make_nomap_function'} = 1;
		} elsif($modifiers[$i] =~ m/^map_at\((.*),(.*)\)$/) {
			my $fd_param = $1;
			my $param_to_be_mapped = $2;
			my $new_name = "mapped__".$param_to_be_mapped;
			$mods->{'mapped_params_by_orig_name'}->{$param_to_be_mapped} = $new_name;
			$mods->{'path_mapping_vars'} .= "\tchar *$new_name = NULL;\n";
			$mods->{'path_mapping_code'} .=
				"\tSBOX_MAP_PATH_AT($fd_param, ".
				"$param_to_be_mapped, $new_name);\n";
			$mods->{'free_path_mapping_vars_code'} .=
				"\tif($new_name) free($new_name);\n";

			# Make a "..._nomap" version, because the main
			# wrapper has mappings.
			$mods->{'make_nomap_function'} = 1;
		} elsif(($modifiers[$i] eq 'create_nomap_nolog_version') &&
			($command eq 'WRAP')) {

			$mods->{'make_nomap_nolog_function'} = 1;
		} elsif($modifiers[$i] =~ m/^hardcode_param\((.*),(.*)\)$/) {
			my $param_number = $1;
			my $param_name = $2;
			$r_param_names->[$param_number - 1] = $param_name;
		} elsif(($modifiers[$i] eq 'optional_arg_is_create_mode') &&
			($fn->{'has_varargs'})) {
			$r_param_names->[$varargs_index] = "mode";
			$mods->{'local_vars_for_varargs_handler'} .=
				"\tint mode = 0;\n";
			$mods->{'va_list_handler_code'} =
				create_code_for_va_list_get_mode(
					"", $fn->{'last_named_var'});
			$varargs_handled = 1;
		} elsif(($modifiers[$i] =~ m/^optional_arg_is_create_mode\((.*)\)$/) &&
			($fn->{'has_varargs'})) {
			my $va_list_condition = $1;
			$r_param_names->[$varargs_index] = "mode";
			$mods->{'local_vars_for_varargs_handler'} .=
				"\tint mode = 0;\n";
			$mods->{'va_list_handler_code'} =
				create_code_for_va_list_get_mode(
					$va_list_condition,
					$fn->{'last_named_var'});
			$varargs_handled = 1;
		} elsif(($modifiers[$i] eq 'pass_va_list') &&
			($fn->{'has_varargs'}) &&
			($command eq 'GATE')) {

			$r_param_names->[$varargs_index] = "ap";
			$mods->{'local_vars_for_varargs_handler'} .=
				"\tva_list ap;\n";
			$mods->{'va_list_handler_code'} = "\tva_start(ap,".
				$fn->{'last_named_var'}.");\n";
			$mods->{'va_list_end_code'} = "\tva_end(ap);\n";
			$varargs_handled = 1;
		} else {
			printf "ERROR: unsupported modifier '%s'\n",
				$modifiers[$i];
			$num_errors++;
			return(undef);
		}
	}

	if(($fn->{'has_varargs'}) && ($varargs_handled == 0)) {
		printf "ERROR: variable arguments not handled properly at '%s'\n",
			$fn_name;
		$num_errors++;
		return(undef);
	}
	return($mods);
}

sub create_call_to_real_fn {
	my $fn = shift;
	my $mods = shift;
	my @param_list_in_next_call = @_;

	my $real_fn_pointer_name = $mods->{'real_fn_pointer_name'};

	return(
		# 1. call with mapped parameters
		"(*$real_fn_pointer_name)(".
			join(", ", @param_list_in_next_call).");\n",
		# 2. call with original (unmapped) parameters
		"(*$real_fn_pointer_name)(".
			join(", ", @{$mods->{'parameter_names'}}).");\n",
		# 3. call with original parameters (without logging)
		"(*$real_fn_pointer_name)(".
			join(", ", @{$mods->{'parameter_names'}}).");\n"
	);
}

sub create_call_to_gate_fn {
	my $fn = shift;
	my $mods = shift;
	my @param_list_in_next_call = @_;

	my @gate_params_with_types = @{$fn->{'all_params_with_types'}};
	my $num_gate_params = @gate_params_with_types;
	my $orig_param_list;
	my $modified_param_list;
	my $gate_params = $mods->{'real_fn_pointer_name'}.", __func__";
	my $fn_ptr_prototype_params;
	my $prototype_params;

	# cache some fn parser results to local vars
	my $fn_name = $fn->{'fn_name'};

	if($num_gate_params > 0) {
		# has parameters
		my $varargs_index = $fn->{'varargs_index'};

		if($varargs_index >= 0) {
			$gate_params_with_types[$varargs_index] =
				"va_list ap";
		}
		$orig_param_list = $gate_params.", ".
			join(", ", @{$mods->{'parameter_names'}});
		$modified_param_list = $gate_params.", ".
			join(", ", @param_list_in_next_call);
		$fn_ptr_prototype_params = join(", ",
			@{$fn->{'all_params_with_types'}});
		$prototype_params = $fn->{'fn_return_type'}." ".
			"(*real_${fn_name}_ptr)".
			"($fn_ptr_prototype_params),\n".
			"\tconst char *realfnname, ".
			join(", ", @gate_params_with_types);
	} else {
		$orig_param_list = $gate_params;
		$modified_param_list = $gate_params;
		$fn_ptr_prototype_params = "void";
		$prototype_params = $fn->{'fn_return_type'}.
			" (*real_${fn_name}_ptr)(void), ".
			"\tconst char *realfnname";
	}

	my $mapped_call = "${fn_name}_gate($modified_param_list);\n";
	my $unmapped_call = "${fn_name}_gate($orig_param_list);\n";
	# nomap_nolog is not possible for GATEs

	my $gate_function_prototype =
		"extern ".$fn->{'fn_return_type'}." ${fn_name}_gate(".
			"$prototype_params);\n";

	return($mapped_call, $unmapped_call, $gate_function_prototype);
}

#-------------------
# actual code generators:

my $wrappers_c_buffer = "";	# buffers contents of the generated ".c" file
my $export_h_buffer = "";	# buffers contents of the generated ".h" file

# Handle "WRAP" and "GATE" commands.
sub command_wrap_or_gate {
	my $command = shift;
	my $funct_def = shift;
	my $all_modifiers = shift;

	if($debug) { printf "\nWRAPPER => '%s'\n", $funct_def; }

	$export_h_buffer .= "extern $funct_def; /* ($command) */\n";

	my $fn = minimal_function_declarator_parser($funct_def);
	if(!defined($fn)) { return; } # return if parsing failed

	# cache some fn parser results to local vars
	my $fn_name = $fn->{'fn_name'};
	my $fn_return_type = $fn->{'fn_return_type'};

	my $va_list_get_mode_code = "";

	# Time to handle modifiers.
	my $mods = process_wrap_or_gate_modifiers($command, $fn, $all_modifiers);
	if(!defined($mods)) { return; } # return if modifiers failed

	# Ok, all preparations done. Create the pointer, wrapper functions, etc.
	if($debug) { print "Creating code:\n"; }

	my $real_fn_pointer_name = $mods->{'real_fn_pointer_name'};
	my $fn_pointer_c_code .=
		"static $fn_return_type ".
		"(*$real_fn_pointer_name)(".
		$fn->{'fn_parameter_list'}.");\n\n";

	# begin the function with the original name:
	my $wrapper_fn_c_code .=
		$funct_def."\n".
		"{\n".
		$mods->{'path_mapping_vars'}.
		$mods->{'local_vars_for_varargs_handler'};

	# begin the function with "_nomap" suffix added to name:
	my $nomap_funct_def = $funct_def;
	$nomap_funct_def =~ s/($fn_name)/$1_nomap/;
	$export_h_buffer .= "extern $nomap_funct_def;\n";
	my $nomap_fn_c_code .=
		$nomap_funct_def."\n".
		"{\n".
		$mods->{'local_vars_for_varargs_handler'};

	# begin the function with "_nomap_nolog" suffix added to name:
	my $nomap_nolog_funct_def = $funct_def;
	$nomap_nolog_funct_def =~ s/($fn_name)/$1_nomap_nolog/;
	$export_h_buffer .= "extern $nomap_nolog_funct_def;\n";
	my $nomap_nolog_fn_c_code .=
		$nomap_nolog_funct_def."\n".
		"{\n".
		$mods->{'local_vars_for_varargs_handler'};

	if($fn_return_type ne "void") {
		$wrapper_fn_c_code .=	"\t$fn_return_type ret;\n";
		$nomap_fn_c_code .=	"\t$fn_return_type ret;\n";
		$nomap_nolog_fn_c_code .= "\t$fn_return_type ret;\n";
	}

	$wrapper_fn_c_code .=		$mods->{'path_mapping_code'};
	$wrapper_fn_c_code .=		$mods->{'va_list_handler_code'};
	$nomap_fn_c_code .=		$mods->{'va_list_handler_code'};
	$nomap_nolog_fn_c_code .=	$mods->{'va_list_handler_code'};

	my $check_fn_pointer_log_enabled .=
		"\tif($real_fn_pointer_name == NULL) {\n".
		"\t\t$real_fn_pointer_name = sbox_find_next_symbol(1, ".
			"\"$fn_name\");\n".
		"\t}\n";
	my $check_fn_pointer_log_disabled .=
		"\tif($real_fn_pointer_name == NULL) {\n".
		"\t\t$real_fn_pointer_name = sbox_find_next_symbol(0, ".
			"\"$fn_name\");\n".
		"\t}\n";
	$wrapper_fn_c_code .=		$check_fn_pointer_log_enabled;
	$nomap_fn_c_code .=		$check_fn_pointer_log_enabled;
	$nomap_nolog_fn_c_code .=	$check_fn_pointer_log_disabled;

	# build the parameter list for the next call..
	my @param_list_in_next_call;
	my $i;
	for($i=0; $i < $fn->{'num_parameters'}; $i++) {
		my $param_name = $mods->{'parameter_names'}->[$i];
		my $mapped_param = $mods->{'mapped_params_by_orig_name'}->{$param_name};
		if(defined $mapped_param) {
			push @param_list_in_next_call, $mapped_param;
		} else {
			push @param_list_in_next_call, $param_name;
		}
	}

	# ..and the actual call.
	my $call_line_prefix = "\t";
	my $return_statement = ""; # return stmt not needed if fn_type==void
	if($fn_return_type ne "void") {
		$call_line_prefix .= "ret = ";
		$return_statement = "\treturn(ret);\n";
	}
	my $mapped_call;
	my $unmapped_call;
	my $unmapped_nolog_call;
	if($command eq 'WRAP') {
		($mapped_call, $unmapped_call, $unmapped_nolog_call) =
			create_call_to_real_fn($fn, $mods,
				@param_list_in_next_call);
	} else { # GATE
		my $gate_function_prototype;
		($mapped_call, $unmapped_call, $gate_function_prototype) =
			create_call_to_gate_fn($fn, $mods,
				@param_list_in_next_call);
		$export_h_buffer .= $gate_function_prototype;
		$unmapped_nolog_call = ""; # not supported for GATEs
	}
	$wrapper_fn_c_code .=		$call_line_prefix.$mapped_call;
	$nomap_fn_c_code .=		$call_line_prefix.$unmapped_call;
	$nomap_nolog_fn_c_code .=	$call_line_prefix.$unmapped_nolog_call;

	$wrapper_fn_c_code .=		$mods->{'va_list_end_code'};
	$wrapper_fn_c_code .=		$mods->{'free_path_mapping_vars_code'};
	$nomap_fn_c_code .=		$mods->{'va_list_end_code'};
	$nomap_nolog_fn_c_code .=	$mods->{'va_list_end_code'};

	$wrapper_fn_c_code .=		$return_statement."}\n";
	$nomap_fn_c_code .=		$return_statement."}\n";
	$nomap_nolog_fn_c_code .=	$return_statement."}\n";

	if($debug) {
		print "Wrapper code:\n".$wrapper_fn_c_code;
		print "Nomap code:\n".$nomap_fn_c_code;
		print "Nomap_nolog code:\n".$nomap_nolog_fn_c_code;
	}

	# put all generated pieces to the output buffer.

	$wrappers_c_buffer .=
		$fn_pointer_c_code.
		$wrapper_fn_c_code;
	if($mods->{'make_nomap_function'}) {
		$wrappers_c_buffer .= $nomap_fn_c_code;
	}
	if($mods->{'make_nomap_nolog_function'}) {
		$wrappers_c_buffer .= $nomap_nolog_fn_c_code;
	}
	$wrappers_c_buffer .= "\n";
}

# Handle the "EXPORT" command.
sub command_export {
	my @field = @_;

	my $funct_def = $field[1];

	$export_h_buffer .= "extern $funct_def; /* (exported) */\n";

	my $fn = minimal_function_declarator_parser($funct_def);
	# The parser put name of the function to the symbol table.
	# Main program will create the symbol list for ld, once everything
	# else has been done => we don't need to do anything else here.

	if($debug) {
		print "Exports: ".$fn->{'fn_name'}." from '$funct_def'\n";
	}
}


#============================================
# Main loop.
#
# Reads lines from standard input and call the above functions
# to perform actions.
my $line;
my $token_cache;		# cached pre-processor token

while ($line = <STDIN>) {
	$line =~ s/^--.*$//; # cut off comments
	next if ($line =~ m/^\s*$/); # skip empty lines

	while($line =~ s/\\$//) {
		# Kill trailing whitespace when joining lines
		$line =~ s/\s$//;
		# line ends with \, glue next line to this one
		my $nextline = <STDIN>;
		$line .= $nextline;
		if($debug) { printf "Continued: '%s'\n", $nextline; }
	}

	# lines starting with -- are comments
	if ($line =~ m/^\s*--/) {
		$wrappers_c_buffer .= $token_cache = $line;
		next
	}

	# Add the line to the output files if it's not a WRAP, EXPORT or
	# GATE line
	my $src_comment = $line;
	if (not ($line =~ m/^(WRAP|EXPORT|GATE)/i)) {
		$wrappers_c_buffer .= "$src_comment\n";

		# Add the line to the output H file
		$export_h_buffer .= "$src_comment\n";
	}

	# replace multiple whitespaces by single spaces:
	$line =~ s/\s+/ /g;

	# Kill off trailing whitespace.
	$line =~ s/\s$//;

	# Split to fields. 1st=command, 2nd=function, 3rd=modifiers
	my @field = split(/\s*:\s*/, $line);

	# Order the cached pre-processor token first
	if ($token_cache) {
		$export_h_buffer .= $token_cache;
		$token_cache = "";
	}

	if(($field[0] eq 'WRAP') || ($field[0] eq 'GATE')) {
		# Generate a wrapper of a gate
		command_wrap_or_gate(@field);
	} elsif($field[0] eq 'EXPORT') {
		# don't generate anything, but tell ld to export a function
		command_export(@field);
	} else {
		# just pass it through to the generated file
	}
}

if($num_errors) {
	print "Failed ($num_errors errors).\n";
	exit(1);
}

# No errors - write output files.

my $file_header_comment = "/* Automatically generated file. Do not edit. */\n";

if(defined $wrappers_c_output_file) {
	write_output_file($wrappers_c_output_file,
		$file_header_comment.
		'#include "libsb2.h"'."\n".
		'#include "exported.h"'."\n".
		$wrappers_c_buffer);
}
if(defined $export_h_output_file) {
	write_output_file($export_h_output_file,
		$file_header_comment.$export_h_buffer);
}
if(defined $export_list_for_ld_output_file) {
	my $export_list_for_ld = join("\n",sort(keys(%all_function_names)));

	write_output_file($export_list_for_ld_output_file,
		$export_list_for_ld);
}

exit(0);
