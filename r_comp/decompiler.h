//	decompiler.h
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

#ifndef decompiler_h
#define decompiler_h

#include <fstream>
#include <sstream>

#include "out_stream.h"
#include "segments.h"


namespace r_comp {

class dll_export Decompiler {
private:
    r_comp::OutStream *out_stream;
    uint16_t indents; // in chars.
    bool closing_set; // set after writing the last element of a set: any element in an expression finding closing_set will indent and set closing_set to false.
    bool in_hlp;
    bool hlp_postfix;
    bool horizontal_set;

    ImageObject *current_object;

    r_comp::Metadata *metadata;
    r_comp::Image *image;

    uint64_t time_offset; // 0 means no offset.

    std::unordered_map<uint16_t, std::string> variable_names; // in the form vxxx where xxx is an integer representing the order of referencing of the variable/label in the code.
    uint16_t last_variable_id;
    std::string get_variable_name(uint16_t index, bool postfix); // associates iptr/vptr indexes to names; inserts them in out_stream if necessary; when postfix==true, a trailing ':' is added.
    std::string get_hlp_variable_name(uint16_t index);

    std::unordered_map<uint16_t, std::string> object_names; // in the form class_namexxx where xxx is an integer representing the order of appearence of the object in the image; or: user-defined names when they are provided.
    std::unordered_map<std::string, uint16_t> object_indices; // inverted version of the object_names.
    std::string get_object_name(uint16_t index); // retrieves the name of an object.

    void write_indent(uint16_t i);
    void write_expression_head(uint16_t read_index); // decodes the leading atom of an expression.
    void write_expression_tail(uint16_t read_index, bool apply_time_offset, bool vertical = false); // decodes the elements of an expression following the head.
    void write_set(uint16_t read_index, bool aply_time_offset, uint16_t write_as_view_index = 0);
    void write_any(uint16_t read_index, bool &after_tail_wildcard, bool apply_time_offset, uint16_t write_as_view_index = 0); // decodes any element in an expression or a set.

    typedef void (Decompiler::*Renderer)(uint16_t);
    r_code::vector<Renderer> renderers; // indexed by opcodes; when not there, write_expression() is used.

// Renderers.
    void write_expression(uint16_t read_index); // default renderer.
    void write_group(uint16_t read_index);
    void write_marker(uint16_t read_index);
    void write_pgm(uint16_t read_index);
    void write_ipgm(uint16_t read_index);
    void write_icmd(uint16_t read_index);
    void write_cmd(uint16_t read_index);
    void write_fact(uint16_t read_index);
    void write_hlp(uint16_t read_index);
    void write_ihlp(uint16_t read_index);
    void write_view(uint16_t read_index, uint16_t arity);

    bool partial_decompilation; // used when decompiling on-the-fly.
    bool ignore_named_objects;
    std::unordered_set<uint16_t> named_objects;
    std::vector<SysObject *> imported_objects; // referenced objects added to the image that were not in the original list of objects to be decompiled.
public:
    Decompiler();
    ~Decompiler();

    void init(r_comp::Metadata *metadata);
    uint64_t decompile(r_comp::Image *image,
                     std::ostringstream *stream,
                     uint64_t time_offset,
                     bool ignore_named_objects); // decompiles the whole image; returns the number of objects.
    uint64_t decompile(r_comp::Image *image,
                     std::ostringstream *stream,
                     uint64_t time_offset,
                     std::vector<SysObject *> &imported_objects); // idem, ignores named objects if in the imported object list.
    uint64_t decompile_references(r_comp::Image *image); // initialize a reference table so that objects can be decompiled individually; returns the number of objects.
    void decompile_object(uint16_t object_index, std::ostringstream *stream, uint64_t time_offset); // decompiles a single object; object_index is the position of the object in the vector returned by Image::getObject.
    void decompile_object(const std::string object_name, std::ostringstream *stream, uint64_t time_offset); // decompiles a single object given its name: use this function to follow references.
};
}


#endif
