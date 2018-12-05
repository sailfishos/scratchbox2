/*
    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
*/

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

#include "libsb2.h"
#include "exported.h"

int system_gate(
	int *result_errno_ptr,
	int (*real_system_ptr)(const char *line),
        const char *realfnname, const char *line)
{
	int	result;

	(void)real_system_ptr;
	(void)realfnname;
	SB_LOG(SB_LOGLEVEL_DEBUG, "system(%s)", line);
	errno = *result_errno_ptr; /* restore to orig.value */
	result = __libc_system(line);
	*result_errno_ptr = errno;
	return(result);
}
