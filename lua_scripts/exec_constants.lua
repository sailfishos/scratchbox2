-- Copyright (c) 2012 Intel Corporation
-- Author: Mika Westerberg <mika.westerberg@linux.intel.com>
--
-- License: LGPL-2.1

--
-- Constants which can be used from the exec policy files. These are loaded
-- to the lue interpreter before the corresponding exec_config.lua file is
-- processed.
--

--
-- Flags that can be passed with exec_policy in "exec_flags" field. Make
-- sure that these match for the C versions defined in exec/sb2_exec.h.
--
EXEC_FLAGS_FORCE_CPU_TRANSPARENCY	= 0x1
