#!/usr/bin/perl
#
# gen-wrappers.pl -- an interface generator for scratchbox2 preload library
#
# Copyright (C) 2007 Lauri T. Aarnio
# Copyright (C) 2020 Open Mobile Platform LLC.
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
#   - 1st field is a command (WRAP, GATE, EXPORT or LOGLEVEL)
#   - 2nd field is a function definition (using 100% standard C syntax)
#   - 3rd (optional) field may contain modifiers for the command.
# Fields are separated by colons (:), and one logical line line can be
# split to several physical lines by using a backslash as the last character
# of a line.
#
# Command "LOGLEVEL" specifies what level will be used for SB_LOG() calls.
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
#     the sbox_map_path() function
#   - "map_at(fdname,varname)" will map function's parameter "varname" using
#     the sbox_map_path_at() function
#   - "hardcode_param(N,name)" will hardcode name of the Nth parameter
#     to "name" (this is typically needed only if the function definition uses
#     macros to build the parameter list, instead of specifying names of
#     all parameters in the definition itself):
#   - "optional_arg_is_create_mode" handles varargs for open() etc.,
#     where an optional 3rd arg is "mode".
#   - "returns_string" indicates that the return value (which should be 
#     "char *") can be safely logged with SB_LOG. Note that other pointers
#     as return values will be logged as "NULL" or "not null"
#   - fail_if_readonly(varname,return_value,error_code) and
#     check_and_fail_if_readonly(extra_check,varname,return_value,error_code)
#     will check if the mapped path has been marked "readonly" by the mapping
#     rules, and fail if it is (the latter modifier also makes an extra user-
#     provided check). "varname" must be the same name which was specified
#     to map() or map_at(). "error_code" will be assigned to errno, and
#     the failure will always be logged (SB_LOG_NOTICE level)
#   - "dont_resolve_final_symlink" is used to prefix "map" modifiers where the
#     final symbolic link should not be followed, because the call operates
#     on the symlink itself (for example, see the difference between stat() and
#     lstat()). NOTE: THIS MUST BE USED BEFORE THE map() OR map_at() MODIFIERS!
#   - "dont_resolve_final_symlink_if(condition)" conditionally
#     leaves the final symlink unresolved (see "dont_resolve_final_symlink")
#     NOTE: THIS MUST BE USED BEFORE THE map() OR map_at() MODIFIERS!
#   - "resolve_final_symlink" is the opposite of "dont_resolve_final_symlink"
#     (and it is on by default)
#   - "allow_nonexistent" is used to prefix "map" modifiers where the
#     whole pathname (not just the last component) is allowed to refer
#     to nonexistent file in nonexistent directory.
#     NOTE: THIS MUST BE USED BEFORE THE map() OR map_at() MODIFIERS!
#   - "allow_nonexistent_if(condition)" conditionally
#     allows the pathname to refer to nonexistent file in nonexistent directory
#     (see "allow_nonexistent")
#     NOTE: THIS MUST BE USED BEFORE THE map() OR map_at() MODIFIERS!
#   - "postprocess(varname)" can be used to call  postprocessor functions for
#     mapped variables.
#   - "return(expr)" can be used to alter the return value.
#   - "create_nomap_nolog_version" creates a direct interface function to the
#     next function (for internal use inside the preload library)
#   - "no_libsb2_init_check" disables the call to sb2_initialize_global_variables()
#   - "log_params(sb_log_params)" calls SB_LOG(sb_log_params); this can be
#     used to log parameters of the call.
#   - class(CLASSNAME,...) defines API class attributes (a comma-separated
#     list can be used to specify multiple classes). The classname
#     can be OPEN, STAT, EXEC, or other pre-defined name (see
#     SB2_INTERFACE_CLASS_* constant definitions)
#   - conditionally_class(CONDITION,CLASSNAME,...) adds API class attributes
#     if CONDITION is true (a comma-separated list can be used to specify
#     multiple classes, just like for "class").
# For "GATE" only:
#   - "pass_va_list" is used for generic varargs processing: It passes a
#     "va_list" to the gate function.
#
# Command "EXPORT" is used to specify that a function needs to be exported
# from the scratchbox preload library. This does not create any wrapper
# functions, but still puts the prototype to the include file and name of
# the function to the export list.
#
# Command "EXPORT_SYMBOL" can be used to export other symbols than functions
# (e.g. variables)

use strict;

our($opt_d, $opt_W, $opt_E, $opt_L, $opt_M, $opt_n, $opt_m, $opt_V);
use Getopt::Std;
use File::Basename;

# Process options:
getopts("dW:E:L:M:n:m:V:");
my $debug = $opt_d;
my $wrappers_c_output_file = $opt_W;		# -W generated_c_filename
my $export_h_output_file = $opt_E;		# -E generated_h_filename
my $export_list_for_ld_output_file = $opt_L;	# -L generated_list_for_ld
my $export_map_for_ld_output_file = $opt_M;	# -M generated_export_map_for_ld
my $interface_name = $opt_n;			# -n interface_name
my $man_page_output_file = $opt_m;		# -m man_page_file_name
my $vrs = $opt_V;				# -V sb2_version

my $num_errors = 0;

my $man_page_name = "";
my $man_page_sect = "";
if ($man_page_output_file) {
	if ($man_page_output_file =~ m/^(.*\/)(.*)\.([0-9])/) {
		$man_page_name = $2;
		$man_page_sect = $3;
	} else {
		printf "ERROR: failed to extract manual name and section number ".
			"from '%s'\n", $man_page_output_file;
		$num_errors++;
	}
}

