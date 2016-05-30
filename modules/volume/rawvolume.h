/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014 - 2016                                                             *
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

#ifndef __RAWVOLUME_H__
#define __RAWVOLUME_H__

namespace openspace {

template <typename Voxel>
class RawVolume {
public:
    typedef Voxel VoxelType;
    RawVolume(const glm::ivec3& dimensions);
    glm::ivec3 dimensions() const;
    void setDimensions(const glm::ivec3& dimensions);
    VoxelType get(const glm::ivec3& coordinates) const;
    VoxelType get(const size_t index) const;
    void set(const glm::ivec3& coordinates, const VoxelType& value);
    void set(size_t index, const VoxelType& value);
    void forEachVoxel(const std::function<void(const glm::ivec3&, const VoxelType&)>& fn);
    VoxelType* data();
private:
    size_t coordsToIndex(const glm::ivec3& cartesian) const;
    glm::ivec3 indexToCoords(size_t linear) const;
    glm::ivec3 _dimensions;
    std::vector<VoxelType> _data;
};

}

#include "rawvolume.inl"

#endif // __RAWVOLUME_H__