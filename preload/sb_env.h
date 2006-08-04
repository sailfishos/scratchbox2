/*
 * Copyright (C) 2005 Nokia
 * Author: Toni Timonen <ttimonen@movial.fi>
 *
 * This file may be used, modified, and distributed under the terms
 * of the GNU General Public License version 2.
 */

#ifndef SB_ENV_H
#define SB_ENV_H

char const* * override_sbox_env(char const* const*oldenv);
void replace_sbox_env(char const**env);

#endif
