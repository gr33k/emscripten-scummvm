/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#include "dreamweb/dreamweb.h"
#include "engines/util.h"
#include "graphics/surface.h"

namespace DreamGen {

void DreamGenContext::printboth() {
	uint16 x = di;
	printboth(es, ds, &x, bx, al);
	di = x;
}

void DreamGenContext::printboth(uint16 dst, uint16 src, uint16 *x, uint16 y, uint8 c) {
	uint16 newX = *x;
	uint8 width, height;
	printchar(dst, src, &newX, y, c, &width, &height);
	multidump(*x, y, width, height);
	*x = newX;
}

uint8 DreamGenContext::getnextword(const uint8 *string, uint8 *totalWidth, uint8 *charCount) {
	*totalWidth = 0;
	*charCount = 0;
	while(true) {
		uint8 firstChar = *string;
		++string;
		++*charCount;
		if ((firstChar == ':') || (firstChar == 0)) { //endall
			*totalWidth += 6;
			return 1;
		}
		if (firstChar == 32) { //endword
			*totalWidth += 6;
			return 0;
		}
		firstChar = engine->modifyChar(firstChar);
		if (firstChar != 255) {
			uint8 secondChar = *string;
			uint8 width = ds.byte(6*(firstChar - 32 + data.word(kCharshift)));
			width = kernchars(firstChar, secondChar, width);
			*totalWidth += width;
		}
	}
}

void DreamGenContext::getnextword() {
	uint8 totalWidth, charCount;
	al = getnextword(es.ptr(di, 0), &totalWidth, &charCount);
	bl = totalWidth;
	bh = charCount;
	di += charCount;
}

void DreamGenContext::printchar() {
	uint16 x = di;
	uint8 width, height;
	printchar(es, ds, &x, bx, al, &width, &height);
	di = x;
	cl = width;
	ch = height;
}

void DreamGenContext::printchar(uint16 dst, uint16 src, uint16* x, uint16 y, uint8 c, uint8 *width, uint8 *height) {
	if (c == 255)
		return;
	push(si);
	push(di);
	if (data.byte(kForeignrelease) != 0)
		y -= 3;
	uint16 tmp = c - 32 + data.word(kCharshift);
	showframe(dst, src, *x, y, tmp & 0xff, tmp >> 8, width, height);
	di = pop();
	si = pop();
	_cmp(data.byte(kKerning), 0);
	if (flags.z())
		kernchars();
	(*x) += *width;
}

void DreamGenContext::printslow() {
	al = printslow(di, bx, dl, (bool)(dl & 1));
}

uint8 DreamGenContext::printslow(uint16 x, uint16 y, uint8 maxWidth, bool centered) {
	data.byte(kPointerframe) = 1;
	data.byte(kPointermode) = 3;
	ds = data.word(kCharset1);
	do {
		uint16 offset = x;
		uint16 charCount = getnumber(si, maxWidth, centered, &offset);
		do {
			push(si);
			push(es);
			uint8 c0 = es.byte(si);
			push(es);
			push(ds);
			c0 = engine->modifyChar(c0);
			printboth(es, ds, &offset, y, c0);
			ds = pop();
			es = pop();
			uint8 c1 = es.byte(si+1);
			++si;
			if ((c1 == 0) || (c1 == ':')) {
				es = pop();
				si = pop();
				return 0;
			}
			if (charCount != 1) {
				push(ds);
				push(es);
				c1 = engine->modifyChar(c1);
				data.word(kCharshift) = 91;
				uint16 offset2 = offset;
				printboth(es, ds, &offset2, y, c1);
				data.word(kCharshift) = 0;
				es = pop();
				ds = pop();
				for (int i=0; i<2; ++i) {
					waitframes();
					if (ax == 0)
						continue;
					if (ax != data.word(kOldbutton)) {
						es = pop();
						si = pop();
						return 1;
					}
				}
			}

			es = pop();
			si = pop();
			++si;
			--charCount;
		} while (charCount);
		y += 10;
	} while (true);
}

void DreamGenContext::printdirect() {
	uint16 y = bx;
	printdirect(di, &y, dl, (bool)(dl & 1));
	bx = y;
}

void DreamGenContext::printdirect(uint16 x, uint16 *y, uint8 maxWidth, bool centered) {
	data.word(kLastxpos) = x;
	ds = data.word(kCurrentset);
	while (true) {
		uint16 offset = x;
		uint8 charCount = getnumber(si, maxWidth, centered, &offset);
		uint16 i = offset;
		do {
			uint8 c = es.byte(si);
			++si;
			if ((c == 0) || (c == ':')) {
				return;
			}
			c = engine->modifyChar(c);
			uint8 width, height;
			push(es);
			printchar(es, ds, &i, *y, c, &width, &height);
			es = pop();
			data.word(kLastxpos) = i;
			--charCount;
		} while(charCount);
		*y += data.word(kLinespacing);
	}
}

void DreamGenContext::getnumber() {
	uint16 offset = di;
	cl = getnumber(si, dl, (bool)(dl & 1), &offset);
	di = offset;
}

uint8 DreamGenContext::getnumber(uint16 index, uint16 maxWidth, bool centered, uint16* offset) {
	uint8 totalWidth = 0;
	uint8 charCount = 0;
	while (true) {
		uint8 wordTotalWidth, wordCharCount;
		uint8 done = getnextword(es.ptr(index, 0), &wordTotalWidth, &wordCharCount);
		index += wordCharCount;

		if (done == 1) { //endoftext
			ax = totalWidth + wordTotalWidth - 10;
			if (ax < maxWidth) {
				totalWidth += wordTotalWidth;
				charCount += wordCharCount;
			}

			if (centered) {
				ax = (maxWidth & 0xfe) + 2 + 20 - totalWidth;
				ax /= 2;
			} else {
				ax = 0;
			}
			*offset += ax;
			return charCount;
		}
		ax = totalWidth + wordTotalWidth - 10;
		if (ax >= maxWidth) { //gotoverend
			if (centered) {
				ax = (maxWidth & 0xfe) - totalWidth + 20;
				ax /= 2;
			} else {
				ax = 0;
			}
			*offset += ax;
			return charCount;
		}
		totalWidth += wordTotalWidth;
		charCount += wordCharCount;
	}
}

uint8 DreamGenContext::kernchars(uint8 firstChar, uint8 secondChar, uint8 width) {
	if ((firstChar == 'a') || (al == 'u')) {
		if ((secondChar == 'n') || (secondChar == 't') || (secondChar == 'r') || (secondChar == 'i') || (secondChar == 'l'))
			return width-1;
	}
	return width;
}

void DreamGenContext::kernchars() {
	cl = kernchars(al, ah, cl);
}

} /*namespace dreamgen */

