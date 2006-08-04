/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <iostream> // TODO: Eliminate

#include "fuse.hpp"

#include "config.h"

namespace FUSE
{

  extern "C"
   int Filesystem_process_options_wrapper
      (void *data, const char *arg, int key, struct fuse_args *outargs)
    {
      const FilesystemOptionsProcessor *datast =
        reinterpret_cast<const FilesystemOptionsProcessor *>(data);
      return datast->process_options (datast->_fs, arg, key, *outargs);
    }

    
  void BaseFilesystem::pre_usage(const char *prog, struct fuse_args *outargs)
  {
    std::cerr <<
"usage: " << prog << " mountpoint [options]\n"
"\n"
"general options:\n"
"    -o opt,[opt...]        mount options\n"
"    -h   --help            print help\n"
"    -V   --version         print version\n"
        << std::endl;
  }

  void BaseFilesystem::post_usage(const char *prog, struct fuse_args *outargs)
  {
    ::fuse_opt_add_arg(outargs, "-ho"); // HACK
    const struct fuse_operations oper = {};
    ::fuse_main(outargs->argc, outargs->argv, &oper);
    ::exit(1);
  }

  void BaseFilesystem::print_version(const char *prog, struct fuse_args *outargs)
  {
    std::cerr << "FUSE++ version: " << PACKAGE_VERSION << std::endl;
#if FUSE_VERSION >= 25
    ::fuse_opt_add_arg(outargs, "--version"); // HACK
    const struct fuse_operations oper = {};
    ::fuse_main(outargs->argc, outargs->argv, &oper);
#endif
    ::exit(0);
  }

} // namespace FUSE
