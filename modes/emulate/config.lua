-- Copyright (c) 2012 Nokia Corporation.
-- Author: Lauri T. Aarnio
-- Licensed under MIT license.
--
-- Common config for the "emulate" mode

-- disable the gcc toolchain tricks. gcc & friends will be available, if
-- those have been installed to target_root (but then they will probably run
-- under cpu transparency = very slowly..)
enable_cross_gcc_toolchain = false

