//	structure_member.cpp
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

#include "structure_member.h"

#include <r_code/image.h>             // for GetStringSize, ReadString, etc
#include <r_comp/compiler.h>          // for Compiler
#include <r_comp/segments.h>          // for Metadata
#include <r_comp/structure_member.h>  // for StructureMember, ReturnType, etc
#include <unordered_map>              // for _Node_iterator, unordered_map
#include <utility>                    // for move, pair


namespace r_comp
{

StructureMember::StructureMember()
{
}

StructureMember::StructureMember(_Read r,
                                 std::string m,
                                 std::string p,
                                 Iteration i): _read(r),
    _class(std::move(p)),
    iteration(i),
    name(std::move(m))
{
    if (_read == &Compiler::read_any) {
        type = ANY;
    } else if (_read == &Compiler::read_number) {
        type = NUMBER;
    } else if (_read == &Compiler::read_timestamp) {
        type = TIMESTAMP;
    } else if (_read == &Compiler::read_boolean) {
        type = BOOLEAN;
    } else if (_read == &Compiler::read_string) {
        type = STRING;
    } else if (_read == &Compiler::read_node) {
        type = NODE_ID;
    } else if (_read == &Compiler::read_device) {
        type = DEVICE_ID;
    } else if (_read == &Compiler::read_function) {
        type = FUNCTION_ID;
    } else if (_read == &Compiler::read_expression) {
        type = ANY;
    } else if (_read == &Compiler::read_set) {
        type = ReturnType::SET;
    } else if (_read == &Compiler::read_class) {
        type = ReturnType::CLASS;
    }
}

Class *StructureMember::get_class(Metadata *metadata) const
{
    return _class == "" ? nullptr : &metadata->classes.find(_class)->second;
}

ReturnType StructureMember::get_return_type() const
{
    return type;
}

bool StructureMember::used_as_expression() const
{
    return iteration == I_EXPRESSION;
}

StructureMember::Iteration StructureMember::getIteration() const
{
    return iteration;
}

_Read StructureMember::read() const
{
    return _read;
}

void StructureMember::write(uint32_t *storage) const
{
    if (_read == &Compiler::read_any) {
        storage[0] = R_ANY;
    } else if (_read == &Compiler::read_number) {
        storage[0] = R_NUMBER;
    } else if (_read == &Compiler::read_timestamp) {
        storage[0] = R_TIMESTAMP;
    } else if (_read == &Compiler::read_boolean) {
        storage[0] = R_BOOLEAN;
    } else if (_read == &Compiler::read_string) {
        storage[0] = R_STRING;
    } else if (_read == &Compiler::read_node) {
        storage[0] = R_NODE;
    } else if (_read == &Compiler::read_device) {
        storage[0] = R_DEVICE;
    } else if (_read == &Compiler::read_function) {
        storage[0] = R_FUNCTION;
    } else if (_read == &Compiler::read_expression) {
        storage[0] = R_EXPRESSION;
    } else if (_read == &Compiler::read_set) {
        storage[0] = R_SET;
    } else if (_read == &Compiler::read_class) {
        storage[0] = R_CLASS;
    }

    uint64_t offset = 1;
    storage[offset++] = type;
    r_code::WriteString(storage + offset, _class);
    offset += r_code::GetStringSize(_class);
    storage[offset++] = iteration;
    r_code::WriteString(storage + offset, name);
}

void StructureMember::read(uint32_t *storage)
{
    switch (storage[0]) {
    case R_ANY:
        _read = &Compiler::read_any;
        break;

    case R_NUMBER:
        _read = &Compiler::read_number;
        break;

    case R_TIMESTAMP:
        _read = &Compiler::read_timestamp;
        break;

    case R_BOOLEAN:
        _read = &Compiler::read_boolean;
        break;

    case R_STRING:
        _read = &Compiler::read_string;
        break;

    case R_NODE:
        _read = &Compiler::read_node;
        break;

    case R_DEVICE:
        _read = &Compiler::read_device;
        break;

    case R_FUNCTION:
        _read = &Compiler::read_function;
        break;

    case R_EXPRESSION:
        _read = &Compiler::read_expression;
        break;

    case R_SET:
        _read = &Compiler::read_set;
        break;

    case R_CLASS:
        _read = &Compiler::read_class;
        break;
    }

    uint64_t offset = 1;
    type = (ReturnType)storage[offset++];
    _class = r_code::ReadString(storage + offset);
    offset += r_code::GetStringSize(_class);
    iteration = (Iteration)storage[offset++];
    name = r_code::ReadString(storage + offset);
}

size_t StructureMember::get_size()
{
    size_t size = 3; // read ID, return type, iteration
    size += r_code::GetStringSize(_class);
    size += r_code::GetStringSize(name);
    return size;
}
}
