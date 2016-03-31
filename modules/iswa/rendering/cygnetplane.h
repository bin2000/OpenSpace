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

#ifndef __CYGNETPLANE_H__
#define __CYGNETPLANE_H__

#include <modules/iswa/rendering/iswacygnet.h>
#include <ghoul/opengl/texture.h>
#include <openspace/util/powerscaledcoordinate.h>

namespace openspace{
class CygnetPlane : public ISWACygnet {
public:
	CygnetPlane(std::string path);
	~CygnetPlane();

	virtual bool initialize();
	virtual bool deinitialize();

	bool isReady();

	virtual void render();
	virtual void update();

protected:
	virtual void setParent() = 0;
	virtual void loadTexture() = 0;
	virtual void updateTexture() = 0;

	void createPlane();

	GLuint _quad;
	GLuint _vertexPositionBuffer;
	bool _planeIsDirty;
};
} //namespace openspace

#endif //__CYGNETPLANE_H__