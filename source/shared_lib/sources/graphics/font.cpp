// ==============================================================
//	This file is part of Glest Shared Library (www.glest.org)
//
//	Copyright (C) 2001-2007 Martio Figueroa
//
//	You can redistribute this code and/or modify it under
//	the terms of the GNU General Public License as published
//	by the Free Software Foundation; either version 2 of the
//	License, or (at your option) any later version
// ==============================================================

#include "font.h"
#include <stdexcept>
#include <string.h>
#include "conversion.h"

#ifdef USE_FTGL
#include "font_textFTGL.h"
#include <vector>
#include <algorithm>
using namespace Shared::Graphics::Gl;
#endif

#ifdef USE_FREETYPEGL
#include "font_text_freetypegl.h"
#endif

// If your compiler has a version older than 2.4.1 of fontconfig, you can tell cmake to
// disable trying to use fontconfig via passing this to cmake:
// -DWANT_FONTCONFIG=Off
#ifdef HAVE_FONTCONFIG
#include <fontconfig/fontconfig.h>
#endif

#include "util.h"
#include "platform_common.h"
#include "platform_util.h"

#ifdef	HAVE_FRIBIDI
#include <fribidi.h>
#endif

#include <iterator>
#include "leak_dumper.h"

using namespace std;
using namespace Shared::Util;
using namespace Shared::PlatformCommon;

