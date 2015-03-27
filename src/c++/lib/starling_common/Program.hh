// -*- mode: c++; indent-tabs-mode: nil; -*-
//
// Starka
// Copyright (c) 2009-2014 Illumina, Inc.
//
// This software is provided under the terms and conditions of the
// Illumina Open Source Software License 1.
//
// You should have received a copy of the Illumina Open Source
// Software License 1 along with this program. If not, see
// <https://github.com/sequencing/licenses/>
//

/// \author Chris Saunders
///

#pragma once

namespace starka
{

/// base-class for all command-line programs
///
/// this is used to standardize bottom-level exception handling
struct Program
{
    virtual
    ~Program() {}

    int
    run(int argc, char* argv[]) const;

    virtual
    const char*
    name() const = 0;

    const char*
    version() const;

protected:
    virtual
    void
    runInternal(int argc, char* argv[]) const = 0;
};

}
