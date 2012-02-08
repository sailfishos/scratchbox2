/*
 * Copyright (C) 2012 Nokia Corporation.
 * Licensed under LGPL version 2.1, see top level LICENSE file for details.
 * Author: Lauri T. Aarnio
 *
 * This file contains private defitions and interfaces of sb2's
 * exec subsystem.
*/

#ifndef __EXEC_INTERNAL_H
#define __EXEC_INTERNAL_H

#include "rule_tree.h"

extern int apply_exec_preprocessing_rules(char **file, char ***argv, char ***envp);

extern const char *find_exec_policy_name(const char *mapped_path, const char *virtual_path);

#endif /* __EXEC_INTERNAL_H */

