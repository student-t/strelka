//
// Strelka - Small Variant Caller
// Copyright (c) 2009-2018 Illumina, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
//

/// \file
/// \author Chris Saunders
///

#pragma once

#include <iosfwd>
#include <memory>
#include <string>



/// provide an output stream which comes from either a file or a tty default
///
struct OutStream
{
    explicit
    OutStream(const std::string& fileName);

    ~OutStream();

    std::ostream&
    getStream()
    {
        if (! _isInit) initStream();
        return *_osptr;
    }

private:

    void
    initStream();

    static
    void
    openFile(
        const std::string& filename,
        std::ofstream& ofs);

    bool _isInit;
    std::string _fileName;
    std::ostream* _osptr;
    std::unique_ptr<std::ofstream> _ofsptr;
};
