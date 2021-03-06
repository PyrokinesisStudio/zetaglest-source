// ==============================================================
//	This file is part of Glest Shared Library (www.glest.org)
//
//	Copyright (C) 2001-2008 Martiño Figueroa
//
//	You can redistribute this code and/or modify it under 
//	the terms of the GNU General Public License as published 
//	by the Free Software Foundation; either version 2 of the 
//	License, or (at your option) any later version
// ==============================================================

#include "font_manager.h"

#include "graphics_interface.h"
#include "graphics_factory.h"
#include <stdexcept>
#include "util.h"
#include "platform_util.h"
#include "leak_dumper.h"

using namespace Shared::Platform;
using namespace Shared::Util;

namespace Shared { namespace Graphics {

// =====================================================
//	class FontManager
// =====================================================
FontManager::FontManager() {
	if(GlobalStaticFlags::getIsNonGraphicalModeEnabled() == true) {
		throw megaglest_runtime_error("Loading graphics in headless server mode not allowed!");
	}
	fonts.clear();
}

FontManager::~FontManager() {
	end();
}

Font2D *FontManager::newFont2D() {
	Font2D *font= GraphicsInterface::getInstance().getFactory()->newFont2D();
	fonts.push_back(font);
	return font;
}

Font3D *FontManager::newFont3D() {
	Font3D *font= GraphicsInterface::getInstance().getFactory()->newFont3D();
	fonts.push_back(font);
	return font;
}

void FontManager::endFont(Font *font,bool mustExistInList) {
	if(font != NULL) {
		bool found = false;
		for(unsigned int i=0; i < fonts.size(); ++i) {
			if(fonts[i] != NULL && font == fonts[i]) {
				found = true;
				fonts.erase(fonts.begin() + i);
				break;
			}
		}
		if(found == false && mustExistInList == true) {
			throw std::runtime_error("found == false in endFont");
		}
		if(found == true) {
			font->end();
			delete font;
		}
	}
}

void FontManager::init() {
	for(size_t i=0; i<fonts.size(); ++i) {
		if(fonts[i] != NULL) {
			fonts[i]->init();
		}
	}
} 

void FontManager::end() {
	for(size_t i=0; i<fonts.size(); ++i) {
		if(fonts[i] != NULL) {
			fonts[i]->end();
			delete fonts[i];
			fonts[i]=NULL;
		}
	}
	fonts.clear();
}

}}//end namespace
