/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2015                                                                    *
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

#include <modules/multiresvolume/rendering/atlasmanager.h>

#include <ghoul/filesystem/filesystem.h>
#include <ghoul/logging/logmanager.h>
#include <ghoul/opengl/texture.h>

#include <iostream>
#include <fstream>
#include <cassert>
#include <cstring>
#include <cmath>

namespace {
    const std::string _loggerCat = "AtlasManager";
}

namespace openspace {

AtlasManager::AtlasManager(TSP* tsp, unsigned int nBricks) : _tsp(tsp), _nBricksInAtlas(nBricks) {}

AtlasManager::~AtlasManager() {}

bool AtlasManager::initialize() {
    TSP::Header header = _tsp->header();

    _nBricksPerDim = header.xNumBricks_;
    _nOtLeaves = _nBricksPerDim * _nBricksPerDim * _nBricksPerDim;
    _nOtNodes = _tsp->numOTNodes();
    _nOtLevels = log(_nOtLeaves)/log(8) + 1;
    _paddedBrickDim = _tsp->paddedBrickDim();
    _nBricksInMap = _nOtLeaves;

    _nBrickVals = _paddedBrickDim*_paddedBrickDim*_paddedBrickDim;
    _brickSize = _nBrickVals * sizeof(float);
    _volumeSize = _brickSize * _nOtLeaves;
    _atlasMap = std::vector<unsigned int>(_nOtLeaves, NOT_USED);
    _nBricksPerAtlasDim = std::ceil(std::pow(_nBricksInAtlas, 1.0/3.0));
    _atlasDim = _nBricksPerAtlasDim * _paddedBrickDim;

    _freeAtlasCoords = std::vector<unsigned int>(_nBricksInAtlas, 0);

    for (unsigned int i = 0; i < _nBricksInAtlas; i++) {
        _freeAtlasCoords[i] = i;
    }

    _textureAtlas = new ghoul::opengl::Texture(
        glm::size3_t(_atlasDim, _atlasDim, _atlasDim), 
        ghoul::opengl::Texture::Format::RGBA, 
        GL_RGBA, 
        GL_FLOAT);
    _textureAtlas->uploadTexture();

    glGenBuffers(2, _pboHandle);

    glGenBuffers(1, &_atlasMapBuffer);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _atlasMapBuffer);
    glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(GLint)*_nBricksInMap, NULL, GL_DYNAMIC_READ);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return true;
}

std::vector<unsigned int> AtlasManager::atlasMap() {
    return _atlasMap;
}

unsigned int AtlasManager::atlasMapBuffer() {
    return _atlasMapBuffer;
}

void AtlasManager::updateAtlas(BUFFER_INDEX bufferIndex, std::vector<int>& brickIndices) {
    int nBrickIndices = brickIndices.size();

    _requiredBricks.clear();
    for (int i = 0; i < nBrickIndices; i++) {
        _requiredBricks.insert(brickIndices[i]);
    }

    for (unsigned int it : _prevRequiredBricks) {
        if (!_requiredBricks.count(it)) {
            removeFromAtlas(it);
        }
    }

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _pboHandle[bufferIndex]);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, _volumeSize, 0, GL_STREAM_DRAW);
    float* mappedBuffer = reinterpret_cast<float*>(glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));

    if (!mappedBuffer) {
        LERROR("Failed to map PBO");
        std::cout << glGetError() << std::endl;
        return;
    }

    for (auto itStart = _requiredBricks.begin(); itStart != _requiredBricks.end();) {
        int firstBrick = *itStart;
        int lastBrick = firstBrick;

        auto itEnd = itStart;
        for (itEnd++; itEnd != _requiredBricks.end() && *itEnd == lastBrick + 1; itEnd++) {
            lastBrick = *itEnd;
        }

        addToAtlas(firstBrick, lastBrick, mappedBuffer);

        itStart = itEnd;
    }

    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    for (int i = 0; i < nBrickIndices; i++) {
        _atlasMap[i] = _brickMap[brickIndices[i]];
    }

    std::swap(_prevRequiredBricks, _requiredBricks);

    pboToAtlas(bufferIndex);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, _atlasMapBuffer);
    GLint *to = (GLint*)glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
    memcpy(to, _atlasMap.data(), sizeof(GLint)*_atlasMap.size());
    glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

}