namespace Shared { namespace Graphics {

// Init statics
int Font::charCount							= 256;
std::string Font::fontTypeName 				= "Times New Roman";
bool Font::fontIsMultibyte 					= false;
bool Font::forceLegacyFonts					= false;
bool Font::fontIsRightToLeft				= false;
bool Font::forceFTGLFonts					= false;
bool Font::fontSupportMixedRightToLeft		= false;

// This value is used to scale the font text rendering
// in 3D render mode
float Font::scaleFontValue					= 0.80f;
// This value is used for centering font text vertically (height)
float Font::scaleFontValueCenterHFactor		= 4.0f;
//float Font::scaleFontValue					= 1.0;
//float Font::scaleFontValueCenterHFactor		= 4.0;

int Font::baseSize							= 3;

int Font::faceResolution					= 72;
string Font::langHeightText					= "yW";
//

void Font::resetToDefaults() {
	Font::charCount					= 256;
	Font::fontTypeName 				= "Times New Roman";
	Font::fontIsMultibyte 			= false;
	//Font::forceLegacyFonts			= false;
	Font::fontIsRightToLeft			= false;
	Font::fontSupportMixedRightToLeft = false;
	// This value is used to scale the font text rendering
	// in 3D render mode
	Font::scaleFontValue					= 0.80f;
	// This value is used for centering font text vertically (height)
	Font::scaleFontValueCenterHFactor		= 4.0f;
	//float Font::scaleFontValue					= 1.0;
	//float Font::scaleFontValueCenterHFactor		= 4.0;

	Font::baseSize							= 3;

	Font::faceResolution					= 72;
	Font::langHeightText					= "yW";

#if defined(WIN32)
	string newEnvValue = "MEGAGLEST_FONT=";
	_putenv(newEnvValue.c_str());
	newEnvValue = "MEGAGLEST_FONT_FAMILY=";
	_putenv(newEnvValue.c_str());
#else
	unsetenv("MEGAGLEST_FONT");
	unsetenv("MEGAGLEST_FONT_FAMILY");
#endif
}

// =====================================================
//	class FontMetrics
// =====================================================

FontMetrics::FontMetrics(Text *textHandler) {
	this->textHandler 	= textHandler;
	this->widths		= new float[Font::charCount];
	this->height		= 0;

	for(int i=0; i < Font::charCount; ++i) {
		widths[i]= 0;
	}
}

FontMetrics::~FontMetrics() {
	delete [] widths;
	widths = NULL;
}

void FontMetrics::setTextHandler(Text *textHandler) {
	this->textHandler = textHandler;
}

Text * FontMetrics::getTextHandler() {
	return this->textHandler;
}

float FontMetrics::getTextWidth(const string &str) {
	string longestLine = "";
	size_t found = str.find("\n");
	if (found == string::npos) {
		longestLine = str;
	}
	else {
		vector<string> lineTokens;
		Tokenize(str,lineTokens,"\n");
		if(lineTokens.empty() == false) {
			for(unsigned int i = 0; i < lineTokens.size(); ++i) {
				string currentStr = lineTokens[i];
				if(currentStr.length() > longestLine.length()) {
					longestLine = currentStr;
				}
			}
		}
    }

	if(textHandler != NULL) {
		//printf("str [%s] textHandler->Advance = %f Font::scaleFontValue = %f\n",str.c_str(),textHandler->Advance(str.c_str()),Font::scaleFontValue);
		return (textHandler->Advance(longestLine.c_str()) * Font::scaleFontValue);
		//return (textHandler->Advance(str.c_str()));
	}
	else {
		float width= 0.f;
		for(unsigned int i=0; i< longestLine.size() && (int)i < Font::charCount; ++i){
			if(longestLine[i] >= Font::charCount) {
				string sError = "str[i] >= Font::charCount, [" + longestLine + "] i = " + uIntToStr(i);
				throw megaglest_runtime_error(sError);
			}
			//Treat 2 byte characters as spaces
			if(longestLine[i] < 0) {
				width+= (widths[97]); // This is the letter a which is a normal wide character and good to use for spacing
			}
			else {
				width+= widths[(int)longestLine[i]];
			}
		}
		return width;
	}
}

float FontMetrics::getHeight(const string &str) const {
	if(textHandler != NULL) {
		//printf("(textHandler->LineHeight(" ") = %f Font::scaleFontValue = %f\n",textHandler->LineHeight(" "),Font::scaleFontValue);
		//return (textHandler->LineHeight(str.c_str()) * Font::scaleFontValue);
		return (textHandler->LineHeight(str.c_str()));
	}
	else {
		return height;
	}
}

string FontMetrics::wordWrapText(string text, int maxWidth) {
	// Strip newlines from source
    replaceAll(text, "\n", " \n ");

	// Get all words (space separated text)
	vector<string> words;
	Tokenize(text,words," ");

	string wrappedText = "";
    float lineWidth = 0.0f;

    for(unsigned int i = 0; i < words.size(); ++i) {
    	string word = words[i];
    	if(word=="\n"){
    		wrappedText += word;
    		lineWidth = 0;
    	}
    	else {
    		float wordWidth = this->getTextWidth(word);
    		if (lineWidth + wordWidth > maxWidth) {
    			wrappedText += "\n";
    			lineWidth = 0;
    		}
    		lineWidth += this->getTextWidth(word+" ");
    		wrappedText += word + " ";
    	}
    }

    return wrappedText;
}

// ===============================================
//	class Font
// ===============================================

Font::Font(FontTextHandlerType type) {
	if(GlobalStaticFlags::getIsNonGraphicalModeEnabled() == true) {
		throw megaglest_runtime_error("Loading graphics in headless server mode not allowed!");
	}

	inited		= false;
	this->type	= fontTypeName;
	width		= 400;
	size 		= 10;
	textHandler = NULL;

#if defined(USE_FTGL) || defined(USE_FREETYPEGL)

	if(Font::forceLegacyFonts == false) {
		try {
#if defined(USE_FREETYPEGL)
#endif
			{
				TextFTGL::faceResolution = Font::faceResolution;
				TextFTGL::langHeightText = Font::langHeightText;

				textHandler = NULL;
				textHandler = new TextFTGL(type);

				TextFTGL::faceResolution = Font::faceResolution;
				TextFTGL::langHeightText = Font::langHeightText;
			}

			metrics.setTextHandler(this->textHandler);
		}
		catch(exception &ex) {
			SystemFlags::OutputDebug(SystemFlags::debugError,"In [%s::%s Line: %d] Error [%s]\n",__FILE__,__FUNCTION__,__LINE__,ex.what());
			textHandler = NULL;
		}
	}

#endif
}

Font::~Font() {
	if(textHandler) {
		delete textHandler;
	}
	textHandler = NULL;
}

string Font::getType() const {
	return this->type;
}

void Font::setType(string typeX11, string typeGeneric, string typeGenericFamily) {
	if(textHandler) {
		try {
			this->type= typeGeneric;
			textHandler->init(typeGeneric,typeGenericFamily,textHandler->GetFaceSize());
			metrics.setTextHandler(this->textHandler);
		}
		catch(exception &ex) {
			SystemFlags::OutputDebug(SystemFlags::debugError,"In [%s::%s Line: %d] Error [%s]\n",__FILE__,__FUNCTION__,__LINE__,ex.what());
			textHandler = NULL;
		}
	}
	if(textHandler == NULL) {
		this->type= typeX11;
	}
}

void Font::setWidth(int width) {
	this->width= width;
}

int Font::getWidth() const	{
	return width;
}

int Font::getSize() const	{
	if(textHandler) {
		return textHandler->GetFaceSize();
	}
	else {
		return size;
	}
}
void Font::setSize(int size)	{
	if(textHandler) {
		return textHandler->SetFaceSize(size);
	}
	else {
		this->size= size;
	}
}

bool is_non_ASCII(const int &c) {
	return (c < 0) || (c >= 128);
}
bool is_ASCII(const int &c) {
	return !is_non_ASCII(c);
}

void Font::bidi_cvt(string &str_) {

/*
#ifdef	HAVE_FRIBIDI
	printf("BEFORE:   [%s]\n",str_.c_str());

	char		*c_str = const_cast<char *>(str_.c_str());	// fribidi forgot const...
	FriBidiStrIndex	len = (int)str_.length();
	FriBidiChar	*bidi_logical = new FriBidiChar[len * 4];
	FriBidiChar	*bidi_visual = new FriBidiChar[len * 4];
	char		*utf8str = new char[4*len + 1];	//assume worst case here (all 4 Byte characters)
	FriBidiCharType	base_dir = FRIBIDI_TYPE_ON;
	FriBidiStrIndex n;


#ifdef	OLD_FRIBIDI
	n = fribidi_utf8_to_unicode (c_str, len, bidi_logical);
#else
	n = fribidi_charset_to_unicode(FRIBIDI_CHAR_SET_UTF8, c_str, len, bidi_logical);
#endif
	fribidi_boolean log2vis = fribidi_log2vis(bidi_logical, n, &base_dir, bidi_visual, NULL, NULL, NULL);
	// If convertion was successful
	//if (log2vis == true) {
		// Remove bidi marks (that we don't need) from the output text
		//n = fribidi_remove_bidi_marks (bidi_visual, n, NULL, NULL, NULL);

		// Convert unicode string back to the encoding the input string was in
		//fribidi_unicode_to_charset (char_set_num, visual, len, op);
#ifdef	OLD_FRIBIDI
		fribidi_unicode_to_utf8 (bidi_visual, n, utf8str);
#else
		fribidi_unicode_to_charset(FRIBIDI_CHAR_SET_UTF8, bidi_visual, n, utf8str);
#endif

		// Insert the output string into the output QString
		str_ = std::string(utf8str);
	//}
	//is_rtl_ = base_dir == FRIBIDI_TYPE_RTL;
	//fontIsRightToLeft = base_dir == FRIBIDI_TYPE_RTL;
	fontIsRightToLeft = false;

	delete[] bidi_logical;
	delete[] bidi_visual;
	delete[] utf8str;

	printf("NEW:      [%s]\n",str_.c_str());

#endif
*/


#ifdef	HAVE_FRIBIDI

	//printf("BEFORE:   [%s]\n",str_.c_str());

	string new_value = "";
	bool hasSoftNewLines = false;
	bool hasHardNewLines = false;
	vector<string> lines;

	if(str_.find("\\n") != str_.npos) {
		Tokenize(str_,lines,"\\n");
		hasSoftNewLines = true;
	}
	else if(str_.find("\n") != str_.npos) {
		Tokenize(str_,lines,"\n");
		hasHardNewLines = true;
	}
	else {
		lines.push_back(str_);
	}

	//printf("Lines: %d\n",(int)lines.size());

	for(int lineIndex = 0; lineIndex < (int)lines.size(); ++lineIndex) {
		if(lineIndex > 0) {
			if(hasSoftNewLines == true) {
				new_value += "\\n";
			}
			else if(hasHardNewLines == true) {
				new_value += "\n";
			}
		}
		str_ = lines[lineIndex];
		//printf("Line: %d [%s]\n",lineIndex,str_.c_str());

		vector<string> words;
		if(Font::fontSupportMixedRightToLeft == true) {
			if(str_.find(" ") != str_.npos) {
				Tokenize(str_,words," ");

			}
			else {
				words.push_back(str_);
			}
		}
		else {
			words.push_back(str_);
		}

		vector<string> wordList;
		wordList.reserve(words.size());
		vector<string> nonASCIIWordList;
		nonASCIIWordList.reserve(words.size());

		for(int wordIndex = 0; wordIndex < (int)words.size(); ++wordIndex) {
			//if(wordIndex > 0) {
			//	new_value += " ";
			//}
			str_ = words[wordIndex];

			//printf("Word: %d [%s]\n",wordIndex,str_.c_str());

			//FriBidi C string holding the original text (that is probably with logical hebrew)
			FriBidiChar *logical = NULL;
			//FriBidi C string for the output text (that should be visual hebrew)
			FriBidiChar *visual = NULL;

			FriBidiStrIndex *ltov = NULL;
			FriBidiStrIndex *vtol = NULL;

			//C string holding the originall text (not nnecessarily as unicode)
			char *ip = NULL;
			//C string for the output text (not necessarily as unicode)
			char *op = NULL;

			//Size to allocate for the char arrays
			int size = (int)str_.size() + 2;

			//Allocate memory:
			//It's probably way too much, but at least it's not too little
			logical = new FriBidiChar[size * 3];
			visual = new FriBidiChar[size * 3];
			ip = new char[size * 3];
			op = new char[size * 3];

			ltov = new FriBidiStrIndex[size * 3];
			vtol = new FriBidiStrIndex[size * 3];

			FriBidiCharType base;
			size_t len;

			//A bool type to see if conversion succeded
			fribidi_boolean log2vis;

			//Holds information telling fribidi to use UTF-8
			FriBidiCharSet char_set_num;
			char_set_num = fribidi_parse_charset ("UTF-8");

			//Copy the given to string into the ip string
			strcpy(ip, str_.c_str());

			//Find length of originall text
			len = strlen( ip );

			//Insert ip to logical as unicode (and find it's size now)
			len = fribidi_charset_to_unicode (char_set_num, ip, (FriBidiStrIndex)len, logical);

			base = FRIBIDI_TYPE_ON;

			//printf("STRIPPED: [%s]\n",str_.c_str());

			//Convert logical text to visual
			log2vis = fribidi_log2vis (logical, (FriBidiStrIndex)len, &base, visual, ltov, vtol, NULL);

			bool is_converted = false;
			//If convertion was successful
			if(log2vis)
			{
				//Remove bidi marks (that we don't need) from the output text
				len = fribidi_remove_bidi_marks (visual, (FriBidiStrIndex)len, ltov, vtol, NULL);

				//Convert unicode string back to the encoding the input string was in
				fribidi_unicode_to_charset ( char_set_num, visual, (FriBidiStrIndex)len ,op);

				if(string(op) != str_) {
					is_converted = true;
				}
				//Insert the output string into the result
				str_ = op;

				//printf("LOG2VIS:  [%s]\n",str_.c_str());

	//			if(ascii_char_map.empty() == false) {
	//				for (int index = 0; index < (int)ascii_char_map.size(); ++index) {
	//					str_.insert(ascii_char_map[index].second,1,ascii_char_map[index].first);
	//				}
	//			}
				//printf("AFTER:    [%s]\n",str_.c_str());
			}
			//printf("AFTER:    [%s]\n",str_.c_str());

			//Free allocated memory
			delete [] ltov;
			delete [] vtol;
			delete [] visual;
			delete [] logical;
			delete [] ip;
			delete [] op;

			if(Font::fontSupportMixedRightToLeft == true) {
				if(is_converted == true) {
					nonASCIIWordList.push_back(str_);

					if(wordIndex+1 == (int)words.size()) {
						if(nonASCIIWordList.size() > 1) {
							std::reverse(nonASCIIWordList.begin(),nonASCIIWordList.end());
							copy(nonASCIIWordList.begin(), nonASCIIWordList.end(), std::inserter(wordList, wordList.begin()));
						}
						else {
							if(wordList.empty() == false) {
								copy(nonASCIIWordList.begin(), nonASCIIWordList.end(), std::inserter(wordList, wordList.begin()+wordList.size()));
							}
							else {
								wordList = nonASCIIWordList;
							}
						}
					}
				}
				else {
					if(nonASCIIWordList.size() > 1) {
						std::reverse(nonASCIIWordList.begin(),nonASCIIWordList.end());
					}

					copy(nonASCIIWordList.begin(), nonASCIIWordList.end(), std::inserter(wordList, wordList.begin()));
					nonASCIIWordList.clear();
					wordList.push_back(str_);
				}
			}
			else {
				wordList.push_back(str_);
			}
		}

		//printf("Building New Line: %d [%s]\n",lineIndex,new_value.c_str());
		for(int wordIndex = 0; wordIndex < (int)wordList.size(); ++wordIndex) {
			//printf("wordIndex: %d [%s]\n",wordIndex,wordList[wordIndex].c_str());
			if(wordIndex > 0) {
				new_value += " ";
			}
			new_value += wordList[wordIndex];
		}
		//printf("New Line: %d [%s]\n",lineIndex,new_value.c_str());
	}
	str_ = new_value;
	//printf("NEW:      [%s]\n",str_.c_str());

#endif

}

// ===============================================
//	class Font2D
// ===============================================

Font2D::Font2D(FontTextHandlerType type) : Font(type) {
}

// ===============================================
//	class Font3D
// ===============================================

Font3D::Font3D(FontTextHandlerType type) : Font(type) {
	depth= 10.f;
}

string findFontFamily(const char* font, const char *fontFamily) {
	string resultFile = "";

	// If your compiler has a version older than 2.4.1 of fontconfig, you can tell cmake to
	// disable trying to use fontconfig via passing this to cmake:
	// -DWANT_FONTCONFIG=Off

#ifdef HAVE_FONTCONFIG
	// Get default font via fontconfig
	if( !font && FcInit() && fontFamily)	{
		FcResult result;
		FcFontSet *fs;
		FcPattern* pat;
		FcPattern *match;

		/*
		TRANSLATORS: If using the FTGL backend, this should be the font
		name of a font that contains all the Unicode characters in use in
		your translation.
		*/
		//pat = FcNameParse((FcChar8 *)"Gothic Uralic");
		pat = FcNameParse((FcChar8 *)fontFamily);
		FcConfigSubstitute(0, pat, FcMatchPattern);

		//FcPatternDel(pat, FC_WEIGHT);
		//FcPatternAddInteger(pat, FC_WEIGHT, FC_WEIGHT_BOLD);

		FcDefaultSubstitute(pat);
		fs = FcFontSetCreate();
		match = FcFontMatch(0, pat, &result);

		if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Trying fontconfig for fontfamily [%s]\n",(fontFamily != NULL ? fontFamily : "null"));

		if (match) FcFontSetAdd(fs, match);
		if (pat) FcPatternDestroy(pat);
		if(fs) {
			FcChar8* file;
			if( FcPatternGetString (fs->fonts[0], FC_FILE, 0, &file) == FcResultMatch ) {
				resultFile = (const char*)file;
			}
			FcFontSetDestroy(fs);
		}
		// If your compiler has a version older than 2.4.1 of fontconfig, you can tell cmake to
		// disable trying to use fontconfig via passing this to cmake:
		// -DWANT_FONTCONFIG=Off
		FcFini();
	}
	else {
		if(SystemFlags::VERBOSE_MODE_ENABLED) printf("******************* FONT CONFIG will not be called font [%s] fontFamily [%s]!\n",(font != NULL ? font : "null"),(fontFamily != NULL ? fontFamily : "null"));
	}
#else
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("******************* NO FONT CONFIG ENABLED!\n");
#endif

	return resultFile;
}

void CHECK_FONT_PATH(const char *filename,const char *fontFamily,const char **font,const char **path) {
	*path = filename;
	if( *font == NULL && *path != NULL && strlen(*path) > 0 && fileExists(*path) == true ) {
		*font = strdup(*path);
		if(SystemFlags::VERBOSE_MODE_ENABLED) printf("#1 candidate font file exists [%s]\n",(*font != NULL ? *font : "null"));
	}
	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("#1 Searching for font file [%s] result [%s]\n",(*path != NULL ? *path : "null"),(*font != NULL ? *font : "null"));
	if( *font == NULL && fontFamily != NULL && strlen(fontFamily) > 0) {
		if(SystemFlags::VERBOSE_MODE_ENABLED) printf("#2 Searching for font [%s] family [%s]\n",(*font != NULL ? *font : "null"),(fontFamily != NULL ? fontFamily : "null"));
		string fileFound = findFontFamily(*font, fontFamily);
		if(fileFound != "") {
			*path = fileFound.c_str();
			if(SystemFlags::VERBOSE_MODE_ENABLED) printf("#2 candidate font file found [%s]\n",fileFound.c_str());
			if( *path != NULL && strlen(*path) > 0 && fileExists(*path) == true ) {
				if(*font) free((void*)*font);
				*font = strdup(*path);
				if(SystemFlags::VERBOSE_MODE_ENABLED) printf("#2 candidate font file has been set[%s]\n",(*font != NULL ? *font : "null"));
			}
		}
		if(SystemFlags::VERBOSE_MODE_ENABLED) printf("#2 Searching for font family [%s] result [%s]\n",(fontFamily != NULL ? fontFamily : "null"),(*font != NULL ? *font : "null"));
	}
}

const char* findFont(const char *firstFontToTry,const char *firstFontFamilyToTry) {
	const char* font = NULL;
	const char* path = NULL;


	string tryFont = "";
	if(firstFontToTry != NULL || firstFontFamilyToTry != NULL) {
		if(firstFontToTry != NULL && strlen(firstFontToTry) > 0) {
			tryFont = firstFontToTry;
			#ifdef WIN32
			  replaceAll(tryFont, "/", "\\");
			#endif


			CHECK_FONT_PATH(tryFont.c_str(),firstFontFamilyToTry,&font,&path);
		}
		else {
			CHECK_FONT_PATH(NULL,firstFontFamilyToTry,&font,&path);
		}
	}

	// Get user-specified font path
	if(getenv("MEGAGLEST_FONT") != NULL || getenv("MEGAGLEST_FONT_FAMILY") != NULL) {

		if(getenv("MEGAGLEST_FONT") != NULL) {
			tryFont = getenv("MEGAGLEST_FONT");

			if(Text::DEFAULT_FONT_PATH_ABSOLUTE != "") {
				tryFont = Text::DEFAULT_FONT_PATH_ABSOLUTE + "/" + extractFileFromDirectoryPath(tryFont);
			}
			#ifdef WIN32
			  replaceAll(tryFont, "/", "\\");
			#endif

			CHECK_FONT_PATH(tryFont.c_str(),getenv("MEGAGLEST_FONT_FAMILY"),&font,&path);
		}
		else {
			CHECK_FONT_PATH(NULL,getenv("MEGAGLEST_FONT_FAMILY"),&font,&path);
		}
	}

	string data_path = Text::DEFAULT_FONT_PATH;
	string defaultFont = data_path + "data/core/fonts/LinBiolinum_RB.ttf";//LinBiolinum_Re-0.6.4.ttf
	if(Text::DEFAULT_FONT_PATH_ABSOLUTE != "") {
		data_path = Text::DEFAULT_FONT_PATH;
		defaultFont = data_path + "/LinBiolinum_RB.ttf";//LinBiolinum_Re-0.6.4.ttf
	}
	tryFont = defaultFont;
	#ifdef WIN32
	  replaceAll(tryFont, "/", "\\");
	#endif
	CHECK_FONT_PATH(tryFont.c_str(),"Linux Biolinum O:style=Bold",&font,&path);

	#ifdef FONT_PATH
	// Get distro-specified font path
	CHECK_FONT_PATH(FONT_PATH,NULL,&font,&path);
	#endif

	CHECK_FONT_PATH("/usr/share/fonts/truetype/uralic/gothub__.ttf","Gothic Uralic:style=Regular",&font,&path);

	// Check a couple of common paths for Gothic Uralic/bold as a last resort
	// Debian
	/*
	TRANSLATORS: If using the FTGL backend, this should be the path of a bold
	font that contains all the Unicode characters in use in	your translation.
	If the font is available in Debian it should be the Debian path.
	*/
	CHECK_FONT_PATH("/usr/share/fonts/truetype/uralic/gothub__.ttf","Gothic Uralic:style=Regular",&font,&path);
	/*
	TRANSLATORS: If using the FTGL backend, this should be the path of a
	font that contains all the Unicode characters in use in	your translation.
	If the font is available in Debian it should be the Debian path.
	*/
	CHECK_FONT_PATH("/usr/share/fonts/truetype/uralic/gothu___.ttf","Gothic Uralic:style=Regular",&font,&path);
	// Mandrake
	/*
	TRANSLATORS: If using the FTGL backend, this should be the path of a bold
	font that contains all the Unicode characters in use in	your translation.
	If the font is available in Mandrake it should be the Mandrake path.
	*/
	CHECK_FONT_PATH("/usr/share/fonts/TTF/uralic/GOTHUB__.TTF","Gothic Uralic:style=Bold",&font,&path);
	/*
	TRANSLATORS: If using the FTGL backend, this should be the path of a
	font that contains all the Unicode characters in use in	your translation.
	If the font is available in Mandrake it should be the Mandrake path.
	*/
	CHECK_FONT_PATH("/usr/share/fonts/TTF/uralic/GOTHU___.TTF","Gothic Uralic:style=Regular",&font,&path);

	// Check the non-translated versions of the above
	CHECK_FONT_PATH("/usr/share/fonts/truetype/uralic/gothub__.ttf","Gothic Uralic:style=Regular",&font,&path);
	CHECK_FONT_PATH("/usr/share/fonts/truetype/uralic/gothu___.ttf","Gothic Uralic:style=Regular",&font,&path);
	CHECK_FONT_PATH("/usr/share/fonts/TTF/uralic/GOTHUB__.TTF","Gothic Uralic:style=Regular",&font,&path);
	CHECK_FONT_PATH("/usr/share/fonts/TTF/uralic/GOTHU___.TTF","Gothic Uralic:style=Regular",&font,&path);

	CHECK_FONT_PATH("/usr/share/fonts/truetype/linux-libertine/LinLibertine_Re.ttf","Linux Libertine O:style=Regular",&font,&path);

	CHECK_FONT_PATH("/usr/share/fonts/truetype/freefont/FreeSerif.ttf","FreeSerif",&font,&path);
	CHECK_FONT_PATH("/usr/share/fonts/truetype/freefont/FreeSans.ttf","FreeSans",&font,&path);
	CHECK_FONT_PATH("/usr/share/fonts/truetype/freefont/FreeMono.ttf","FreeMono",&font,&path);

	//openSUSE
	CHECK_FONT_PATH("/usr/share/fonts/truetype/LinBiolinum_RB.otf","Bold",&font,&path);
	CHECK_FONT_PATH("/usr/share/fonts/truetype/FreeSerif.ttf","FreeSerif",&font,&path);
	CHECK_FONT_PATH("/usr/share/fonts/truetype/FreeSans.ttf","FreeSans",&font,&path);
	CHECK_FONT_PATH("/usr/share/fonts/truetype/FreeMono.ttf","FreeMono",&font,&path);

	// gentoo paths
	CHECK_FONT_PATH("/usr/share/fonts/freefont-ttf/FreeSerif.ttf","FreeSerif",&font,&path);
	CHECK_FONT_PATH("/usr/share/fonts/freefont-ttf/FreeSans.ttf","FreeSans",&font,&path);
	CHECK_FONT_PATH("/usr/share/fonts/freefont-ttf/FreeMono.ttf","FreeMono",&font,&path);

#ifdef _WIN32
	CHECK_FONT_PATH("c:\\windows\\fonts\\verdana.ttf",NULL,&font,&path);
	CHECK_FONT_PATH("c:\\windows\\fonts\\tahoma.ttf",NULL,&font,&path);
	CHECK_FONT_PATH("c:\\windows\\fonts\\arial.ttf",NULL,&font,&path);
	CHECK_FONT_PATH("\\windows\\fonts\\arial.ttf",NULL,&font,&path);
#endif

	if(SystemFlags::VERBOSE_MODE_ENABLED) printf("Final selection of font file is [%s]\n",(font != NULL ? font : "null")); \

	return font;
}


}}//end namespace
