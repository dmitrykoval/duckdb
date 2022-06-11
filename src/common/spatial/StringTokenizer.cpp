/**********************************************************************
 *
 * GEOS - Geometry Engine Open Source
 * http://geos.osgeo.org
 *
 * Copyright (C) 2005-2006 Refractions Research Inc.
 * Copyright (C) 2001-2002 Vivid Solutions Inc.
 *
 * This is free software; you can redistribute and/or modify it under
 * the terms of the GNU Lesser General Public Licence as published
 * by the Free Software Foundation.
 * See the COPYING file for more information.
 *
 **********************************************************************
 *
 * Last port: ORIGINAL WORK
 *
 **********************************************************************/

#include "duckdb/common/spatial/StringTokenizer.hpp"

#include <string>
#include <cstdlib>

using std::string;

namespace duckdb {

/*public*/
StringTokenizer::StringTokenizer(const string& txt)
    :
    str(txt),
    stok(""),
    ntok(0.0)
{
    iter = str.begin();
}


bool
StringTokenizer::isNumberNext()
{
	return peekNextToken() == StringTokenizer::TT_NUMBER;
}

double
StringTokenizer::getNextNumber()
{
	int type = nextToken();
	switch (type) {
	case StringTokenizer::TT_EOF:
		throw std::invalid_argument("Expected number but encountered end of stream");
	case StringTokenizer::TT_EOL:
		throw std::invalid_argument("Expected number but encountered end of line");
	case StringTokenizer::TT_NUMBER:
		return getNVal();
	case StringTokenizer::TT_WORD:
		throw std::invalid_argument("Expected number but encountered word " + getSVal());
	case '(':
		throw std::invalid_argument("Expected number but encountered '('");
	case ')':
		throw std::invalid_argument("Expected number but encountered ')'");
	case ',':
		throw std::invalid_argument("Expected number but encountered ','");
	default:
		throw std::invalid_argument("Unexpected token.");
	}
}

std::string
StringTokenizer::getNextEmptyOrOpener()
{
	std::string nextWord = getNextWord();

	// Z, M, ZM coordinates are not supported.
	if (nextWord == "Z" || nextWord == "M" || nextWord == "ZM") {
		throw std::invalid_argument("Only two-dimensional coordinates are supported. Encountered unsupported '" +
		                            nextWord + "' coordinates marker.");
	}

	if (nextWord == "EMPTY" || nextWord == "(") {
		return nextWord;
	}
	throw std::invalid_argument("Expected 'EMPTY' or '(' but encountered " + nextWord);
}

std::string
StringTokenizer::getNextCloserOrComma()
{
	std::string nextWord = getNextWord();
	if (nextWord == "," || nextWord == ")") {
		return nextWord;
	}
	throw std::invalid_argument("Expected ')' or ',' but encountered " + nextWord);
}

std::string
StringTokenizer::getNextCloser()
{
	std::string nextWord = getNextWord();
	if (nextWord == ")") {
		return nextWord;
	}
	throw std::invalid_argument("Expected ')' but encountered" + nextWord);
}

std::string
StringTokenizer::getNextWord()
{
	int type = nextToken();
	switch (type) {
	case StringTokenizer::TT_EOF:
		throw std::invalid_argument("Expected word but encountered end of stream");
	case StringTokenizer::TT_EOL:
		throw std::invalid_argument("Expected word but encountered end of line");
	case StringTokenizer::TT_NUMBER:
		throw std::invalid_argument("Expected word but encountered number " + std::to_string(getNVal()));
	case StringTokenizer::TT_WORD: {
		std::string word = getSVal();
		for (char &c : word) {
			// Avoid UB if c is not representable as unsigned char
			// https://en.cppreference.com/w/cpp/string/byte/toupper
			c = static_cast<char>(toupper(static_cast<unsigned char>(c)));
		}
		return word;
	}
	case '(':
		return "(";
	case ')':
		return ")";
	case ',':
		return ",";
	default:
		throw std::invalid_argument("Encountered unexpected StreamTokenizer type.");
	}
}

double
strtod_with_vc_fix(const char* str, char** str_end)
{
    double dbl = strtod(str, str_end);
#if _MSC_VER && !__INTEL_COMPILER
    // Special handling of NAN and INF in MSVC, where strtod returns 0.0
    // for NAN and INF.
    // This fixes failing test GEOSisValidDetail::test<3>, maybe others
    // as well.
    // Note: this hack is not robust, Boost lexical_cast or
    // std::stod (C++11) would be better.
    if(*str_end[0] != '\0') {
        char sign = 0;
        const char* pos = str;
        if(*pos == '+' || *pos == '-') {
            sign = *pos++;
        }

        if(stricmp(pos, "inf") == 0) {
            if(!sign || sign == '+') {
                dbl = DoubleInfinity;
            }
            else {
                dbl = DoubleNegInfinity;
            }
            *str_end[0] = '\0';
        }
        else if(stricmp(pos, "nan") == 0) {
            dbl = DoubleNotANumber;
            *str_end[0] = '\0';
        }
    }
#endif
    return dbl;
}

/*public*/
int
StringTokenizer::nextToken()
{
    string tok = "";
    if(iter == str.end()) {
        return StringTokenizer::TT_EOF;
    }
    switch(*iter) {
    case '(':
    case ')':
    case ',':
        return *iter++;
    case '\n':
    case '\r':
    case '\t':
    case ' ':
        string::size_type pos = str.find_first_not_of(" \n\r\t",
                                static_cast<string::size_type>(iter - str.begin()));
        if(pos == string::npos) {
            return StringTokenizer::TT_EOF;
        }
        else {
            iter = str.begin() + static_cast<string::difference_type>(pos);
            return nextToken();
        }
    }
    string::size_type pos = str.find_first_of("\n\r\t() ,",
                            static_cast<string::size_type>(iter - str.begin()));
    if(pos == string::npos) {
        if(iter != str.end()) {
            tok.assign(iter, str.end());
            iter = str.end();
        }
        else {
            return StringTokenizer::TT_EOF;
        }
    }
    else {
        tok.assign(iter, str.begin() + static_cast<string::difference_type>(pos));
        iter = str.begin() + static_cast<string::difference_type>(pos);
    }
    char* stopstring;
    double dbl = strtod_with_vc_fix(tok.c_str(), &stopstring);
    if(*stopstring == '\0') {
        ntok = dbl;
        stok = "";
        return StringTokenizer::TT_NUMBER;
    }
    else {
        ntok = 0.0;
        stok = tok;
        return StringTokenizer::TT_WORD;
    }
}

/*public*/
int
StringTokenizer::peekNextToken()
{

    string::size_type pos;
    string tok = "";
    if(iter == str.end()) {
        return StringTokenizer::TT_EOF;
    }

    pos = str.find_first_not_of(" \r\n\t", static_cast<string::size_type>(iter - str.begin()));

    if(pos == string::npos) {
        return StringTokenizer::TT_EOF;
    }
    switch(str[pos]) {
    case '(':
    case ')':
    case ',':
        return str[pos];
    }

    // It's either a Number or a Word, let's
    // see when it ends

    pos = str.find_first_of("\n\r\t() ,", static_cast<string::size_type>(iter - str.begin()));

    if(pos == string::npos) {
        if(iter != str.end()) {
            tok.assign(iter, str.end());
        }
        else {
            return StringTokenizer::TT_EOF;
        }
    }
    else {
        tok.assign(iter, str.begin() + static_cast<string::difference_type>(pos)); //str.end());
    }

    char* stopstring;
    double dbl = strtod_with_vc_fix(tok.c_str(), &stopstring);
    if(*stopstring == '\0') {
        ntok = dbl;
        stok = "";
        return StringTokenizer::TT_NUMBER;
    }
    else {
        ntok = 0.0;
        stok = tok;
        return StringTokenizer::TT_WORD;
    }
}

double
StringTokenizer::getNVal() const
{
    return ntok;
}

string
StringTokenizer::getSVal() const
{
    return stok;
}

} // namespace duckdb
