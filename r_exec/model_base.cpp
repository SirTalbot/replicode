//	model_base.cpp
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

#include "model_base.h"
#include "mem.h"


namespace r_exec
{

uint64_t ModelBase::MEntry::_ComputeHashCode(_Fact *component)   // 14 bits: [fact or |fact (1)|type (3)|data (10)].
{
    uint64_t hash_code;

    if (component->is_fact()) {
        hash_code = 0x00000000;
    } else {
        hash_code = 0x00002000;
    }

    Code *payload = component->get_reference(0);
    Atom head = payload->code(0);
    uint16_t opcode = head.asOpcode();

    switch (head.getDescriptor()) {
    case Atom::OBJECT:
        if (opcode == Opcodes::Cmd) { // type: 2.
            hash_code |= 0x00000800;
            hash_code |= (payload->code(CMD_FUNCTION).asOpcode() & 0x000003FF); // data: function id.
        } else if (opcode == Opcodes::IMdl) { // type 3.
            hash_code |= 0x00000C00;
            hash_code |= (((uintptr_t)payload->get_reference(0)) & 0x000003FF); // data: address of the mdl.
        } else if (opcode == Opcodes::ICst) { // type 4.
            hash_code |= 0x00001000;
            hash_code |= (((uintptr_t)payload->get_reference(0)) & 0x000003FF); // data: address of the cst.
        } else { // type: 0.
            hash_code |= (opcode & 0x000003FF);    // data: class id.
        }

        break;

    case Atom::MARKER: // type 1.
        hash_code |= 0x00000400;
        hash_code |= (opcode & 0x000003FF); // data: class id.
        break;
    }

    return hash_code;
}

uint64_t ModelBase::MEntry::ComputeHashCode(Code *mdl, bool packed)
{
    uint64_t hash_code = (mdl->code(mdl->code(HLP_TPL_ARGS).asIndex()).getAtomCount() << 28);
    _Fact *lhs;
    _Fact *rhs;

    if (packed) {
        Code *unpacked_mdl = mdl->get_reference(mdl->references_size() - MDL_HIDDEN_REFS);
        lhs = (_Fact *)unpacked_mdl->get_reference(0);
        rhs = (_Fact *)unpacked_mdl->get_reference(1);
    } else {
        lhs = (_Fact *)mdl->get_reference(0);
        rhs = (_Fact *)mdl->get_reference(1);
    }

    hash_code |= (_ComputeHashCode(lhs) << 14);
    hash_code |= _ComputeHashCode(rhs);
    return hash_code;
}

bool ModelBase::MEntry::Match(Code *lhs, Code *rhs)
{
    if (lhs->code(0).asOpcode() == Opcodes::Ent || rhs->code(0).asOpcode() == Opcodes::Ent) {
        return lhs == rhs;
    }

    if (lhs->code(0).asOpcode() == Opcodes::Ont || rhs->code(0).asOpcode() == Opcodes::Ont) {
        return lhs == rhs;
    }

    if (lhs->code_size() != rhs->code_size()) {
        return false;
    }

    if (lhs->references_size() != rhs->references_size()) {
        return false;
    }

    for (uint16_t i = 0; i < lhs->code_size(); ++i) {
        if (lhs->code(i) != rhs->code(i)) {
            return false;
        }
    }

    for (uint16_t i = 0; i < lhs->references_size(); ++i) {
        if (!Match(lhs->get_reference(i), rhs->get_reference(i))) {
            return false;
        }
    }

    return true;
}

ModelBase::MEntry::MEntry(): mdl(nullptr), touch_time(0), hash_code(0)
{
}

ModelBase::MEntry::MEntry(Code *mdl, bool packed): mdl(mdl), touch_time(Now()), hash_code(ComputeHashCode(mdl, packed))
{
}

bool ModelBase::MEntry::match(const MEntry &e) const   // at this point both models have the same hash code; this.mdl is packed, e.mdl is unpacked.
{
    if (mdl == e.mdl) {
        return true;
    }

    for (uint16_t i = 0; i < mdl->code_size(); ++i) { // first check the mdl code: this checks on tpl args and guards.
        if (i == MDL_STRENGTH || i == MDL_CNT || i == MDL_SR || i == MDL_DSR || i == MDL_ARITY) { // ignore house keeping data.
            continue;
        }

        if (mdl->code(i) != e.mdl->code(i)) {
            return false;
        }
    }

    Code *lhs_0 = mdl->get_reference(mdl->references_size() - 1)->get_reference(0); // payload of the fact.
    Code *lhs_1 = e.mdl->get_reference(0)->get_reference(0); // payload of the fact.

    if (!Match(lhs_0, lhs_1)) {
        return false;
    }

    Code *rhs_0 = mdl->get_reference(mdl->references_size() - 1)->get_reference(1); // payload of the fact.
    Code *rhs_1 = e.mdl->get_reference(1)->get_reference(1); // payload of the fact.

    if (!Match(rhs_0, rhs_1)) {
        return false;
    }

    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

ModelBase *ModelBase::Singleton = nullptr;

ModelBase *ModelBase::Get()
{
    return Singleton;
}

ModelBase::ModelBase()
{
    Singleton = this;
}

void ModelBase::trim_objects()
{
    std::lock_guard<std::mutex> guard(m_mdlMutex);
    uint64_t now = Now();
    MdlSet::iterator m;

    for (m = black_list.begin(); m != black_list.end();) {
        if (now - (*m).touch_time >= thz) {
            m = black_list.erase(m);
        } else {
            ++m;
        }
    }
}

void ModelBase::load(Code *mdl)
{
    MEntry e(mdl, true);

    if (mdl->views.size() > 0) { // no need to lock at load time.
        white_list.insert(e);
    } else {
        black_list.insert(e);
    }
}

void ModelBase::get_models(r_code::list<P<Code> > &models)
{
    std::lock_guard<std::mutex> guard(m_mdlMutex);
    MdlSet::iterator m;

    for (m = white_list.begin(); m != white_list.end(); ++m) {
        models.push_back((*m).mdl);
    }

    for (m = black_list.begin(); m != black_list.end(); ++m) {
        models.push_back((*m).mdl);
    }
}

Code *ModelBase::check_existence(Code *mdl)
{
    MEntry e(mdl, false);
    std::lock_guard<std::mutex> guard(m_mdlMutex);
    MdlSet::iterator m = black_list.find(e);

    if (m != black_list.end()) {
        ModelBase::MEntry mEntry = *m;
        black_list.erase(m);
        mEntry.touch_time = Now();
        black_list.emplace(mEntry);
        return nullptr;
    }

    m = white_list.find(e);

    if (m != white_list.end()) {
        ModelBase::MEntry mEntry = *m;
        white_list.erase(m);
        mEntry.touch_time = Now();
        white_list.emplace(mEntry);
        return mEntry.mdl;
    }

    _Mem::Get()->pack_hlp(e.mdl);
    white_list.insert(e);
    return mdl;
}

void ModelBase::check_existence(Code *m0, Code *m1, Code *&_m0, Code *&_m1)
{
    MEntry e_m0(m0, false);
    std::lock_guard<std::mutex> guard(m_mdlMutex);
    MdlSet::iterator m = black_list.find(e_m0);

    if (m != black_list.end()) {
        ModelBase::MEntry mEntry = *m;
        black_list.erase(m);
        mEntry.touch_time = Now();
        black_list.emplace(mEntry);
        _m0 = _m1 = nullptr;
        return;
    }

    m = white_list.find(e_m0);

    if (m != white_list.end()) {
        ModelBase::MEntry mEntry = *m;
        white_list.erase(m);
        mEntry.touch_time = Now();
        white_list.emplace(mEntry);
        _m0 = mEntry.mdl;
        Code *rhs = m1->get_reference(m1->code(m1->code(MDL_OBJS).asIndex() + 2).asIndex());
        Code *im0 = rhs->get_reference(0);
        im0->set_reference(0, _m0); // change imdl m0 into imdl _m0.
    } else {
        _m0 = m0;
    }

    MEntry e_m1(m1, false);
    m = black_list.find(e_m1);

    if (m != black_list.end()) {
        ModelBase::MEntry mEntry = *m;
        black_list.erase(m);
        mEntry.touch_time = Now();
        black_list.emplace(mEntry);
        _m1 = nullptr;
        return;
    }

    m = white_list.find(e_m1);

    if (m != white_list.end()) {
        ModelBase::MEntry mEntry = *m;
        white_list.erase(m);
        mEntry.touch_time = Now();
        white_list.emplace(mEntry);
        _m1 = mEntry.mdl;
        return;
    }

    if (_m0 == m0) {
        _Mem::Get()->pack_hlp(m0);
        white_list.insert(e_m0);
    }

    _Mem::Get()->pack_hlp(m1);
    white_list.insert(e_m1);
    _m1 = m1;
}

void ModelBase::register_mdl_failure(Code *mdl)
{
    MEntry e(mdl, true);
    std::lock_guard<std::mutex> guard(m_mdlMutex);
    white_list.erase(e);
    black_list.insert(e);
}

void ModelBase::register_mdl_timeout(Code *mdl)
{
    MEntry e(mdl, true);
    std::lock_guard<std::mutex> guard(m_mdlMutex);
    white_list.erase(e);
}
}
