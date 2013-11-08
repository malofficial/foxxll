/***************************************************************************
 *  lib/common/version.cpp
 *
 *  Part of the STXXL. See http://stxxl.sourceforge.net
 *
 *  Copyright (C) 2007, 2008, 2011 Andreas Beckmann <beckmann@cs.uni-frankfurt.de>
 *  Copyright (C) 2013 Timo Bingmann <tb@panthema.net>
 *
 *  Distributed under the Boost Software License, Version 1.0.
 *  (See accompanying file LICENSE_1_0.txt or copy at
 *  http://www.boost.org/LICENSE_1_0.txt)
 **************************************************************************/

#include <stxxl/bits/version.h>


__STXXL_BEGIN_NAMESPACE

int version_major()
{
    return STXXL_VERSION_MAJOR;
}

int version_minor()
{
    return STXXL_VERSION_MINOR;
}

int version_patch()
{
    return STXXL_VERSION_PATCH;
}

int version_int()
{
    return STXXL_VERSION_INT;
}

const char * get_library_version_string()
{
    return get_version_string();
}

const char * get_library_version_string_long()
{
    return get_version_string_long();
}

__STXXL_END_NAMESPACE

// vim: et:ts=4:sw=4
