//	pgm_controller.h
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

#ifndef pgm_controller_h
#define pgm_controller_h

#include "pgm_overlay.h"


namespace r_exec
{

class dll_export _PGMController:
    public OController
{
protected:
    bool run_once;

    _PGMController(r_code::View *ipgm_view);
    virtual ~_PGMController();
public:
    Code *get_core_object() const
    {
        return getObject()->get_reference(0);
    }
};

// TimeCores holding InputLessPGMSignalingJob trigger the injection of the productions.
// No overlays.
class dll_export InputLessPGMController:
    public _PGMController
{
public:
    InputLessPGMController(r_code::View *ipgm_view);
    ~InputLessPGMController();

    /// next job will be pushed by the rMem upon processing the current signaling job, i.e. right after exiting this function.
    void signal_input_less_pgm();

private:
    std::mutex m_hostMutex;
};

// Controller for programs with inputs.
class dll_export PGMController:
    public _PGMController
{
public:
    PGMController(r_code::View *ipgm_view);
    virtual ~PGMController();

    void take_input(r_exec::View *input);
    void reduce(r_exec::View *input);

    void notify_reduction();
};

// Signaled by TimeCores (holding AntiPGMSignalingJob).
// Possible recursive locks: signal_anti_pgm()->overlay->inject_productions()->mem->inject()->injectNow()->inject_reduction_jobs()->overlay->take_input().
class dll_export AntiPGMController:
    public _PGMController
{
private:
    bool successful_match;
    std::mutex m_hostMutex;

    void push_new_signaling_job();
public:
    AntiPGMController(r_code::View *ipgm_view);
    ~AntiPGMController();

    void take_input(r_exec::View *input);
    void reduce(r_exec::View *input);
    void signal_anti_pgm();

    /// one anti overlay matched all its inputs, timings and guards.
    void restart();
};
}

#endif
