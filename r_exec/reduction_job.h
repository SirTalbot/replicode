//	reduction_job.h
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

#ifndef reduction_job_h
#define reduction_job_h


#include <r_exec/view.h>       // for View
#include <stdint.h>            // for uint64_t

#include "CoreLibrary/base.h"  // for P, _Object
#include "CoreLibrary/dll.h"   // for dll_export

namespace r_exec
{

class dll_export _ReductionJob:
    public _Object
{
protected:
    _ReductionJob();
public:
    uint64_t ijt; // time of injection of the job in the pipe.
    virtual bool update(uint64_t now) = 0; // return false to shutdown the reduction core.
    virtual void debug() {}
};

template<class _P> class ReductionJob:
    public _ReductionJob
{
public:
    P<View> input;
    P<_P> processor;
    ReductionJob(View *input, _P *processor): _ReductionJob(), input(input), processor(processor) {}
    bool update(uint64_t now);
    void debug()
    {
        processor->debug(input);
    }
};

template<class _P, class T, class C> class BatchReductionJob:
    public _ReductionJob
{
public:
    P<_P> processor; // the controller that will process the job.
    P<T> trigger; // the event that triggered the job.
    P<C> controller; // the controller that produced the job.
    BatchReductionJob(_P *processor, T *trigger, C *controller): _ReductionJob(), processor(processor), trigger(trigger), controller(controller) {}
    bool update(uint64_t now);
};

class dll_export ShutdownReductionCore:
    public _ReductionJob
{
public:
    bool update(uint64_t now);
};

class dll_export AsyncInjectionJob:
    public _ReductionJob
{
public:
    P<View> input;
    AsyncInjectionJob(View *input): _ReductionJob(), input(input) {}
    bool update(uint64_t now);
};
}

#include <r_exec/reduction_job.tpl.h>

#endif
