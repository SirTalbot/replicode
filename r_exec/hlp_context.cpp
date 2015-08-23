//	hlp_context.cpp
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

#include "hlp_context.h"

#include <r_code/object.h>       // for Code
#include <r_code/vector.h>       // for vector
#include <r_exec/binding_map.h>  // for HLPBindingMap
#include <r_exec/hlp_context.h>  // for HLPContext
#include <r_exec/operator.h>     // for Operator, Context
#include <r_exec/overlay.h>      // for Overlay

#include <replicode_common.h>    // for P


namespace r_exec
{

HLPContext::HLPContext(): _Context(nullptr, 0, nullptr, UNDEFINED)
{
}

HLPContext::HLPContext(Atom *code, uint16_t index, HLPOverlay *const overlay, Data data): _Context(code, index, overlay, data)
{
}

bool HLPContext::operator ==(const HLPContext &c) const
{
    HLPContext lhs = **this;
    HLPContext rhs = *c;

    if (lhs[0] != rhs[0]) { // both contexts point to an atom which is not a pointer.
        return false;
    }

    if (lhs[0].isStructural()) { // both are structural.
        uint16_t atom_count = lhs.getChildrenCount();

        for (uint16_t i = 1; i <= atom_count; ++i)
            if (*lhs.getChild(i) != *rhs.getChild(i)) {
                return false;
            }

        return true;
    }

    return true;
}

bool HLPContext::operator !=(const HLPContext &c) const
{
    return !(*this == c);
}

HLPContext HLPContext::operator *() const
{
    switch ((*this)[0].getDescriptor()) {
    case Atom::I_PTR:
        return *HLPContext(code, (*this)[0].asIndex(), (HLPOverlay *)overlay, data);

    case Atom::VL_PTR: {
        Atom *value_code = ((HLPOverlay *)overlay)->get_value_code((*this)[0].asIndex());

        if (value_code) {
            return *HLPContext(value_code, 0, (HLPOverlay *)overlay, BINDING_MAP);
        } else { // unbound variable.
            return HLPContext();    // data=undefined: evaluation will return false.
        }
    }

    case Atom::VALUE_PTR:
        return *HLPContext(&overlay->values[0], (*this)[0].asIndex(), (HLPOverlay *)overlay, VALUE_ARRAY);

    default:
        return *this;
    }
}

bool HLPContext::evaluate(uint16_t &result_index) const
{
    if (data == BINDING_MAP || data == VALUE_ARRAY) {
        return true;
    }

    HLPContext c = **this;
    return c.evaluate_no_dereference(result_index);
}

bool HLPContext::evaluate_no_dereference(uint16_t &result_index) const
{
    switch (data) {
    case VALUE_ARRAY:
    case BINDING_MAP:
        return true;

    case UNDEFINED:
        return false;

    default:
        break;
    }

    switch (code[index].getDescriptor()) {
    case Atom::ASSIGN_PTR: {
        HLPContext c(code, code[index].asIndex(), (HLPOverlay *)overlay);

        if (c.evaluate_no_dereference(result_index)) {
            ((HLPOverlay *)overlay)->bindings->bind_variable(code, code[index].asAssignmentIndex(), code[index].asIndex(), &overlay->values[0]);
            return true;
        } else {
            return false;
        }
    }

    case Atom::OPERATOR: {
        uint16_t atom_count = getChildrenCount();

        for (uint16_t i = 1; i <= atom_count; ++i) {
            uint16_t unused_result_index;

            if (!(*getChild(i)).evaluate_no_dereference(unused_result_index)) {
                return false;
            }
        }

        Operator op = Operator::Get((*this)[0].asOpcode());
        HLPContext *c = new HLPContext(*this);
        Context _c(c);
        return op(_c, result_index);
    }

    case Atom::OBJECT:
    case Atom::MARKER:
    case Atom::INSTANTIATED_PROGRAM:
    case Atom::INSTANTIATED_CPP_PROGRAM:
    case Atom::INSTANTIATED_INPUT_LESS_PROGRAM:
    case Atom::INSTANTIATED_ANTI_PROGRAM:
    case Atom::COMPOSITE_STATE:
    case Atom::MODEL:
    case Atom::GROUP:
    case Atom::SET:
    case Atom::S_SET: {
        uint16_t atom_count = getChildrenCount();

        for (uint16_t i = 1; i <= atom_count; ++i) {
            uint16_t unused_result_index;

            if (!(*getChild(i)).evaluate_no_dereference(unused_result_index)) {
                return false;
            }
        }

        result_index = index;
        return true;
    }

    default:
        result_index = index;
        return true;
    }
}

uint16_t HLPContext::get_object_code_size() const
{
    switch (data) {
    case STEM:
        return ((HLPOverlay *)overlay)->get_unpacked_object()->code_size();

    case BINDING_MAP:
        return ((HLPOverlay *)overlay)->get_value_code_size((*this)[0].asIndex());

    case VALUE_ARRAY:
        return overlay->values.size();

    default:
        return 0;
    }
}
}
