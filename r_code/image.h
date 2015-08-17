//	image.h
//
//	Author: Eric Nivel
//
//	BSD license:
//	Copyright (c) 2010, Eric Nivel
//	All rights reserved.
//	Redistribution and use in source and binary forms, with or without
//	modification, are permitted provided that the following conditions are met:
//
//   - Redistributions of source code must retain the above copyright
//     notice, this list of conditions and the following disclaimer.
//   - Redistributions in binary form must reproduce the above copyright
//     notice, this list of conditions and the following disclaimer in the
//     documentation and/or other materials provided with the distribution.
//   - Neither the name of Eric Nivel nor the
//     names of their contributors may be used to endorse or promote products
//     derived from this software without specific prior written permission.
//
//	THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
//	EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
//	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
//	DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
//	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
//	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
//	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
//	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
//	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef r_code_image_h
#define r_code_image_h

#include "CoreLibrary/dll.h"

#include <fstream>


using namespace std;

namespace r_comp {
class Image;
}

namespace r_code {

// An image contains the following:
// - sizes of what follows
// - object map: list of indexes (4 bytes) of objects in the code segment
// - code segment: list of objects
// - object:
// - size of the code (number of atoms)
// - size of the reference set (number of pointers)
// - size of the marker set (number of pointers)
// - size of the view set (number of views)
// - code: indexes in internal pointers are relative to the beginning of the object, indexes in reference pointers are relative to the beginning of the reference set
// - reference set:
// - number of pointers
// - pointers to the relocation segment, i.e. indexes of relocation entries
// - marker set:
// - number of pointers
// - pointers to the relocation segment, i.e. indexes of relocation entries
// - view set: list of views
// - view:
// - size of the code (number of atoms)
// - size of the reference set (number of pointers)
// - list of atoms
// - reference set:
// - pointers to the relocation segment, i.e. indexes of relocation entries
//
// RAM layout of Image::data [sizes in word32]:
//
// data[0]: index of first object in data (i.e. def_size+map_size) [1]
// ... ...
// data[map_size-1]: index of last object in data [1]
// data[map_size]: first word32 of code segment [1]
// ... ...
// data[map_size+code_size-1]: last word32 of code segment [1]
//
// I is the implementation class; prototype:
// class ImageImpl{
// protected:
// uin64 get_timestamp() const;
// uint64_t map_size() const;
// uint64_t code_size() const;
// word32 *data(); // [object map|code segment|reloc segment]
// public:
// void *operator new(size_t,uint64_t data_size);
// ImageImpl(uint64_t map_size,uint64_t code_size);
// ~ImageImpl();
// };

template<class I> class Image:
    public I {
    friend class r_comp::Image;
public:
    static Image<I> *Build(uint64_t timestamp, size_t map_size, size_t code_size, size_t names_size);
// file IO
    static Image<I> *Read(ifstream &stream);
    static void Write(Image<I> *image, ofstream &stream);

    Image();
    ~Image();

    size_t get_size() const; // size of data in word32
    size_t getObjectCount() const;
    uint32_t *getObject(size_t i); // points to the code size of the object; the first atom is at getObject()+2
    uint32_t *getCodeSegment(); // equals getObject(0)
    size_t getCodeSegmentSize() const;

    void trace() const;
};

// utilities

/// Returns the number of word32 needed to encode the string
/// String content is aligned on 4 bytes boundaries; string length heads the structure
size_t dll_export GetStringSize(const std::string &s);

/// Writes the string to \a data, null-terminated
void dll_export WriteString(uint32_t *data, const std::string &string);
/// Reads a null-terminated string from \a data
std::string dll_export ReadString(uint32_t *data);
}


#include "image.tpl.h"


#endif
