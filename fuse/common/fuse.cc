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

#include <iostream> // for usage message (TODO: Eliminate somehow)

namespace FUSE
{

template<class Base>
int SubdirectoryFilesystem<Base>::set_base_dir(const std::string &base_dir)
{
    _base_dir = base_dir;
    while(!_base_dir.empty() && _base_dir[_base_dir.size()-1] == '/') // Remove this stmt.?
        _base_dir.resize(_base_dir.size() - 1);
    if(!_base_dir.empty()) {
        // Current version adds '/' ahead if missing, but this behavior may
        // change in future versions:
        if(_base_dir[0] != '/') _base_dir = '/' + _base_dir;
        _base_dir = _base_dir + '/';
    }
    return 0;
}

template<class Base>
void SubdirectoryFilesystem<Base>::usage(const char *prog, struct fuse_args *outargs)
{
  std::cerr <<
"root dir options:\n"
"    -o path=/<DIR>, --path=/<DIR>\n"
"                           directory used as filesystem root\n"
      << std::endl;
  Base::usage(outargs->argv[0], outargs);
}

} // namespace FUSE
