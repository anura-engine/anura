/*
	Copyright (C) 2003-2013 by David White <davewx7@gmail.com>

  This software is provided 'as-is', without any express or implied
  warranty. In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

     1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgement in the product documentation would be
     appreciated but is not required.

     2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.

     3. This notice may not be removed or altered from any source
     distribution.
*/
#ifndef COMPAT_HPP_INCLUDED
#define COMPAT_HPP_INCLUDED

#ifdef _WINDOWS
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#define strtoll _strtoui64

_Check_return_ inline bool __isblank(_In_ int _C) 
    { return (MB_CUR_MAX > 1 ? _isctype(_C,_BLANK) : __chvalidchk(_C, _BLANK)); }

#endif // _WINDOWS


#endif // COMPAT_HPP_INCLUDED
