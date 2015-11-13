/*****************************************************************************************
 *                                                                                       *
 * OpenSpace                                                                             *
 *                                                                                       *
 * Copyright (c) 2014-2015                                                               *
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

#ifndef __WINDOWWRAPPER_H__
#define __WINDOWWRAPPER_H__

#include <ghoul/glm.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace openspace {

class WindowWrapper {
public:
    virtual void setBarrier(bool enabled) = 0;
    virtual void clearAllWindows() = 0;
    virtual bool windowHasResized() const = 0;
    virtual double time() const = 0;
    virtual double averageDeltaTime() const = 0;
    virtual uint32_t mouseButtons(int maxNumber = 8) const = 0;
    virtual glm::vec2 mousePosition() const = 0;
    virtual glm::ivec2 currentWindowSize() const = 0;
    virtual glm::ivec2 currentWindowResolution() const = 0;
    virtual bool isRegularRendering() const = 0;

    virtual glm::mat4 viewProjectionMatrix() const = 0;
    virtual void setNearFarClippingPlane(float near, float far) = 0;
    
    virtual glm::ivec4 viewportPixelCoordinates() const = 0;
    
    virtual bool isExternalControlConnected() const = 0;
    virtual void sendMessageToExternalControl(const std::vector<char>& message) const = 0;
    
    // true for single viewport, single window; false otherwise
    virtual bool isSimpleRendering() const = 0;
    
    virtual void takeScreenshot() const = 0;
    
    //virtual void forEachWindow(std::function<void (void)> function) = 0;
    
};

} // namespace openspace

#endif // _WINDOW_H__