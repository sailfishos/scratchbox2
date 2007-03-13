/*
 * Copyright (C) 2007 Lauri Leukkunen <lle@rahina.org>
 *
 * Licensed under LGPL 2.1, see toplevel LICENSE file for details.
 *
 * emumode.c implements target emulation mapping profile.
 * It maps everything except /tmp, /dev, /proc and /sys to
 * $SBOX_TARGET_ROOT. It doesn't use the cache so there's
 * no conflict between emumode and the default mapping mode.
 */

#include <mapping.h>

char *emumode_map(const char *path)
{
}