my $man_page_body = ".TH $man_page_name $man_page_sect \"\" \"$vrs\" \"libsb2 interface man page\"\n";
my $man_page_tail = "";

# loglevel defaults to a value which a) causes compilation to fail, if
# "LOGLEVEL" was not in interface.master and b) tries to be informative
my $generated_code_loglevel = "LOGLEVEL_statement_missing_from_interface_master";

#============================================
# This will be added to all generated interface functions (unless
# modifier 'no_libsb2_init_check' is present):
# global variables need to be initialized by a function call
# because the library constructor function seems to be unreliable:
# it may not be the first executed function in a multithreaded
# environment!
my $libsb2_initialized_check_for_all_functions =
	"\tif (!sb2_global_vars_initialized__)\n".
	"\t\tsb2_initialize_global_variables();\n";

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
		'orig_funct_def' => $fn_declarator,
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
	return( "\t" . $condition . "{\n".
		"\t\tva_list arg;\n".
                "\t\tva_start (arg, $last_named_var);\n".
                "\t\tmode = va_arg (arg, int);\n".
                "\t\tva_end (arg);\n".
		"\t}\n");
}

sub create_code_for_va_list_get_void_ptr {
	my $last_named_var = shift;

	return( "\t{\n".
		"\t\tva_list arg;\n".
                "\t\tva_start (arg, $last_named_var);\n".
                "\t\topt_arg = va_arg (arg, void*);\n".
                "\t\tva_end (arg);\n".
		"\t}\n");
}

sub process_readonly_check_modifier {
	my $mods = shift;
	my $extra_check = shift;
	my $param_to_be_mapped = shift;
	my $return_value = shift;
	my $error_code = shift;

	my $new_name = "res_mapped__".$param_to_be_mapped.".mres_result_path";
	my $ro_flag = "res_mapped__".$param_to_be_mapped.".mres_readonly";

	if (!defined($mods->{'mapping_results_by_orig_name'}->{$param_to_be_mapped})) {
		printf "ERROR: mapping_results_by_orig_name not found for '%s'\n",
			$param_to_be_mapped;
		$num_errors++;
	}

	if (defined($extra_check)) {
		$extra_check = " && ($extra_check)";
	}

	$mods->{'path_ro_check_code'} .=
		"\tif ($ro_flag$extra_check) {\n".
		"\t\tSB_LOG(SB_LOGLEVEL_NOTICE, ".
		"\"%s returns (%s is readonly) ".
		"$return_value, error_code=$error_code\", ".
		"__func__, ($new_name ? $new_name : \"<empty path>\"));\n".
		"\t\tfree_mapping_results(&res_mapped__".$param_to_be_mapped.");\n";
	if ($error_code ne '') {
		# set errno just before returning
		$mods->{'path_ro_check_code'} .=
			"\t\terrno = $error_code;\n";
	}
	if ($return_value ne '') {
		$mods->{'path_ro_check_code'} .=
			"\t\treturn ($return_value);\n";
	} else {
		$mods->{'path_ro_check_code'} .=
			"\t\treturn;\n";
	}
	$mods->{'path_ro_check_code'} .=
		"\t}\n";
}

sub class_list_to_expr {
	my $class_list = shift;
	my @class_arr;
	my $class;
        foreach $class (split(/,/,$class_list)) {
		push(@class_arr, 'SB2_INTERFACE_CLASS_'.$class);
	}
	return ('('.join(' | ',@class_arr).')');
}

sub expr_to_flagexpr {
	my $expr = shift;
	my $flag = shift;
	return "0" if ($expr eq "0");
	return $flag if $expr eq "1";
	return "($expr ? $flag : 0)";
}