void AtlasManager::addToAtlas(int firstBrickIndex, int lastBrickIndex, float* mappedBuffer) {
    while (_brickMap.count(firstBrickIndex) && firstBrickIndex <= lastBrickIndex) firstBrickIndex++;
    while (_brickMap.count(lastBrickIndex) && lastBrickIndex >= firstBrickIndex) lastBrickIndex--;
    if (lastBrickIndex < firstBrickIndex) return;
    
    int sequenceLength = lastBrickIndex - firstBrickIndex + 1;
    float* sequenceBuffer = new float[sequenceLength*_nBrickVals];
    size_t bufferSize = sequenceLength * _brickSize;

    long offset = TSP::dataPosition() + static_cast<long>(firstBrickIndex) * static_cast<long>(_brickSize);
    _tsp->file().seekg(offset);
    _tsp->file().read(reinterpret_cast<char*>(sequenceBuffer), bufferSize);

    for (int brickIndex = firstBrickIndex; brickIndex <= lastBrickIndex; brickIndex++) {
        if (!_brickMap.count(brickIndex)) {
            unsigned int atlasCoords = _freeAtlasCoords.back();
            _freeAtlasCoords.pop_back();
            int level = _nOtLevels - floor(log((7.0 * (float(brickIndex % _nOtNodes)) + 1.0))/log(8)) - 1;
            assert(atlasCoords <= 0x0FFFFFFF);
            unsigned int atlasData = (level << 28) + atlasCoords;
            _brickMap.insert(std::pair<unsigned int, unsigned int>(brickIndex, atlasData));

            fillVolume(&sequenceBuffer[_nBrickVals*(brickIndex - firstBrickIndex)], mappedBuffer, atlasCoords);
        }
    }

    delete[] sequenceBuffer;
}

void AtlasManager::removeFromAtlas(int brickIndex) {
    unsigned int atlasData = _brickMap[brickIndex];
    unsigned int atlasCoords = atlasData & 0x0FFFFFFF;
    _brickMap.erase(brickIndex);
    _freeAtlasCoords.push_back(atlasCoords);
}

void AtlasManager::fillVolume(float* in, float* out, unsigned int linearAtlasCoords) {
    int x = linearAtlasCoords % _nBricksPerAtlasDim;
    int y = (linearAtlasCoords / _nBricksPerAtlasDim) % _nBricksPerAtlasDim;
    int z = linearAtlasCoords / _nBricksPerAtlasDim / _nBricksPerAtlasDim;


    unsigned int xMin = x*_paddedBrickDim;
    unsigned int yMin = y*_paddedBrickDim;
    unsigned int zMin = z*_paddedBrickDim;
    unsigned int xMax = xMin + _paddedBrickDim;
    unsigned int yMax = yMin + _paddedBrickDim;
    unsigned int zMax = zMin + _paddedBrickDim;

    unsigned int from = 0;
    for (unsigned int zValCoord = zMin; zValCoord<zMax; ++zValCoord) {
        for (unsigned int yValCoord = yMin; yValCoord<yMax; ++yValCoord) {
            for (unsigned int xValCoord = xMin; xValCoord<xMax; ++xValCoord) {

                unsigned int idx =
                    xValCoord +
                    yValCoord*_atlasDim +
                    zValCoord*_atlasDim*_atlasDim;

                out[idx] = in[from];
                from++;
            }
        }
    }
}

void AtlasManager::pboToAtlas(BUFFER_INDEX bufferIndex) {
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, _pboHandle[bufferIndex]);
    glm::size3_t dim = _textureAtlas->dimensions();
    glBindTexture(GL_TEXTURE_3D, *_textureAtlas);
    glTexSubImage3D(GL_TEXTURE_3D,  // target
        0,                          // level
        0,                          // xoffset
        0,                          // yoffset
        0,                          // zoffset
        dim[0],                     // width
        dim[1],                     // height
        dim[2],                     // depth
        GL_RED,                     // format
        GL_FLOAT,                   // type
        NULL);                      // *pixels
    glBindTexture(GL_TEXTURE_3D, 0);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
}

ghoul::opengl::Texture* AtlasManager::textureAtlas() {
    return _textureAtlas;
}

}
