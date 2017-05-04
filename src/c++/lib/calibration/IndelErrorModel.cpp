//
// Strelka - Small Variant Caller
// Copyright (c) 2009-2017 Illumina, Inc.
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

#include "IndelErrorModel.hh"
#include "blt_util/log.hh"
#include "common/Exceptions.hh"

#include "json/json.h"

#include <cmath>
#include <cassert>

#include <fstream>
#include <iostream>


/// \brief Provide simple static indel error rates.
///
/// Provides a single log-linear ramp for homopolymer lengths 1-16. These rates are set empirically. In
/// practice these are scaled up if used for germline likelihood computations.
///
/// This was the default static error model used for all cases in NS5/v2.7.x release series.
///
static
IndelErrorRateSet
getLogLinearIndelErrorModel()
{
    static const double logLowErrorRate(std::log(5e-5));
    static const double logHighErrorRate(std::log(3e-4));

    // this is the zero-indexed endpoint of the ramp, so we hit the
    // constant high error rate at an hpol length of repeatCountSwitchPoint+1
    static const unsigned repeatCountSwitchPoint(15);

    IndelErrorRateSet rates;

    // model covers homopolymers only:
    static const unsigned repeatingPatternSize(1);

    for (unsigned patternRepeatCount=1; patternRepeatCount <= (repeatCountSwitchPoint+1); ++patternRepeatCount)
    {
        const double highErrorFrac(std::min((patternRepeatCount-1),repeatCountSwitchPoint)/static_cast<double>(repeatCountSwitchPoint));
        const double logErrorRate((1.-highErrorFrac)*logLowErrorRate + highErrorFrac*logHighErrorRate);
        const double errorRate(std::exp(repeatingPatternSize==1 ? logErrorRate : logLowErrorRate));

        rates.addRate(repeatingPatternSize, patternRepeatCount, errorRate, errorRate);
    }
    return rates;
}



/// \brief Provide static indel error rates developed from pattern analyzer 'model 3' estimates.
///
/// Provides a set of error rates using a single value for the non-STR state, a log-linear ramp
/// for homopolymer lengths 2-16, and a log-linear ramp for dinucleotide repeat counts 2-9.
///
/// The parameters here are designed to correspond to the adaptive indel error estimates computed
/// from the input data. These can be used under any circumstance where adaptive estimation is not
/// practical. The parameters are based on the geometric average of adaptive parameter estimates
/// from 'typical' Nano and PCR-free samples, with minor empirical adjustments.
///
static
IndelErrorRateSet
getSimplifiedAdaptiveParameters()
{
    IndelErrorRateSet rates;

    // the preset values for the indel error model
    const double nonStrRate(8e-3);
    const std::vector<unsigned> repeatingPatternSizeVector = {1, 2};
    const std::vector<double> logLowErrorRateVector = {std::log(4.9e-3), std::log(1.0e-2)};
    const std::vector<double> logHighErrorRateVector = {std::log(4.5e-2), std::log(1.8e-2)};
    const std::vector<unsigned> repeatCountSwitchPointVector = {16,9};
    const unsigned numberOfPatternSizes = repeatingPatternSizeVector.size();

    assert(logLowErrorRateVector.size() == numberOfPatternSizes &&
           logHighErrorRateVector.size() == numberOfPatternSizes &&
           repeatCountSwitchPointVector.size() == numberOfPatternSizes);


    for (unsigned repeatingPatternSizeIx = 0; repeatingPatternSizeIx < numberOfPatternSizes; repeatingPatternSizeIx++)
    {
        const unsigned repeatingPatternSize(repeatingPatternSizeVector[repeatingPatternSizeIx]);
        const unsigned repeatCountSwitchPoint(repeatCountSwitchPointVector[repeatingPatternSizeIx]);

        AdaptiveIndelErrorModelLogParams lowLogParams;
        lowLogParams.logErrorRate = logLowErrorRateVector[repeatingPatternSizeIx];
        AdaptiveIndelErrorModelLogParams highLogParams;
        highLogParams.logErrorRate = logHighErrorRateVector[repeatingPatternSizeIx];

        AdaptiveIndelErrorModel indelErrorModel(repeatingPatternSize,
                                                repeatCountSwitchPoint,
                                                lowLogParams,
                                                highLogParams);

        unsigned patternRepeatCount = 1;
        rates.addRate(repeatingPatternSize, patternRepeatCount, nonStrRate, nonStrRate);

        for (patternRepeatCount = AdaptiveIndelErrorModel::lowRepeatCount; patternRepeatCount <= repeatCountSwitchPoint; ++patternRepeatCount)
        {
            const double errorRate(indelErrorModel.errorRate(patternRepeatCount));
            rates.addRate(repeatingPatternSize, patternRepeatCount, errorRate, errorRate);
        }
    }
    return rates;
}

void
IndelErrorModel::deserializeIndelModels
(const std::string& modelFilename,
 const Json::Value& root)
{

    Json::Value motifs = root["motifs"];
    if (motifs.isNull())
    {
        using namespace illumina::common;
        std::ostringstream oss;
        oss << "ERROR: no motifs in model file '" << modelFilename << "'\n";
        BOOST_THROW_EXCEPTION(LogicException(oss.str()));
    }

    for (const auto& motifValue : motifs)
    {
        const double indelRate = motifValue["indelRate"].asDouble();
        const double noisyLocusRate = motifValue["noisyLocusRate"].asDouble();
        const unsigned repeatCount = motifValue["repeatCount"].asInt();
        const unsigned repeatPatternSize = motifValue["repeatPatternSize"].asInt();
        _errorRates.addRate(repeatPatternSize, repeatCount, indelRate, indelRate, noisyLocusRate);
    }

}


