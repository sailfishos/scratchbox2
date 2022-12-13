/*
 * libsb2 -- dbus-related GATE fucntions of the scratchbox2 preload library
 *
 * Copyright (C) 2022 Jolla Ltd.
*/

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

#include <stdint.h>
#include <dbus/dbus-shared.h>
#include "libsb2.h"
#include "exported.h"

static const char *SYSTEM_BUS_REDIRECT = NULL;

static void *find_symbol(const char *fn_name)
{
    char *msg;
    void *fn_ptr;

    fn_ptr = dlsym(RTLD_DEFAULT, fn_name);

    if ((msg = dlerror()) != NULL) {
        SB_LOG(SB_LOGLEVEL_WARNING, "dbusgates.c: dlsym(%s): %s",
                fn_name, dlerror());
        return NULL;
    }

    return fn_ptr;
}

static void *dbus_connection_open_helper(const char *address, void *error)
{
    // Cannot depend on libdbus here as it depends on libpthread - see the
    // note in Makefile.
    static int loaded = 0;
    static int loaded_ok = 1;
    static void * (*dbus_connection_open_private_ptr)(
            const char *address, void *error);
    static uint32_t (*dbus_bus_register_ptr)(
            void *connection, void *error);
    static void (*dbus_connection_unref_ptr)(void *connection);

    void *connection = NULL;

    if (!loaded) {
        dbus_connection_open_private_ptr = find_symbol("dbus_connection_open_private");
        loaded_ok = loaded_ok && (dbus_connection_open_private_ptr != NULL);
        dbus_bus_register_ptr = find_symbol("dbus_bus_register");
        loaded_ok = loaded_ok && (dbus_bus_register_ptr != NULL);
        dbus_connection_unref_ptr = find_symbol("dbus_connection_unref");
        loaded_ok = loaded_ok && (dbus_connection_unref_ptr != NULL);
        loaded = 1;
    }

    if (!loaded_ok) {
        // find_symbol is verbose, no need to say more here
        return NULL;
    }

    connection = (*dbus_connection_open_private_ptr)(address, error);
    if (connection == NULL)
        return NULL;

    if (!(*dbus_bus_register_ptr)(connection, error)) {
        (*dbus_connection_unref_ptr)(connection);
        connection = NULL;
        return NULL;
    }

    return connection;
}

void * dbus_bus_get_private_gate(
    int *result_errno_ptr,
    void * (*real_dbus_bus_get_private_ptr)(int type, void *error),
    const char *realfnname,
    int type,
    void *error)
{
    void *connection = NULL;

    if (type == DBUS_BUS_SYSTEM && SYSTEM_BUS_REDIRECT) {
        SB_LOG(SB_LOGLEVEL_NOISE, "%s: Redirecting D-Bus connection to '%s'",
                realfnname, SYSTEM_BUS_REDIRECT);

        errno = *result_errno_ptr; /* restore to orig.value */
        connection = dbus_connection_open_helper(SYSTEM_BUS_REDIRECT, error);
        if (connection)
            return connection;

        SB_LOG(SB_LOGLEVEL_WARNING, "%s: Failed to redirect D-Bus connection",
                realfnname);
    }

    errno = *result_errno_ptr; /* restore to orig.value */
    connection = (*real_dbus_bus_get_private_ptr)(type, error);
    *result_errno_ptr = errno;
    return connection;
}

void * px_proxy_factory_new_gate(
    int *result_errno_ptr,
    void * (*real_px_proxy_factory_new_ptr)(void),
    const char *realfnname)
{
    static const char *system_bus_redirect = NULL;
    void *rv = NULL;

    if (!system_bus_redirect) {
        system_bus_redirect = ruletree_catalog_get_string(
                "config", "sbox_pacrunner_dbus_bus_address");
        SB_LOG(SB_LOGLEVEL_NOISE, "%s: Will do D-Bus redirection to '%s'",
                realfnname, system_bus_redirect);
    }

    if (system_bus_redirect && *system_bus_redirect) {
        SYSTEM_BUS_REDIRECT = system_bus_redirect;

        SB_LOG(SB_LOGLEVEL_NOISE, "%s: Enabling D-Bus redirection to '%s'",
                realfnname, SYSTEM_BUS_REDIRECT);
    }

    errno = *result_errno_ptr; /* restore to orig.value */
    rv = (*real_px_proxy_factory_new_ptr)();
    *result_errno_ptr = errno;

    if (system_bus_redirect && *system_bus_redirect) {
        SB_LOG(SB_LOGLEVEL_NOISE, "%s: Disabling D-Bus redirection", realfnname);

        SYSTEM_BUS_REDIRECT = NULL;
    }

    return rv;
}
