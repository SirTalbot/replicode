//	g_monitor.h
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

#ifndef	g_monitor_h
#define	g_monitor_h

#include	"binding_map.h"
#include	"overlay.h"


namespace	r_exec{

	class	HLPController;

	class	GMonitor:
	public	_Object{
	protected:
		P<Code>	goal;	//	mk.goal.
		P<Code>	super_goal;	//	mk.goal.
		uint64	expected_time_high;
		uint64	expected_time_low;

		P<BindingMap>	bindings;

		P<Code>	matched_pattern;

		CriticalSection	matchCS;
		bool			match;

		HLPController	*controller;
	public:
		GMonitor(	HLPController	*controller,
					BindingMap		*bindings,
					Code			*goal,
					Code			*super_goal,
					Code			*matched_pattern,
					uint64			expected_time_high,
					uint64			expected_time_low);
		virtual	~GMonitor();

		bool	is_alive();
		virtual	bool	reduce(Code	*input);
		void	update();

		uint64	get_deadline()	const{	return	expected_time_high;	}
	};
}


#endif