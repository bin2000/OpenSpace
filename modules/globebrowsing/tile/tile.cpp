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

#include <modules/globebrowsing/tile/tiledataset.h>

#include <gdal_priv.h>

namespace {
    const std::string _loggerCat = "Tile";
}

namespace openspace {
namespace globebrowsing {

void TileMetaData::serialize(std::ostream& os) {
    os << maxValues.size() << std::endl;
    for (float f : maxValues) {
        os << f << " ";
    }
    os << std::endl;
    for (float f : minValues) {
        os << f << " ";
    }
    os << std::endl;
}

TileMetaData TileMetaData::deserialize(std::istream& is) {
    TileMetaData res;
    int n; is >> n;
    res.maxValues.resize(n);
    for (int i = 0; i < n; i++) {
        is >> res.maxValues[i];
    }
    res.minValues.resize(n);
    for (int i = 0; i < n; i++) {
        is >> res.minValues[i];
    }

    return std::move(res);
}

RawTile::RawTile()
    : imageData(nullptr)
    , dimensions(0, 0, 0)
    , tileMetaData(nullptr)
    , tileIndex(0, 0, 0)
    , error(CE_None)
    , nBytesImageData(0)
{}
        
RawTile RawTile::createDefaultRes() {
    RawTile defaultRes;
    int w = 8;
    int h = 8;
    defaultRes.dimensions = glm::uvec3(w, h, 1);
    defaultRes.nBytesImageData = w * h * 1 * 3 * 4; // assume max 3 channels, max 4 bytes per pixel
    defaultRes.imageData = new char[defaultRes.nBytesImageData];
    std::fill_n((char*)defaultRes.imageData, defaultRes.nBytesImageData, 0);
    return std::move(defaultRes);
}

void RawTile::serializeMetaData(std::ostream& os) {
    os << dimensions.x << " " << dimensions.y << " " << dimensions.z << std::endl;
    os << tileIndex.x << " " << tileIndex.y << " " << tileIndex.level << std::endl;
    os << error << std::endl;

    // preprocess data
    os << (tileMetaData != nullptr) << std::endl;
    if (tileMetaData != nullptr) {    
        tileMetaData->serialize(os);
    }
        
    os << nBytesImageData << std::endl;
}

RawTile RawTile::deserializeMetaData(std::istream& is) {
    RawTile res;
    is >> res.dimensions.x >> res.dimensions.y >> res.dimensions.z;
    is >> res.tileIndex.x >> res.tileIndex.y >> res.tileIndex.level;
    int err; is >> err; res.error = (CPLErr) err;
        
    res.tileMetaData = nullptr;
    bool hastileMetaData; 
    is >> hastileMetaData;
    if (hastileMetaData) {
        TileMetaData tileMetaData = TileMetaData::deserialize(is);
        res.tileMetaData = std::make_shared<TileMetaData>(tileMetaData);
    }
        
    is >> res.nBytesImageData;

    char binaryDataSeparator;
    is >> binaryDataSeparator; // not used
        
    char* buffer = new char[res.nBytesImageData]();
    return std::move(res);
}

const Tile Tile::TileUnavailable = {nullptr, nullptr, Tile::Status::Unavailable };
    
Tile Tile::createPlainTile(const glm::uvec2& size, const glm::uvec4& color) {
    using namespace ghoul::opengl;
        
    // Create pixel data
    int numBytes = size.x * size.y * 4 * 1;
    char* pixels = new char[numBytes];
    size_t numPixels = size.x * size.y;
    size_t i = 0;
    for (size_t p = 0; p < numPixels; p++){
        pixels[i++] = color.r;
        pixels[i++] = color.g;
        pixels[i++] = color.b;
        pixels[i++] = color.a;
    }

    // Create ghoul texture
    auto texture = std::make_shared<Texture>(glm::uvec3(size, 1));
    texture->setDataOwnership(Texture::TakeOwnership::Yes);
    texture->setPixelData(pixels);
    texture->uploadTexture();
    texture->setFilter(ghoul::opengl::Texture::FilterMode::Linear);

    // Create tile
    Tile tile;
    tile.status = Tile::Status::OK;
    tile.metaData = nullptr;
    tile.texture = texture;

    return tile;
}

glm::vec2 Tile::compensateSourceTextureSampling(glm::vec2 startOffset, glm::vec2 sizeDiff,
                                                glm::uvec2 resolution, glm::vec2 tileUV)
{
    glm::vec2 sourceSize = glm::vec2(resolution) + sizeDiff;
    glm::vec2 currentSize = glm::vec2(resolution);
    glm::vec2 sourceToCurrentSize = currentSize / sourceSize;
    tileUV = sourceToCurrentSize * (tileUV - startOffset / sourceSize);
    return tileUV;
}

glm::vec2 Tile::TileUvToTextureSamplePosition(const TileUvTransform uvTransform,
                                              glm::vec2 tileUV, glm::uvec2 resolution)
{
    glm::vec2 uv = uvTransform.uvOffset + uvTransform.uvScale * tileUV;
    uv = compensateSourceTextureSampling(
        TileDataset::tilePixelStartOffset,
        TileDataset::tilePixelSizeDifference,
        resolution,
        uv);
    return uv;
}

} // namespace globebrowsing
} // namespace openspace
