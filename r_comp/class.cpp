//	class.cpp
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

#include "class.h"
#include "segments.h"


namespace r_comp {

const char *Class::Expression = "xpr";
const char *Class::Type = "type";

bool Class::has_offset() const {

    switch (atom.getDescriptor()) {
    case Atom::OBJECT:
    case Atom::GROUP:
    case Atom::INSTANTIATED_PROGRAM:
    case Atom::INSTANTIATED_CPP_PROGRAM:
    case Atom::S_SET:
    case Atom::MARKER: return true;
    default: return false;
    }
}

Class::Class(ReturnType t): str_opcode("undefined"), type(t) {
}

Class::Class(Atom atom,
             std::string str_opcode,
             std::vector<StructureMember> r,
             ReturnType t): atom(atom),
    str_opcode(str_opcode),
    things_to_read(r),
    type(t),
    use_as(StructureMember::I_CLASS) {
}

bool Class::is_pattern(Metadata *metadata) const {

    return (metadata->classes.find("ptn")->second.atom == atom) || (metadata->classes.find("|ptn")->second.atom == atom);
}

bool Class::is_fact(Metadata *metadata) const {

    return (metadata->classes.find("fact")->second.atom == atom) || (metadata->classes.find("|fact")->second.atom == atom);
}

bool Class::get_member_index(Metadata *metadata, std::string &name, uint16_t &index, Class *&p) const {

    for (uint16_t i = 0; i < things_to_read.size(); ++i)
        if (things_to_read[i].name == name) {

            index = (has_offset() ? i + 1 : i); // in expressions the lead r-atom is at 0; in objects, members start at 1
            if (things_to_read[i].used_as_expression()) // the class is: [::a-class]
                p = NULL;
            else
                p = things_to_read[i].get_class(metadata);
            return true;
        }
    return false;
}

std::string Class::get_member_name(uint64_t index) {

    return things_to_read[has_offset() ? index - 1 : index].name;
}

ReturnType Class::get_member_type(const uint16_t index) {

    return things_to_read[has_offset() ? index - 1 : index].get_return_type();
}

Class *Class::get_member_class(Metadata *metadata, const std::string &name) {

    for (uint16_t i = 0; i < things_to_read.size(); ++i)
        if (things_to_read[i].name == name)
            return things_to_read[i].get_class(metadata);
    return NULL;
}

void Class::write(uint32_t *storage)
{
    storage[0] = atom.atom;
    r_code::WriteString(storage + 1, str_opcode);
    uint64_t offset = 1 + r_code::GetStringSize(str_opcode);
    storage[offset++] = type;
    storage[offset++] = use_as;
    storage[offset++] = things_to_read.size();
    for (uint64_t i = 0; i < things_to_read.size(); ++i) {
        things_to_read[i].write(storage + offset);
        offset += things_to_read[i].get_size();
    }
}

void Class::read(uint32_t *storage)
{
    atom = storage[0];
    str_opcode = r_code::ReadString(storage + 1);
    uint64_t offset = 1 + r_code::GetStringSize(str_opcode);
    type = (ReturnType)storage[offset++];
    use_as = (StructureMember::Iteration)storage[offset++];
    uint64_t member_count = storage[offset++];
    for (uint64_t i = 0; i < member_count; ++i) {

        StructureMember m;
        m.read(storage + offset);
        things_to_read.push_back(m);
        offset += m.get_size();
    }
}

size_t Class::get_size() { // see segments.cpp for the RAM layout

    size_t size = 4; // atom, return type, usage, number of members
    size += r_code::GetStringSize(str_opcode);
    for (size_t i = 0; i < things_to_read.size(); ++i)
        size += things_to_read[i].get_size();
    return size;
}
}
