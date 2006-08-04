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

#include <streambuf>

namespace misc
{
 
  /** streambuf which forwards all operatorions to an other streambuf. */
  template< class CharT, class Traits = std::char_traits<char> >
  class streambuf_forwarder: public std::basic_streambuf<CharT, Traits>
  {
    public:
      typedef std::basic_streambuf<CharT, Traits> base_type;
      typedef typename base_type::char_type char_type;
      typedef typename base_type::int_type int_type;
      typedef typename base_type::pos_type pos_type;
      typedef typename base_type::off_type off_type;
      typedef typename base_type::traits_type traits_type;
    private:
      base_type *_base;
    public:
      streambuf_forwarder() : _base(0) { }
      streambuf_forwarder(base_type *stream) : _base(stream) { }
     ~streambuf_forwarder() { if(_base) _base->pubsync(); }

    protected:
      base_type *get_base_stream()
      {
        return _base;
      }
      void set_base_stream(base_type *stream)
      {
        if(_base) _base->pubsync(); // FIXME: If sync() return -1 should setstate(badbit)
        _base = stream;
      }
      void release_base_stream()
      {
        _base = 0;
      }

    protected:
      // Virtual overrides:
      void imbue (const std::locale &loc)
      { // FIXME: not sure that this the right thing
        base_type::imbue(loc);
        _base->pubimbue(loc);
      }
      pos_type seekoff (off_type off, std::ios_base::seekdir dir, std::ios_base::openmode mode = std::ios_base::in|std::ios_base::out)
      {
        return _base->pubseekoff (off, dir, mode);
      }
      pos_type seekpos (pos_type pos, std::ios_base::openmode mode = std::ios_base::in|std::ios_base::out)
      {
        return _base->pubseekpos (pos, mode);
      }
      int sync ()
      {
        return _base->pubsync ();
      }
      std::streamsize showmanyc ()
      {
        return _base->in_avail();
      }
      std::streamsize xsgetn (char_type *s, std::streamsize n)
      {
        return _base->sgetn (s, n);
      }
      int_type underflow ()
      {
        return _base->sgetc ();
      }
      int_type uflow ()
      {
        return _base->sbumpc ();
      }
      int_type pbackfail (int_type c = traits_type::eof())
      {
        return _base->sungetc ();
      }
      std::streamsize xsputn (const char_type *s, std::streamsize n)
      {
        return _base->sputn (s, n);
      }
      int_type overflow (int_type c = traits_type::eof()) {
        return _base->sputc (c);
      }
  };

#if 0 // It was a stupid idea
  // Methods of this class never call positioning methods of Base.
  // Current version of this class has one position (not separate positions for read and write).
  template<class Base>
  class streambuf_which_remembers_position: public Base
  {
      streampos _gpos, _ppos;
    public:
      typedef Base base_type;
      streambuf_which_remembers_position() : _gpos (0), _ppos (0) { }
    protected:
      // Virtual overrides:
      // This function does not need to be so elaborate and so may be faster (not called often anyway).
      pos_type seekoff (off_type off, ios_base::seekdir dir, ios_base::openmode mode = ios_base::in|ios_base::out)
      {
        if (dir == ios_base::end) // currently unsupported
            return pos_type(off_type(-1)); // currently unsupported
        const bool testin  = mode & ios_base::in ;
        const bool testout = mode & ios_base::out;
        if (!testin && !testout) return pos_type(off_type(-1));
        switch (dir)
          {
            case ios_base::beg:
              return seekpos (pos, mode);
            case ios_base::cur:
              assert (_gpos == _ppos); // probably should not assert but fail, but C++ std. has no req. anyway
              if (testin ) _gpos += ??;
              if (testout) _ppos += ??;
              return ??;
          }
      }
      pos_type seekpos (pos_type pos, ios_base::openmode /*mode*/ = ios_base::in|ios_base::out)
      {
        // FIXME: take pointer differences in account
        if (mode & ios_base::in ) _gpos = off;
        if (mode & ios_base::out) _ppos = off;
        return off;
      }
      streamsize xsgetn (char_type *s, streamsize n)
      {
        const streamsize res = Base::xsgetn (s, n);
        _gpos += res;
        return res;
      }
      int_type underflow ()
      {
        const int_type res = Base::underflow ();
        ++_gpos;
        return res;
      }
      int_type uflow ()
      {
        const int_type res = Base::uflow ();
        ++_gpos;
        return res;
      }
      int_type pbackfail (int_type c = traits_type::eof())
      {
          ??
        return Base::pbackfail (c);
      }
      streamsize xsputn (const char_type *s, streamsize n)
      {
        const streamsize res = Base::xsputn (s, n);
        _ppos += res;
        return res;
      }
      int_type overflow (int_type c = traits_type::eof()) {
        const int_type res = Base::overflow (c);
        ++_ppos;
        return res;
      }
  };
#endif // 0

#if 0
  class reconnect_streambuf
  {
      int tries;
    public:
      void reconnect()
      {
        --tries;
        if(!tries) throw std::ios_base::failure ("Max number of attempts exceeded");
      }
    protected:
      // Virtual overrides:
//pos_type 	seekoff (off_type, ios_base::seekdir, ios_base::openmode=ios_base::in|ios_base::out)
//pos_type 	seekpos (pos_type, ios_base::openmode=ios_base::in|ios_base::out)
int 	sync ()
streamsize 	showmanyc ()
streamsize 	xsgetn (char_type *s, streamsize n)
int_type 	underflow ()
int_type 	uflow ()
int_type 	pbackfail (int_type=traits_type::eof())
streamsize 	xsputn (const char_type *s, streamsize n)
int_type 	overflow (int_type = traits_type::eof())
  };
#endif // 0
  
} // namespace misc
