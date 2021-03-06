/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2016                                                               *
 *                                                                                       *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this  *
 * software and associated documentation files (the "Software"), to deal in the Software *
 * without restriction, including without limitation the rights to use, copy, modify,    *
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to    *
 * permit persons to whom the Software is furnished to do so, subject to the following   *
 * conditions:                                                                           *
 *                                                                                       *
 * The above copyright notice and this permission notice shall be included in all copies *
 * or substantial portions of the Software.                                              *
 *                                                                                       *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,   *
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A         *
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT    *
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF  *
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE  *
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                                         *
 ****************************************************************************************/

#include <modules/globebrowsing/tile/tileprovider/temporaltileprovider.h>

#include <modules/globebrowsing/tile/tileprovider/cachingtileprovider.h>

#include <ghoul/logging/logmanager.h>

#include "cpl_minixml.h"
#include <fstream>

namespace {
    const std::string _loggerCat = "TemporalTileProvider";

    const char* KeyDoPreProcessing = "DoPreProcessing";
    const char* KeyMinimumPixelSize = "MinimumPixelSize";
    const char* KeyFilePath = "FilePath";
    const char* KeyCacheSize = "CacheSize";
    const char* KeyFlushInterval = "FlushInterval";
}

namespace openspace {
namespace globebrowsing {

const char* TemporalTileProvider::URL_TIME_PLACEHOLDER("${OpenSpaceTimeId}");

const char* TemporalTileProvider::TemporalXMLTags::TIME_START = "OpenSpaceTimeStart";
const char* TemporalTileProvider::TemporalXMLTags::TIME_END = "OpenSpaceTimeEnd";
const char* TemporalTileProvider::TemporalXMLTags::TIME_RESOLUTION =
                                                                "OpenSpaceTimeResolution";
const char* TemporalTileProvider::TemporalXMLTags::TIME_FORMAT = "OpenSpaceTimeIdFormat";

TemporalTileProvider::TemporalTileProvider(const ghoul::Dictionary& dictionary) 
    : _initDict(dictionary) 
{

    if (!dictionary.getValue<std::string>(KeyFilePath, _datasetFile)) {
        throw std::runtime_error(std::string("Must define key '") + KeyFilePath + "'");
    }

    std::ifstream in(_datasetFile.c_str());
    if (!in.is_open()) {
        throw ghoul::FileNotFoundError(_datasetFile);
    }
        
    // read file
    std::string xml(
        std::istreambuf_iterator<char>(in),
        (std::istreambuf_iterator<char>())
    );
    _gdalXmlTemplate = consumeTemporalMetaData(xml);
    _defaultTile = getTileProvider()->getDefaultTile();
}

std::string TemporalTileProvider::consumeTemporalMetaData(const std::string& xml) {
    CPLXMLNode* node = CPLParseXMLString(xml.c_str());

    std::string timeStart = getXMLValue(
        node,
        TemporalXMLTags::TIME_START,
        "2000 Jan 1"
    );
    std::string timeResolution = getXMLValue(
        node,
        TemporalXMLTags::TIME_RESOLUTION,
        "2d"
    );
    std::string timeEnd = getXMLValue(
        node,
        TemporalXMLTags::TIME_END,
        "Now"
    );
    std::string timeIdFormat = getXMLValue(
        node,
        TemporalXMLTags::TIME_FORMAT,
        "YYYY-MM-DDThh:mm:ssZ"
    );

    Time start; start.setTime(timeStart);
    Time end(Time::now());
    if (timeEnd != "Now") {
        end.setTime(timeEnd);
    }

    try {
        _timeQuantizer = TimeQuantizer(start, end, timeResolution);
    }
    catch (const ghoul::RuntimeError& e) {
        throw ghoul::RuntimeError(
            "Could not create time quantizer for Temporal GDAL dataset '" +
            _datasetFile + "'. " + e.message);
    }
    _timeFormat = TimeIdProviderFactory::getProvider(timeIdFormat);
    if (!_timeFormat) {
        throw ghoul::RuntimeError(
            "Invalid Time Format " + timeIdFormat + " in " + _datasetFile
        );
    }

    std::string gdalDescription;
    CPLXMLNode* gdalNode = CPLSearchXMLNode(node, "GDAL_WMS");
    if (gdalNode) {
        gdalDescription = CPLSerializeXMLTree(gdalNode);
    }
    if (!gdalNode) {
        CPLXMLNode* gdalNode = CPLSearchXMLNode(node, "FilePath");
        gdalDescription = std::string(gdalNode->psChild->pszValue);
    }
        
    return gdalDescription;
}

std::string TemporalTileProvider::getXMLValue(CPLXMLNode* root, const std::string& key,
                                              const std::string& defaultVal)
{
    CPLXMLNode * n = CPLSearchXMLNode(root, key.c_str());
    if (!n) {
        throw ghoul::RuntimeError(
            "Unable to parse file " + _datasetFile + ". " + key + " missing."
        );
    }

    bool hasValue =
        (n != nullptr && n->psChild != nullptr && n->psChild->pszValue != nullptr);
    return hasValue ? std::string(n->psChild->pszValue) : defaultVal;
}

TileDepthTransform TemporalTileProvider::depthTransform() {
    ensureUpdated();
    return _currentTileProvider->depthTransform();
}

Tile::Status TemporalTileProvider::getTileStatus(const TileIndex& tileIndex) {
    ensureUpdated();
    return _currentTileProvider->getTileStatus(tileIndex);
}

Tile TemporalTileProvider::getTile(const TileIndex& tileIndex) {
    ensureUpdated();
    return _currentTileProvider->getTile(tileIndex);
}

Tile TemporalTileProvider::getDefaultTile() {
    return _defaultTile;
}

int TemporalTileProvider::maxLevel() {
    ensureUpdated();
    return _currentTileProvider->maxLevel();
}

void TemporalTileProvider::ensureUpdated() {
    if (_currentTileProvider == nullptr) {
        LDEBUG("Warning: update was done lazily");
        update();
    }
}

void TemporalTileProvider::update() {
    auto newCurrent = getTileProvider();
    if (newCurrent) {
        _currentTileProvider = newCurrent;
    }
    _currentTileProvider->update();
}

void TemporalTileProvider::reset() {
    for (auto& it : _tileProviderMap) {
        it.second->reset();
    }
    //auto end = _tileProviderMap.end();
    //for (auto it = _tileProviderMap.begin(); it != end; it++) {
    //    it->second->reset();
    //}
}

std::shared_ptr<TileProvider> TemporalTileProvider::getTileProvider(Time t) {
    Time tCopy(t);
    if (_timeQuantizer.quantize(tCopy, true)) {
        TimeKey timekey = _timeFormat->stringify(tCopy);
        try {
            return getTileProvider(timekey);
        }
        catch (const ghoul::RuntimeError& e) {
            LERROR(e.message);
            return nullptr;
        }
    }
    return nullptr;
}

std::shared_ptr<TileProvider> TemporalTileProvider::getTileProvider(TimeKey timekey) {
    auto it = _tileProviderMap.find(timekey);
    if (it != _tileProviderMap.end()) {
        return it->second;
    }
    else {
        auto tileProvider = initTileProvider(timekey);

        _tileProviderMap[timekey] = tileProvider;
        return tileProvider;
    }
}

std::shared_ptr<TileProvider> TemporalTileProvider::initTileProvider(TimeKey timekey) {
    std::string gdalDatasetXml = getGdalDatasetXML(timekey);
    _initDict.setValue<std::string>(KeyFilePath, gdalDatasetXml);
    auto tileProvider = std::make_shared<CachingTileProvider>(_initDict);
    return tileProvider;
}
    
std::string TemporalTileProvider::getGdalDatasetXML(Time t) {
    TimeKey timekey = _timeFormat->stringify(t);
    return getGdalDatasetXML(timekey);
}

std::string TemporalTileProvider::getGdalDatasetXML(TimeKey timeKey) {
    std::string xmlTemplate(_gdalXmlTemplate);
    size_t pos = xmlTemplate.find(URL_TIME_PLACEHOLDER);
    //size_t numChars = std::string(URL_TIME_PLACEHOLDER).length();
    size_t numChars = strlen(URL_TIME_PLACEHOLDER);
    ghoul_assert(pos != std::string::npos, "Invalid dataset file");
    std::string timeSpecifiedXml = xmlTemplate.replace(pos, numChars, timeKey);
    return timeSpecifiedXml;
}

std::string YYYY_MM_DD::stringify(const Time& t) const {
    return t.ISO8601().substr(0, 10);
}

std::string YYYY_MM_DDThhColonmmColonssZ::stringify(const Time& t) const {
    return t.ISO8601().substr(0, 19) + "Z";
}
    
std::string YYYY_MM_DDThh_mm_ssZ::stringify(const Time& t) const {
    std::string timeString = t.ISO8601().substr(0, 19) + "Z";
    replace( timeString.begin(), timeString.end(), ':', '_' );
    return timeString;
}

bool TimeIdProviderFactory::initialized = false;

std::unordered_map<std::string, std::unique_ptr<TimeFormat>>
    TimeIdProviderFactory::_timeIdProviderMap =
    std::unordered_map<std::string, std::unique_ptr<TimeFormat>>();

void TimeIdProviderFactory::init() {
    _timeIdProviderMap.insert(std::pair<std::string, std::unique_ptr<TimeFormat>>(
        { "YYYY-MM-DD" , std::make_unique<YYYY_MM_DD>() }
    ));
    _timeIdProviderMap.insert(std::pair<std::string, std::unique_ptr<TimeFormat>>(
        { "YYYY-MM-DDThh:mm:ssZ", std::make_unique<YYYY_MM_DDThhColonmmColonssZ>() }
    ));
    initialized = true;
    _timeIdProviderMap.insert(std::pair<std::string, std::unique_ptr<TimeFormat>>(
        { "YYYY-MM-DDThh_mm_ssZ", std::make_unique<YYYY_MM_DDThh_mm_ssZ>() }
    ));
    initialized = true;
}

TimeFormat* TimeIdProviderFactory::getProvider(const std::string& format) {
    if (!initialized) {
        init();
    }
    ghoul_assert(
        _timeIdProviderMap.find(format) != _timeIdProviderMap.end(), 
        "Unsupported Time format: " << format
    );
    return _timeIdProviderMap[format].get();
}

TimeQuantizer::TimeQuantizer(const Time& start, const Time& end, double resolution)
    : _timerange(start.j2000Seconds(), end.j2000Seconds())
    , _resolution(resolution)
{}

TimeQuantizer::TimeQuantizer(const Time& start, const Time& end,
                             const std::string& resolution)
    : TimeQuantizer(start, end, parseTimeResolutionStr(resolution))
{}

double TimeQuantizer::parseTimeResolutionStr(const std::string& resoltutionStr) {
    const char unit = resoltutionStr.back();
    std::string numberString = resoltutionStr.substr(0, resoltutionStr.length() - 1);
        
    char* p;
    double value = strtol(numberString.c_str(), &p, 10);
    if (*p) { // not a number
        throw ghoul::RuntimeError("Cannot convert " + numberString + " to number");
    }
    else {
        // convert value to seconds, based on unit.
        // The switch statment has intentional fall throughs
        switch (unit) {
        case 'y': value *= 365;
        case 'd': value *= 24.0;
        case 'h': value *= 60.0;
        case 'm': value *= 60.0;
        case 's': value *= 1.0;
            break;
        default:
            throw ghoul::RuntimeError("Invalid unit format '" + std::string(1, unit) +
                "'. Expected 'y', 'd', 'h', 'm' or 's'.");
        }
        return value;
    }
}

bool TimeQuantizer::quantize(Time& t, bool clamp) const {
    double unquantized = t.j2000Seconds();
    if (_timerange.includes(unquantized)) {
        double quantized = std::floor((unquantized - _timerange.start) / _resolution) * _resolution + _timerange.start;
        t.setTime(quantized);
        return true;
    }
    else if (clamp) {
        double clampedTime = unquantized;
        clampedTime = std::max(clampedTime, _timerange.start);
        clampedTime = std::min(clampedTime, _timerange.end);
        t.setTime(clampedTime);
        return true;
    }
    else {
        return false;
    }
}

} // namespace globebrowsing
} // namespace openspace