sub flagexpr_join {
	my $result = "0";
	foreach my $flagexpr (@_) {
		if ($flagexpr eq "0") {
			# zero flag, leave result as is
		} elsif ($result eq "0") {
			# zero result, replace with flag
			$result = $flagexpr;
		} else {
			# non-zero result and flag, join with |
			$result .= " | $flagexpr";
		}
	}
	return $result;
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
		'path_nomap_code' => "",
		'path_ro_check_code' => "",
		'free_path_mapping_vars_code' => "",
		'local_vars_for_varargs_handler' => "",
		'va_list_handler_code' => "",
		'va_list_end_code' => "",
		'mapped_params_by_orig_name' => {},
		'mapping_results_by_orig_name' => {},
		'dont_resolve_final_symlink' => 0,
		'allow_nonexistent' => 0,

		'postprocess_vars' => [],
		'return_expr' => undef,

		# processing modifiers may change the parameter list
		# (but always we'll start with a copy of the original names)
		'parameter_names' => [@{$fn->{'parameter_names'}}],
		'parameter_types' => [@{$fn->{'parameter_types'}}],

		'make_nomap_function' => 0,		# flag
		'make_nomap_nolog_function' => 0,	# flag
		'returns_string' => 0,			# flag
		'check_libsb2_has_been_initialized' => 1, # flag
		'log_params' => undef,
		'class' => '0',
		'conditionally_class' => '0',

		# name of the function pointer variable
		'real_fn_pointer_name' => "${fn_name}_next__",

		# Default value to return if error
		# (e.g. if path mapping returns an error,
		# errno will be set and this value will be
		# returned without calling the real function)
		'return_value_if_error' => "-1",
	};

	my $r_param_names = $mods->{'parameter_names'};
	my $r_param_types = $mods->{'parameter_types'};

	my $varargs_handled = 0;

	my $return_statement = "return;";
	my $fn_return_type = $fn->{'fn_return_type'};
	if($fn_return_type ne "void") {
		$return_statement = "return(ret);";

		if ($fn_return_type =~ m/\*/) {
			# return value is a pointer, default to NULL
			$mods->{'return_value_if_error'} = "NULL";
		}
	}

	my $i;
	for($i=0; $i < $num_modifiers; $i++) {
		if($debug) { printf "Modifier:'%s'\n", $modifiers[$i]; }
		if($modifiers[$i] =~ m/^map\((.*)\)$/) {
			my $param_to_be_mapped = $1;

			my $new_name = "mapped__".$param_to_be_mapped;
			my $no_symlink_resolve =
				expr_to_flagexpr($mods->{'dont_resolve_final_symlink'}, "SBOX_MAP_PATH_DONT_RESOLVE_FINAL_SYMLINK");
			my $allow_nonexistent =
				expr_to_flagexpr($mods->{'allow_nonexistent'}, "SBOX_MAP_PATH_ALLOW_NONEXISTENT");
			my $flags = flagexpr_join($no_symlink_resolve, $allow_nonexistent);

			$mods->{'mapped_params_by_orig_name'}->{$param_to_be_mapped} = "res_$new_name.mres_result_path";
			$mods->{'mapping_results_by_orig_name'}->{$param_to_be_mapped} = "res_$new_name";
			$mods->{'path_mapping_vars'} .= 
				"\tmapping_results_t res_$new_name;\n";

			$mods->{'path_mapping_code'} .=
				"\tclear_mapping_results_struct(&res_$new_name);\n".
				"\tsbox_map_path(__func__, ".
					"$param_to_be_mapped, ".
					"$flags, ".
					"&res_$new_name, classmask);\n".
				"\tif (res_$new_name.mres_errno) {\n".
				"\t\tSB_LOG(SB_LOGLEVEL_DEBUG, \"mapping failed, errno %d\",".
					" res_$new_name.mres_errno);\n".
				"\t\terrno = res_$new_name.mres_errno;\n".
				"\t\tfree_mapping_results(&res_$new_name);\n".
				"\t\t$return_statement\n".
				"\t}\n";
			if ($command eq 'GATE') {
				$mods->{'path_nomap_code'} .=
					"\tclear_mapping_results_struct(&res_$new_name);\n".
					"\tforce_path_to_mapping_result(&res_$new_name, $param_to_be_mapped);\n";
			}
			$mods->{'free_path_mapping_vars_code'} .=
				"\tfree_mapping_results(&res_$new_name);\n";

			# Make a "..._nomap" version, because the main
			# wrapper has mappings.
			$mods->{'make_nomap_function'} = 1;
		} elsif($modifiers[$i] =~ m/^postprocess\((.*)\)$/) {
			my $param_to_postprocess = $1;

			if (defined($mods->{'mapped_params_by_orig_name'}->{$param_to_postprocess}) ||
			    ($param_to_postprocess eq '')) {
				push(@{$mods->{'postprocess_vars'}},
					$param_to_postprocess);
			} else {
				printf "ERROR: can't postprocess ".
					"%s (parameter not mapped)\n",
					$param_to_postprocess;
				$num_errors++;
			}
		} elsif($modifiers[$i] =~ m/^return\((.*)\)$/) {
			$mods->{'return_expr'} = $1;
		} elsif($modifiers[$i] =~ m/^map_at\((.*),(.*)\)$/) {
			my $fd_param = $1;
			my $param_to_be_mapped = $2;

			my $new_name = "mapped__".$param_to_be_mapped;
			my $ro_flag = $param_to_be_mapped."_is_readonly";
			my $no_symlink_resolve =
				expr_to_flagexpr($mods->{'dont_resolve_final_symlink'}, "SBOX_MAP_PATH_DONT_RESOLVE_FINAL_SYMLINK");
			my $allow_nonexistent =
				expr_to_flagexpr($mods->{'allow_nonexistent'}, "SBOX_MAP_PATH_ALLOW_NONEXISTENT");
			my $flags = flagexpr_join($no_symlink_resolve, $allow_nonexistent);

			$mods->{'mapped_params_by_orig_name'}->{$param_to_be_mapped} = "res_$new_name.mres_result_path";
			$mods->{'mapping_results_by_orig_name'}->{$param_to_be_mapped} = "res_$new_name";
			$mods->{'path_mapping_vars'} .= 
				"\tmapping_results_t res_$new_name;\n";
			$mods->{'path_mapping_code'} .=
				"\tclear_mapping_results_struct(&res_$new_name);\n".
				"\tsbox_map_path_at(__func__, ".
					"$fd_param, ".
					"$param_to_be_mapped, ".
					"$flags, ".
					"&res_$new_name, classmask);\n".
				"\tif (res_$new_name.mres_errno) {\n".
				"\t\terrno = res_$new_name.mres_errno;\n".
				"\t\tfree_mapping_results(&res_$new_name);\n".
				"\t\t$return_statement\n".
				"\t}\n";
			if ($command eq 'GATE') {
				$mods->{'path_nomap_code'} .=
					"\tclear_mapping_results_struct(&res_$new_name);\n".
					"\tforce_path_to_mapping_result(&res_$new_name, $param_to_be_mapped);\n";
			}
			$mods->{'free_path_mapping_vars_code'} .=
				"\tfree_mapping_results(&res_$new_name);\n";

			# Make a "..._nomap" version, because the main
			# wrapper has mappings.
			$mods->{'make_nomap_function'} = 1;
		} elsif ($modifiers[$i] =~ m/^fail_if_readonly\((.*),(.*),(.*)\)$/) {
			my $param_to_be_mapped = $1;
			my $return_value = $2;
			my $error_code = $3;

			process_readonly_check_modifier($mods, undef,
				$param_to_be_mapped, $return_value, 
				$error_code);
		} elsif ($modifiers[$i] =~ m/^check_and_fail_if_readonly\((.*),(.*),(.*),(.*)\)$/) {
			my $extra_check = $1;
			my $param_to_be_mapped = $2;
			my $return_value = $3;
			my $error_code = $4;

			process_readonly_check_modifier($mods, $extra_check,
				$param_to_be_mapped, $return_value, 
				$error_code);
		} elsif($modifiers[$i] eq 'create_nomap_nolog_version') {
			$mods->{'make_nomap_nolog_function'} = 1;
		} elsif($modifiers[$i] =~ m/^hardcode_param\((.*),(.*)\)$/) {
			my $param_number = $1;
			my $param_name = $2;
			$r_param_names->[$param_number - 1] = $param_name;
		} elsif(($modifiers[$i] eq 'optional_arg_is_create_mode') &&
			($fn->{'has_varargs'})) {
			$r_param_names->[$varargs_index] = "mode";
			$r_param_types->[$varargs_index] = "int";
			$mods->{'local_vars_for_varargs_handler'} .=
				"\tint mode = 0;\n";
			$mods->{'va_list_handler_code'} =
				create_code_for_va_list_get_mode(
					"", $fn->{'last_named_var'});
			$varargs_handled = 1;
		} elsif($modifiers[$i] eq 'dont_resolve_final_symlink') {
			$mods->{'dont_resolve_final_symlink'} = 1;
		} elsif($modifiers[$i] =~ m/^dont_resolve_final_symlink_if\((.*)\)$/) {
			my $condition = $1;
			$mods->{'dont_resolve_final_symlink'} = "($condition)";
		} elsif($modifiers[$i] eq 'allow_nonexistent') {
			$mods->{'allow_nonexistent'} = 1;
		} elsif($modifiers[$i] =~ m/^allow_nonexistent_if\((.*)\)$/) {
			my $condition = $1;
			$mods->{'allow_nonexistent'} = "($condition)";
		} elsif(($modifiers[$i] =~ m/^optional_arg_is_create_mode\((.*)\)$/) &&
			($fn->{'has_varargs'})) {
			my $va_list_condition = $1;
			$r_param_names->[$varargs_index] = "mode";
			$r_param_types->[$varargs_index] = "int";
			$mods->{'local_vars_for_varargs_handler'} .=
				"\tint mode = 0;\n";
			$mods->{'va_list_handler_code'} =
				create_code_for_va_list_get_mode(
					$va_list_condition,
					$fn->{'last_named_var'});
			$varargs_handled = 1;
		} elsif(($modifiers[$i] =~ m/^optional_arg_is_void_ptr$/) &&
			($fn->{'has_varargs'})) {
			$r_param_names->[$varargs_index] = "opt_arg";
			$r_param_types->[$varargs_index] = "void *";
			$mods->{'local_vars_for_varargs_handler'} .=
				"\tvoid *opt_arg = NULL;\n";
			$mods->{'va_list_handler_code'} =
				create_code_for_va_list_get_void_ptr(
					$fn->{'last_named_var'});
			$varargs_handled = 1;
		} elsif(($modifiers[$i] eq 'pass_va_list') &&
			($fn->{'has_varargs'}) &&
			($command eq 'GATE')) {

			$r_param_names->[$varargs_index] = "ap";
			$r_param_types->[$varargs_index] = "va_list";
			$mods->{'local_vars_for_varargs_handler'} .=
				"\tva_list ap;\n";
			$mods->{'va_list_handler_code'} = "\tva_start(ap,".
				$fn->{'last_named_var'}.");\n";
			$mods->{'va_list_end_code'} = "\tva_end(ap);\n";
			$varargs_handled = 1;
		} elsif($modifiers[$i] eq 'returns_string') {
			$mods->{'returns_string'} = 1;
		} elsif($modifiers[$i] =~ m/^log_params\((.*)\)$/) {
			$mods->{'log_params'} = $1;
		} elsif($modifiers[$i] eq 'no_libsb2_init_check') {
			$mods->{'check_libsb2_has_been_initialized'} = 0;
		} elsif($modifiers[$i] =~ m/^class\((.*)\)$/) {
			if ($mods->{'class'} ne '0') {
				printf "ERROR: redefinition of 'class' for '%s'\n",
					$fn_name;
				$num_errors++;
			} else {
				$mods->{'class'} = class_list_to_expr($1);
			}
		} elsif($modifiers[$i] =~ m/^conditionally_class\(([^,]*),(.*)\)$/) {
			if ($mods->{'conditionally_class'} ne '0') {
				printf "ERROR: redefinition of 'conditionally_class' for '%s'\n",
					$fn_name;
				$num_errors++;
			} else {
				$mods->{'conditionally_class_cnd'} = $1;
				$mods->{'conditionally_class'} = class_list_to_expr($2);
			}
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

sub create_postprocessors {
	my $fn = shift;
	my $mods = shift;

	my $num_params_to_postprocess = @{$mods->{'postprocess_vars'}};
	my $postprocessor_calls = undef;
	my $postprocessor_prototypes = "";
	my $fn_name = $fn->{'fn_name'};

	my $return_value_param_in_call = "";
	my $return_value_param_in_prototype = "";

	my $fn_return_type = $fn->{'fn_return_type'};
	if($fn_return_type ne "void") {
		$return_value_param_in_call = "ret, ";
		$return_value_param_in_prototype = $fn_return_type." ret, ";
	}

	if ($num_params_to_postprocess > 0) {
		$postprocessor_calls = "";

		# insert call to postprocessor for each variable
		my $ppvar;
		foreach $ppvar (@{$mods->{'postprocess_vars'}}) {
			my $pp_fn = "${fn_name}_postprocess_${ppvar}";

			if ($ppvar eq '') {
				# postprocess, no mapped parameter
				$postprocessor_calls .= "$pp_fn(__func__, ".
					$return_value_param_in_call.
					# add orig (unmapped) parameters
					join(", ", @{$mods->{'parameter_names'}}).
					"); ";
				$postprocessor_prototypes .= "extern void ".
					"$pp_fn(const char *realfnname, ".
					$return_value_param_in_prototype.
					# orig (unmapped) parameters
					join(", ", @{$mods->{'parameter_types'}}).
					");\n";
			} else {
				# has mapped parameter
				my $mapping_results = $mods->{'mapping_results_by_orig_name'}->{$ppvar};

				$postprocessor_calls .= "$pp_fn(__func__, ".
					$return_value_param_in_call.
					"&$mapping_results, ".
					# add orig (unmapped) parameters
					join(", ", @{$mods->{'parameter_names'}}).
					"); ";
				$postprocessor_prototypes .= "extern void ".
					"$pp_fn(const char *realfnname, ".
					$return_value_param_in_prototype.
					"mapping_results_t *res, ".
					# orig (unmapped) parameters
					join(", ", @{$mods->{'parameter_types'}}).
					");\n";
			}
		}
	}
	return($postprocessor_calls, $postprocessor_prototypes);
}

sub create_call_to_real_fn {
	my $fn = shift;
	my $mods = shift;
	my @param_list_in_next_call = @_;

	my $real_fn_pointer_name = $mods->{'real_fn_pointer_name'};

	my $postprocessor_calls = undef;
	my $postprocessor_prototypes = "";
	($postprocessor_calls, $postprocessor_prototypes) = 
		create_postprocessors($fn, $mods);

	return(
		# 1. call with mapped parameters
		"(*$real_fn_pointer_name)(".
			join(", ", @param_list_in_next_call).");".
			" result_errno = errno;\n",
		# 2. call with original (unmapped) parameters
		"(*$real_fn_pointer_name)(".
			join(", ", @{$mods->{'parameter_names'}}).");".
			" result_errno = errno;\n",
		# 3. call with original parameters (without logging)
		"(*$real_fn_pointer_name)(".
			join(", ", @{$mods->{'parameter_names'}}).");".
			" result_errno = errno;\n",
		# 4.
		$postprocessor_calls,
		# 5. prototypes.
		$postprocessor_prototypes
	);
}

sub create_call_to_gate_fn {
	my $fn = shift;
	my $mods = shift;
	my @param_list_in_gate_call = @_;

	my @gate_params_with_orig_types = @{$fn->{'all_params_with_types'}};
	my $num_gate_params = @gate_params_with_orig_types;
	my $orig_param_list;
	my $modified_param_list;
	my $gate_params = "&result_errno,".$mods->{'real_fn_pointer_name'}.", __func__";
	my $fn_ptr_prototype_params;
	my $prototype_params;

	# cache some fn parser results to local vars
	my $fn_name = $fn->{'fn_name'};

	if($num_gate_params > 0) {
		my $j;
		my @gate_params_with_types;
		for ($j = 0; $j < $num_gate_params; $j++) {
			my $param_j_name = $fn->{'parameter_names'}->[$j];
			my $mpo = $mods->{'mapped_params_by_orig_name'}->{$param_j_name};

			if (defined $mpo) {
				push @gate_params_with_types, "const mapping_results_t *$param_j_name";
			} else {
				if($debug) {
					print "Gate: ".$fn_name.": $j: MODS: ".
						$mods->{'parameter_types'}->[$j]." ".
						$mods->{'parameter_names'}->[$j]." ### ".
						$fn->{'all_params_with_types'}->[$j]."\n";
				}
				push @gate_params_with_types, $fn->{'all_params_with_types'}->[$j];
			}
		}

		my $varargs_index = $fn->{'varargs_index'};

		if($varargs_index >= 0) {
			$gate_params_with_types[$varargs_index] =
				$mods->{'parameter_types'}->[$varargs_index]." ".
				$mods->{'parameter_names'}->[$varargs_index];
		}
		$orig_param_list = $gate_params.", ".
			join(", ", @{$mods->{'parameter_names'}});
		$modified_param_list = $gate_params.", ".
			join(", ", @param_list_in_gate_call);
		$fn_ptr_prototype_params = join(", ",
			@{$fn->{'all_params_with_types'}});
		$prototype_params =
			"int *result_errno_ptr,\n".
			 $fn->{'fn_return_type'}." ".
			"(*real_${fn_name}_ptr)".
			"($fn_ptr_prototype_params),\n".
			"\tconst char *realfnname, ".
			join(", ", @gate_params_with_types);
	} else {
		$orig_param_list = $gate_params;
		$modified_param_list = $gate_params;
		$fn_ptr_prototype_params = "void";
		$prototype_params =
			"int *result_errno_ptr,\n".
			$fn->{'fn_return_type'}.
			" (*real_${fn_name}_ptr)(void), ".
			"\tconst char *realfnname";
	}

	my $mapped_call = "${fn_name}_gate($modified_param_list);\n";

	my $postprocessor_calls = undef;
	my $postprocessor_prototypes = "";
	($postprocessor_calls, $postprocessor_prototypes) = 
		create_postprocessors($fn, $mods);

	my $unmapped_call = "${fn_name}_gate($modified_param_list);\n";

	# nomap_nolog for a gate is a direct call to the real function
	my $unmapped_nolog_call = "${fn_name}_next__(".
		join(", ", @{$mods->{'parameter_names'}}).
		"); result_errno = errno;\n";

	my $gate_function_prototype =
		"extern ".$fn->{'fn_return_type'}." ${fn_name}_gate(".
			"$prototype_params);\n";

	return($mapped_call, $unmapped_call, $unmapped_nolog_call,
		$gate_function_prototype.$postprocessor_prototypes,
		$postprocessor_calls);
}

#-------------------
# actual code generators:

my $wrappers_c_buffer = "";	# buffers contents of the generated ".c" file

# buffers contents of the generated ".h" file
my $h_file_include_check_macroname = uc($export_h_output_file)."__";
$h_file_include_check_macroname =~ s/\W/_/g;
my $export_h_buffer =
"#ifndef $h_file_include_check_macroname
#define $h_file_include_check_macroname

#include <sys/utsname.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <sys/user.h>
#include <sys/mman.h>

#include <assert.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <dlfcn.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <stdarg.h>
#include <dirent.h>
#include <string.h>
#include <glob.h>
#include <utime.h>
#include <spawn.h>
#ifdef HAVE_FTS_H
#include <fts.h>
#endif
#ifdef HAVE_FTW_H
#include <ftw.h>
#endif
#ifdef HAVE_SHADOW_H
#include <shadow.h>
#endif
#ifdef HAVE_SYS_XATTR_H
#include <sys/xattr.h>
#endif

#include \"mapping.h\"

#if (defined(PROPER_DIRENT) && (PROPER_DIRENT == 1))
typedef const struct dirent *scandir_arg_t;
typedef const struct dirent64 *scandir64_arg_t;
#else
typedef const void scandir_arg_t;
typedef const void scandir64_arg_t;
#endif

";

sub add_fn_to_man_page {
	my $fn = shift;

	$man_page_body .= ".TP\n".$fn->{'orig_funct_def'}.";\n";
}

my %fn_to_classmasks;

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

	# Ok, all preparations done.
	
	# Add it to the document
	add_fn_to_man_page($fn);

	# Create the pointer, wrapper functions, etc.
	if($debug) { print "Creating code:\n"; }

	my $real_fn_pointer_name = $mods->{'real_fn_pointer_name'};
	my $fn_pointer_c_code .=
		"static $fn_return_type ".
		"(*$real_fn_pointer_name)(".
		$fn->{'fn_parameter_list'}.") = NULL;\n\n";

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
	if($command eq 'GATE') {
		# nomap versions of GATEs need the mapping result struct, too
		$nomap_fn_c_code .= $mods->{'path_mapping_vars'};
	}

	# begin the function with "_nomap_nolog" suffix added to name:
	my $nomap_nolog_funct_def = $funct_def;
	$nomap_nolog_funct_def =~ s/($fn_name)/$1_nomap_nolog/;
	$export_h_buffer .= "extern $nomap_nolog_funct_def;\n";
	my $nomap_nolog_fn_c_code .=
		$nomap_nolog_funct_def."\n".
		"{\n".
		$mods->{'local_vars_for_varargs_handler'};

	if($fn_return_type ne "void") {
		my $default_return_value = $mods->{'return_value_if_error'};
		$wrapper_fn_c_code .=	"\t$fn_return_type ret = $default_return_value;\n";
		$nomap_fn_c_code .=	"\t$fn_return_type ret = $default_return_value;\n";
		$nomap_nolog_fn_c_code .= "\t$fn_return_type ret = $default_return_value;\n";
	}
	$wrapper_fn_c_code .=	"\tint saved_errno = errno;\n".
				"\tint result_errno = saved_errno;\n".
				"\tuint32_t classmask = ".$mods->{'class'}.";\n".
				"\t(void)classmask; /* ok, if it isn't used */\n".
				"\terrno = 0;\n";
	if(defined($mods->{'conditionally_class_cnd'})) {
		$wrapper_fn_c_code .=	"\tif(".$mods->{'conditionally_class_cnd'}.") {\n".
				"\t\tclassmask |= (".$mods->{'conditionally_class'}.");\n".
				"\t}\n";
	}
	$nomap_fn_c_code .=	"\tint saved_errno = errno;\n".
				"\tint result_errno = saved_errno;\n";
	$nomap_nolog_fn_c_code .= "\tint result_errno = errno;\n";

	# variables have been introduced, add the code:
	if($mods->{'check_libsb2_has_been_initialized'} != 0) {
		$wrapper_fn_c_code .=		$libsb2_initialized_check_for_all_functions;
		$nomap_fn_c_code .=		$libsb2_initialized_check_for_all_functions;
		$nomap_nolog_fn_c_code .=	$libsb2_initialized_check_for_all_functions;
	}
	if(defined $mods->{'log_params'}) {
		$wrapper_fn_c_code .=		"\tSB_LOG(".$mods->{'log_params'}.");\n";
		$nomap_fn_c_code .=		"\tSB_LOG(".$mods->{'log_params'}.");\n";
	}

	$wrapper_fn_c_code .=		$mods->{'path_mapping_code'}.
					$mods->{'path_ro_check_code'};
	$wrapper_fn_c_code .=		$mods->{'va_list_handler_code'};
	$nomap_fn_c_code .=		$mods->{'path_nomap_code'}.
					$mods->{'va_list_handler_code'};
	$nomap_nolog_fn_c_code .=	$mods->{'va_list_handler_code'};

	my $loglevel_no_real_fn;
	my $no_real_fn_abort_code;
	if($command eq 'WRAP') {
		# Wrappers log an error and abort if the real function
		# does not exist.
		$loglevel_no_real_fn = "SB_LOGLEVEL_ERROR";
		$no_real_fn_abort_code = "abort();";
	} else { # GATE
		# Gates log a warning (but don't abort) if the real function
		# does not exist - the gate function should handle the rest.
		$loglevel_no_real_fn = "SB_LOGLEVEL_WARNING";
		$no_real_fn_abort_code = "/* no abort() */";
	}

	my $check_fn_pointer_log_enabled .=
		"\tif($real_fn_pointer_name == NULL) {\n".
		"\t\t$real_fn_pointer_name = sbox_find_next_symbol(1, ".
			"\"$fn_name\");\n".
		"\t\tif ($real_fn_pointer_name == NULL) {\n".
		"\t\t\tSB_LOG($loglevel_no_real_fn, \"Real '%s'".
			" not found\", \"$fn_name\");\n".
		"\t\t\t$no_real_fn_abort_code\n".
		"\t\t}\n".
		"\t}\n";
	my $check_fn_pointer_log_disabled .=
		"\tif($real_fn_pointer_name == NULL) {\n".
		"\t\t$real_fn_pointer_name = sbox_find_next_symbol(0, ".
			"\"$fn_name\");\n".
		"\t\tif ($real_fn_pointer_name == NULL) {\n".
		"\t\t\t$no_real_fn_abort_code\n".
		"\t\t}\n".
		"\t}\n";
	$wrapper_fn_c_code .=		$check_fn_pointer_log_enabled;
	$nomap_fn_c_code .=		$check_fn_pointer_log_enabled;
	$nomap_nolog_fn_c_code .=	$check_fn_pointer_log_disabled;

	# build the parameter list for the next call..
	my @param_list_in_next_call;
	my @param_list_in_gate_call;
	my $i;
	for($i=0; $i < $fn->{'num_parameters'}; $i++) {
		my $param_name = $mods->{'parameter_names'}->[$i];
		my $mapped_param = $mods->{'mapped_params_by_orig_name'}->{$param_name};
		if(defined $mapped_param) {
			push @param_list_in_next_call, $mapped_param;
			my $mapping_result_struct_name = $mods->{'mapping_results_by_orig_name'}->{$param_name};
			push @param_list_in_gate_call, "&$mapping_result_struct_name";
		} else {
			push @param_list_in_next_call, $param_name;
			push @param_list_in_gate_call, $param_name;
		}
	}

	# ..and the actual call.
	my $call_line_prefix = "\t";
	my $return_statement = ""; # return stmt not needed if fn_type==void
	my $log_return_val = ""; 
	if($mods->{'return_expr'}) {
		$return_statement .= "\tret = ".$mods->{'return_expr'}.";\n";
	}
	if($fn_return_type ne "void") {
		$call_line_prefix .= "ret = ";
		$return_statement .= "\treturn(ret);\n";
	}

	# create a call to log the return
	my $log_return_value_format = undef;
	my $log_return_val = "ret";
	if($fn_return_type eq "int") {
		$log_return_value_format = "%d";
	} elsif($fn_return_type eq "long") {
		$log_return_value_format = "%ld";
	} elsif($fn_return_type =~ m/\*$/) {
		# Last char of return type is a * => it returns a pointer
		if($mods->{'returns_string'}) {
			$log_return_value_format = "'%s'";
			$log_return_val = "(ret ? ret : \"<NULL>\")";
		} else {
			# a pointer to non-printable data. 
			# Log if it is a NULL or not.
			$log_return_value_format = "%s";
			$log_return_val = "(ret ? \"not null\" : \"NULL\")";
		}
	}
	# NOTE: this code prints the numeric value of errno, since there
	# is no fully portable and thread-safe way to get the string
	# representation of the error message (sys_errlist is nonstandard,
	# and there are two different implemetations of strerror_r() :-(
	if(defined $log_return_value_format) {
		$log_return_val = "\tSB_LOG($generated_code_loglevel, ".
			"\"%s returns ".
			"$log_return_value_format, errno=%d (%s)\", ".
			"__func__, $log_return_val, result_errno, ".
			"(saved_errno != result_errno ? ".
			" \"SET\" : \"unchanged\") );\n";
	} else {
		# don't know how to print the return value itself 
		# (an unknown type or no return value at all), but log errno
		$log_return_val = "\tSB_LOG($generated_code_loglevel, ".
			"\"%s returns,".
			" errno=%d (%s)\", ".
			"__func__, result_errno, ".
			"(saved_errno != result_errno ? ".
			" \"SET\" : \"unchanged\") );\n";
	}
	
	my $mapped_call;
	my $unmapped_call;
	my $unmapped_nolog_call;
	my $postprocesors;
	my $prototypes;
	if($command eq 'WRAP') {
		($mapped_call, $unmapped_call, $unmapped_nolog_call,
		 $postprocesors, $prototypes) = create_call_to_real_fn(
			$fn, $mods, @param_list_in_next_call);
	} else { # GATE
		($mapped_call, $unmapped_call, $unmapped_nolog_call,
		 $prototypes, $postprocesors) = create_call_to_gate_fn(
			$fn, $mods, @param_list_in_gate_call);
	}
	$export_h_buffer .= $prototypes;

	# First restore errno to what it was at entry (the path mapping
	# code might have set it)
	$wrapper_fn_c_code .=		"\terrno = saved_errno;\n";
	$nomap_fn_c_code .=		"\terrno = saved_errno;\n";

	# Next, insert the call to the real function
	# (the call will also copy errno to result_errno)
	$wrapper_fn_c_code .=		$call_line_prefix.$mapped_call;
	$nomap_fn_c_code .=		$call_line_prefix.$unmapped_call;
	$nomap_nolog_fn_c_code .=	$call_line_prefix.$unmapped_nolog_call;

	# calls to postprocessors (if any) before the cleanup 
	if (defined $postprocesors) {
		$wrapper_fn_c_code .=	"\t".$postprocesors."\n";
	}
	
	# cleanup; free allocated variables etc.
	$wrapper_fn_c_code .=		$mods->{'va_list_end_code'};
	$wrapper_fn_c_code .=		$mods->{'free_path_mapping_vars_code'};
	$nomap_fn_c_code .=		$mods->{'va_list_end_code'};
	if($command eq 'GATE') {
		$nomap_fn_c_code .=	$mods->{'free_path_mapping_vars_code'};
	}
	$nomap_nolog_fn_c_code .=	$mods->{'va_list_end_code'};

	$wrapper_fn_c_code .=		$log_return_val.
					"\terrno = result_errno;\n".
					$return_statement."}\n";
	$nomap_fn_c_code .=		$log_return_val.
					"\terrno = result_errno;\n".
					$return_statement."}\n";
	$nomap_nolog_fn_c_code .=	"\terrno = result_errno;\n".
					$return_statement."}\n";

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

	# Finally, add name of the function and the classmask
	# to the table, used to create interface_functions_and_classes
	# table at end.
	$fn_to_classmasks{$fn_name} = $mods->{'class'};
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

# Handle the "EXPORT_SYMBOL" command.
sub command_export_symbol {
	my @field = @_;

	my $sym;
	foreach $sym (@field) {
		# Put the symbol to the symbol table.
		# Main program will create the symbol list for ld, once everything
		# else has been done => we don't need to do anything else here.
		$all_function_names{$sym} = 1;

		if($debug) {
			print "Exports symbol: $sym\n";
		}
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

	# lines starting with @ are documentation
	if ($line =~ m/^\s*@/) {
		chomp($line);
		# Split to fields. 1st=command, 2nd=text
		my @man_field = split(/\s*:\s*/, $line, 2);
		if($man_field[0] eq '@MAN') {
			$man_page_body .= $man_field[1]."\n";
		} elsif($man_field[0] eq '@MAN_TAIL') {
			$man_page_tail .= $man_field[1]."\n";
		} else {
			printf "ERROR: Unknown documentation directive '%s'\n", $man_field[0];
			$num_errors++;
		}
		next
	}

	# Add the line to the output files if it's not a command 
	my $src_comment = $line;
	if (not ($line =~ m/^(WRAP|EXPORT|GATE|LOGLEVEL)/i)) {
		$wrappers_c_buffer .= "$src_comment\n";

		# Add the line to the output H file
		$export_h_buffer .= "$src_comment\n";
	}

	# replace multiple whitespaces by single spaces:
	$line =~ s/\s+/ /g;

	# Kill off trailing whitespace.
	$line =~ s/\s$//;

	# Split to fields. 1st=command, 2nd=function, 3rd=modifiers
	my @field = split(/\s*:\s*/, $line, 3);

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
	} elsif($field[0] eq 'EXPORT_SYMBOL') {
		# don't generate anything, but tell ld to export a symbol
		# (e.g. used for variables)
		command_export_symbol(@field);
	} elsif($field[0] eq 'LOGLEVEL') {
		if(!($field[1] =~ m/^SB_LOGLEVEL_/)) {
			printf "ERROR: LOGLEVEL is not SB_LOGLEVEL_*\n";
			$num_errors++;
		}
		$generated_code_loglevel = $field[1];
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
	my $include_h_file = "";
	
	if(defined $export_h_output_file) {
		my $bn = basename($export_h_output_file);
		$include_h_file = '#include "'.$bn.'"'."\n";
	}
	my $interface_functions_and_classes =
		"interface_function_and_classes_t ".
		"interface_functions_and_classes__".$interface_name."[] = {\n";
	my $fnn;
	foreach $fnn (sort(keys(%fn_to_classmasks))) {
		$interface_functions_and_classes .= "\t{\"".$fnn."\", ".
			$fn_to_classmasks{$fnn}."},\n";
	}
	$interface_functions_and_classes .= "\t{NULL, 0},\n};\n";
	write_output_file($wrappers_c_output_file,
		$file_header_comment.
		'#include "libsb2.h"'."\n".
		$include_h_file.
		$wrappers_c_buffer.
		$interface_functions_and_classes);
}
if(defined $export_h_output_file) {
	write_output_file($export_h_output_file,
		$file_header_comment.$export_h_buffer."\n#endif\n");
}
if(defined $export_list_for_ld_output_file) {
	my $export_list_for_ld = join("\n",sort(keys(%all_function_names)));

	write_output_file($export_list_for_ld_output_file,
		$export_list_for_ld);
}
if(defined $export_map_for_ld_output_file) {
	my $export_map = "{\n\tglobal: ";

	my $sym;
	foreach $sym (sort(keys(%all_function_names))) {
		$export_map .= "$sym; ";
	}
	$export_map .= "\n\tlocal: *;\n";
	$export_map .= "};\n";

	write_output_file($export_map_for_ld_output_file,
		$export_map);
}

if(defined $man_page_output_file) {
	write_output_file($man_page_output_file,
		$man_page_body.$man_page_tail);
}

exit(0);
