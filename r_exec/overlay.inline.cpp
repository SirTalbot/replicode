//	overlay.inline.h
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

namespace	r_exec{

	inline	void	Overlay::kill(){
		
		reductionCS.enter();
		alive=false;
		reductionCS.leave();
	}

	inline	r_code::Code	*Overlay::getObject()	const{	
		
		return	controller->getObject();
	}
	
	inline	r_exec::View	*Overlay::getView()	const{	
		
		return	controller->getView();
	}

	////////////////////////////////////////////////////////////////

	inline	bool	Controller::is_alive(){
		
		aliveCS.enter();
		bool	_alive=alive;
		aliveCS.leave();
		return	_alive;
	}
	
	inline	r_code::Code	*Controller::getObject()	const{
		
		return	view->object;
	}

	inline	r_exec::View	*Controller::getView()	const{
		
		return	(r_exec::View	*)view;
	}

	inline	_Mem	*Controller::get_mem()		const{
		
		return	mem;
	}
}