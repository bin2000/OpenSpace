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

#ifndef __DATAPLANE_H__
#define __DATAPLANE_H__

#include <modules/iswa/rendering/cygnetplane.h>
#include <modules/kameleon/include/kameleonwrapper.h>

 namespace openspace{
 
 class DataPlane : public CygnetPlane {
 public:
 	DataPlane(std::shared_ptr<KameleonWrapper> kw, std::string path);
 	~DataPlane();

 	virtual bool initialize();
    virtual bool deinitialize();

	virtual void render();
	virtual void update();
 
 private:
 	virtual void setParent() override;
 	virtual void loadTexture() override;
 	virtual void updateTexture() override;

	static int id();

	std::shared_ptr<KameleonWrapper> _kw;
	glm::size3_t _dimensions;
	float* _dataSlice;
 };
 
 } // namespace openspace

#endif //__DATAPLANE_H__