IndelErrorModel::
IndelErrorModel(
    const std::string& modelName,
    const std::string& modelFilename)
{
    if (modelFilename.empty())
    {
        if (modelName == "logLinear")
        {
            _errorRates = getLogLinearIndelErrorModel();
        }
        else if (modelName == "adaptiveDefault")
        {
            _errorRates = getSimplifiedAdaptiveParameters();
        }
        else
        {
            using namespace illumina::common;

            std::ostringstream oss;
            oss << "ERROR: unrecognized indel error model name: '" << modelName << "'\n";
            BOOST_THROW_EXCEPTION(LogicException(oss.str()));
        }
    }
    else
    {
        std::string jsonString;
        Json::Value root;
        {
            std::ifstream ifs(modelFilename , std::ifstream::binary);
            std::stringstream buffer;
            buffer << ifs.rdbuf();
            jsonString = buffer.str();
        }
        Json::Reader reader;
        if (reader.parse(jsonString, root))
        {
            deserializeIndelModels(modelFilename, root);
        }
        else
        {
            using namespace illumina::common;

            std::ostringstream oss;
            oss << "Failed to parse JSON " << modelFilename << " " << reader.getFormattedErrorMessages() << "'\n";
            BOOST_THROW_EXCEPTION(LogicException(oss.str()));
        }

    }

    _errorRates.finalizeRates();

    // the indel candidate model always uses the v2.7.x log-linear indel error ramp:
    _candidateErrorRates = getLogLinearIndelErrorModel();
    _candidateErrorRates.finalizeRates();
}




void
IndelErrorModel::
getIndelErrorRate(
    const IndelKey& indelKey,
    const AlleleReportInfo& indelReportInfo,
    double& refToIndelErrorProb,
    double& indelToRefErrorProb,
    const bool isCandidateRates) const
{
    using namespace IndelErrorRateType;

    // tmp transition step:
    const IndelErrorRateSet& errorRates(isCandidateRates ? _candidateErrorRates : _errorRates);

    const index_t indelType(getRateType(indelKey));
    // determine simple case
    const bool isSimpleIndel(indelType==INSERT || indelType==DELETE);

    if (! isSimpleIndel)
    {
        // complex indels use baseline indel error rates
        /// TODO - provide estimates for complex indels
        const double baselineInsertionErrorRate(errorRates.getRate(1,1,INSERT));
        const double baselineDeletionErrorRate(errorRates.getRate(1,1,DELETE));

        refToIndelErrorProb=std::max(baselineInsertionErrorRate,baselineDeletionErrorRate);
        indelToRefErrorProb=refToIndelErrorProb;
        return;
    }
    else
    {
        // determine the repeat pattern size and count:
        static const unsigned one(1);
        const unsigned repeatingPatternSize = std::max(indelReportInfo.repeatUnitLength, one);
        const unsigned refPatternRepeatCount = std::max(indelReportInfo.refRepeatCount, one);
        const unsigned indelPatternRepeatCount = std::max(indelReportInfo.indelRepeatCount, one);

        //const int indelPatternRepeatSize(std::abs(static_cast<int>(iri.ref_repeat_count)-static_cast<int>(iri.indel_repeat_count));

        const index_t reverseIndelType((indelType == DELETE) ? INSERT : DELETE);

        refToIndelErrorProb = errorRates.getRate(repeatingPatternSize, refPatternRepeatCount, indelType);
        indelToRefErrorProb = errorRates.getRate(repeatingPatternSize, indelPatternRepeatCount, reverseIndelType);
    }
}



AdaptiveIndelErrorModel::AdaptiveIndelErrorModel(
    unsigned repeatPatternSizeIn,
    unsigned highRepeatCountIn,
    const AdaptiveIndelErrorModelLogParams& lowLogParamsIn,
    const AdaptiveIndelErrorModelLogParams& highLogParamsIn):
    _repeatPatternSize(repeatPatternSizeIn),
    _highRepeatCount(highRepeatCountIn),
    _lowLogParams(lowLogParamsIn),
    _highLogParams(highLogParamsIn)
{
}

double AdaptiveIndelErrorModel::errorRate(const unsigned repeatCount) const
{
    assert(repeatCount > 1);
    if (repeatCount>=_highRepeatCount)
    {
        return std::exp(_highLogParams.logErrorRate);
    }
    return std::exp(linearFit(repeatCount, _lowRepeatCount, _lowLogParams.logErrorRate, _highRepeatCount, _highLogParams.logErrorRate));
}

double AdaptiveIndelErrorModel::noisyLocusRate(const unsigned repeatCount) const
{
    assert(repeatCount > 1);
    if (repeatCount>=_highRepeatCount)
    {
        return std::exp(_highLogParams.logNoisyLocusRate);
    }
    return std::exp(
               linearFit(repeatCount, _lowRepeatCount, _lowLogParams.logNoisyLocusRate, _highRepeatCount, _highLogParams.logNoisyLocusRate));
}

double AdaptiveIndelErrorModel::linearFit(const double x, const double x1, const double y1, const double x2, const double y2)
{
    assert(x1!=x2);
    return ((y2-y1)*x +(x2*y1-x1*y2))/(x2-x1);
}

unsigned AdaptiveIndelErrorModel::lowRepeatCount = 2;
