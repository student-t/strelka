// -*- mode: c++; indent-tabs-mode: nil; -*-
//
// Strelka - Small Variant Caller
// Copyright (c) 2009-2016 Illumina, Inc.
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

///
/// \author Chris Saunders
///

#pragma once

#include "somaticVariantEmpiricalScoringFeatures.hh"
#include "calibration/VariantScoringModelServer.hh"

#include <cassert>

#include <array>
#include <bitset>
#include <iosfwd>



namespace SOMATIC_VARIANT_VCF_FILTERS
{

enum index_t
{
    // SNVs and indels:
    HighDepth,
    LowEVS,
    // SNVs only:
    BCNoise,
    SpanDel,
    QSS_ref,
    // indels only:
    Repeat,
    iHpol,
    IndelBCNoise,
    QSI_ref,
    Nonref,
    SIZE
};

inline
const char*
get_label(const unsigned idx)
{
    switch (idx)
    {
    case HighDepth:
        return "HighDepth";
    //return "DP"; // old strelka workflow name
    case BCNoise:
        return "BCNoise";
    case SpanDel:
        return "SpanDel";
    case QSS_ref:
        return "QSS_ref";
    case Repeat:
        return "Repeat";
    case iHpol:
        return "iHpol";
    case IndelBCNoise:
        return "BCNoise";
    case QSI_ref:
        return "QSI_ref";
    case LowEVS:
        return "LowEVS";
    case Nonref:
        return "Nonref";
    default:
        assert(false && "Unknown vcf filter id");
        return nullptr;
    }
}
}


struct strelka_filter_keeper
{
    strelka_filter_keeper()
    {
        clear();
    }

    void
    set(const SOMATIC_VARIANT_VCF_FILTERS::index_t i)
    {
        if (_filters.test(i))
        {
            assert(false && "Setting filter twice");
        }
        _filters.set(i);
    }

    void
    write(
        std::ostream& os) const
    {
        if (_filters.none())
        {
            os << "PASS";
            return;
        }

        bool is_sep(false);
        for (unsigned i(0); i<SOMATIC_VARIANT_VCF_FILTERS::SIZE; ++i)
        {
            if (! _filters.test(i)) continue;

            if (is_sep)
            {
                os << ";";
            }
            else
            {
                is_sep=true;
            }
            os << SOMATIC_VARIANT_VCF_FILTERS::get_label(i);
        }
    }

    void
    clear()
    {
        _filters.reset();
    }

private:
    std::bitset<SOMATIC_VARIANT_VCF_FILTERS::SIZE> _filters;
};


template<typename _evs_featureset,
         typename _evs_dev_featureset>
struct strelka_shared_modifiers
{
    bool isEVS = false;
    double EVS = 0;
    strelka_filter_keeper filters;
    VariantScoringFeatureKeeper<_evs_featureset> features;
    VariantScoringFeatureKeeper<_evs_dev_featureset> dfeatures;
};


typedef strelka_shared_modifiers<SOMATIC_SNV_SCORING_FEATURES, SOMATIC_SNV_SCORING_DEVELOPMENT_FEATURES> strelka_shared_modifiers_snv;
typedef strelka_shared_modifiers<SOMATIC_INDEL_SCORING_FEATURES, SOMATIC_INDEL_SCORING_DEVELOPMENT_FEATURES> strelka_shared_modifiers_indel;

