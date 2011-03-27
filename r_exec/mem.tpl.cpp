//	mem.tpl.cpp
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

#include	"init.h"
#include	"../r_code/replicode_defs.h"
#include	"operator.h"
#include	"factory.h"
#include	"cpp_programs.h"
#include	"../r_code/utils.h"
#include	<math.h>


namespace	r_exec{

	template<class	O>	Mem<O>::Mem():_Mem(){
	}

	template<class	O>	Mem<O>::~Mem(){
	}

	////////////////////////////////////////////////////////////////

	template<class	O>	r_code::Code	*Mem<O>::build_object(r_code::SysObject	*source){

		switch(source->code[0].getDescriptor()){
		case	Atom::GROUP:
			return	new	Group(source,this);
		default:
			return	new	O(source,this);
		}
	}

	template<class	O>	r_code::Code	*Mem<O>::build_object(Atom	head){

		r_code::Code	*object;
		switch(head.getDescriptor()){
		case	Atom::GROUP:
			object=new	Group();
			break;
		default:
			if(O::RequiresPacking())
				object=new	r_code::LObject();	//	temporary sand box for assembling code; will be packed into an O at injection time.
			else
				object=new	O();
			break;
		}

		object->code(0)=head;
		return	object;
	}

	////////////////////////////////////////////////////////////////

	template<class	O>	void	Mem<O>::delete_object(Code	*object){

		if(!IsNotification(object)	&&	object->code(0).getDescriptor()!=Atom::GROUP){

			object_registerCS.enter();
			object_register.erase(((O	*)object)->position_in_object_register);
			object_registerCS.leave();
		}

		objectsCS.enter();
		objects.erase(object->position_in_objects);
		objectsCS.leave();
	}

	////////////////////////////////////////////////////////////////

	template<class	O>	bool	Mem<O>::load(std::vector<r_code::Code	*>	*objects){	//	NB: no cov at init time.

		uint32	i;
		reduction_cores=new	ReductionCore	*[reduction_core_count];
		for(i=0;i<reduction_core_count;++i)
			reduction_cores[i]=new	ReductionCore();
		time_cores=new	TimeCore	*[time_core_count];
		for(i=0;i<time_core_count;++i)
			time_cores[i]=new	TimeCore();

		last_oid=0;

		//	load root (always comes first).
		_root=(Group	*)(*objects)[0];
		this->objects.push_back(_root);
		initial_groups.push_back(_root);
		_root->setOID(get_oid());

		UNORDERED_MAP<Code	*,Code	*>	abstraction_map;

		for(uint32	i=1;i<objects->size();++i){	//	skip root as it has no initial views.

			Code	*object=(*objects)[i];
			switch(object->get_axiom()){
			case	SysObject::STDIN_GRP:
				_stdin=(Group	*)(*objects)[i];
				break;
			case	SysObject::STDOUT_GRP:
				_stdout=(Group	*)(*objects)[i];
				break;
			case	SysObject::SELF_ENT:
				_self=(O	*)(*objects)[i];
				break;
			}

			Code	*abstracted_object=NULL;
			switch(object->code(0).getDescriptor()){
			case	Atom::MODEL:
			case	Atom::COMPOSITE_STATE:	//	these constructs are assumed not to be already abstracted.
				abstracted_object=_Mem::Get()->abstract_high_level_pattern(object);	//	original is not deleted if abstracted.
				abstraction_map[object]=abstracted_object;
				break;
			case	Atom::INSTANTIATED_PROGRAM:	//	refine the opcode depending on the inputs and the program type.
				if(object->get_reference(0)->code(0).asOpcode()==Opcodes::Pgm){

					if(object->get_reference(0)->code(object->get_reference(0)->code(PGM_INPUTS).asIndex()).getAtomCount()==0)
						object->code(0)=Atom::InstantiatedInputLessProgram(object->code(0).asOpcode(),object->code(0).getAtomCount());
				}else
					object->code(0)=Atom::InstantiatedAntiProgram(object->code(0).asOpcode(),object->code(0).getAtomCount());
				break;
			}

			for(uint16	j=0;j<object->references_size();++j){	//	update the references to objects that have been abstracted.

				UNORDERED_MAP<Code	*,Code	*>::const_iterator	a=abstraction_map.find(object->get_reference(j));
				if(a!=abstraction_map.end())
					object->set_reference(j,a->second);
			}

			UNORDERED_SET<r_code::View	*,r_code::View::Hash,r_code::View::Equal>::const_iterator	v;
			if(abstracted_object){	//	copy the object's views for the abstracted_object; set the object's views resilience to 1 and activation to 0.

				for(v=object->views.begin();v!=object->views.end();++v){

					r_exec::View	*copy=new	View((r_exec::View	*)*v,true);
					copy->set_object(abstracted_object);
					abstracted_object->views.insert(copy);

					//	init hosts' member_set.
					Group	*host=copy->get_host();
					(*v)->code(VIEW_RES)=Atom::Float(1);
					(*v)->code(VIEW_ACT)=Atom::Float(0);

					if(!host->load(copy,abstracted_object))
						return	false;
				}

				abstracted_object->setOID(get_oid());
				abstracted_object->position_in_objects=this->objects.insert(this->objects.end(),abstracted_object);
				abstracted_object->is_registered=true;
				((O	*)abstracted_object)->position_in_object_register=object_register.insert((O	*)abstracted_object).first;
			}
			
			for(v=object->views.begin();v!=object->views.end();++v){

				//	init hosts' member_set.
				View	*view=(r_exec::View	*)*v;
				view->set_object(object);
				Group	*host=view->get_host();

				if(!host->load(view,object))
					return	false;
			}

			object->setOID(get_oid());
			object->position_in_objects=this->objects.insert(this->objects.end(),object);
			object->is_registered=true;
			if(object->code(0).getDescriptor()!=Atom::GROUP)	//	load non-group object in register.
				((O	*)object)->position_in_object_register=object_register.insert((O	*)object).first;
			else
				initial_groups.push_back((Group	*)object);	//	convenience to create initial update jobs - see start().
		}

		return	true;
	}

	template<class	O>	void	Mem<O>::init_timings(uint64	now){

		std::list<Code	*>::const_iterator	o;
		for(o=objects.begin();o!=objects.end();++o){

			uint16	opcode=(*o)->code(0).asOpcode();
			if(opcode==Opcodes::Fact	||	opcode==Opcodes::AntiFact)	//	uset facts' times as offset from now.
				Utils::SetTimestamp<Code>(*o,FACT_TIME,Utils::GetTimestamp<Code>(*o,FACT_TIME)+now);
		}
	}

	////////////////////////////////////////////////////////////////

	template<class	O>	r_comp::Image	*Mem<O>::get_image(){

		r_comp::Image	*image=new	r_comp::Image();
		image->timestamp=Now();
		std::list<Code	*>::const_iterator	i;
		for(i=objects.begin();i!=objects.end();++i)
			if(!(*i)->is_invalidated())
				image->operator	<<(*i);
		return	image;
	}

	////////////////////////////////////////////////////////////////

	template<class	O>	Code	*Mem<O>::check_existence(Code	*object){

		if(object->code(0).getDescriptor()==Atom::GROUP)	//	groups are always new.
			return	object;

		O	*_object;
		if(O::RequiresPacking())			//	false if LObject, true for network-aware objects.
			_object=O::Pack(object,this);	//	non compact form will be deleted (P<> in view) if not an instance of O; compact forms are left unchanged.
		else
			_object=(O	*)object;

		object_registerCS.enter();
		UNORDERED_SET<O	*,typename	O::Hash,typename	O::Equal>::const_iterator	it=object_register.find(_object);
		if(it!=object_register.end()){

			object_registerCS.leave();
			return	*it;
		}
		object_registerCS.leave();
		return	_object;
	}

	template<class	O>	void	Mem<O>::inject(O	*object,View	*view){

		view->set_object(object);
		injectNewObject(view);
	}

	template<class	O>	void	Mem<O>::inject(View	*view){

		Group	*host=view->get_host();

		host->enter();
		if(host->is_invalidated()){

			host->leave();
			return;
		}
		host->leave();

		uint64	now=Now();
		uint64	ijt=view->get_ijt();

		if(view->object->is_registered){	//	existing object.

			if(ijt<=now)
				inject_existing_object(view,view->object,host,true);
			else{
				
				P<TimeJob>	j=new	EInjectionJob(view,ijt);
				time_job_queue->push(j);
			}
		}else{								//	new object.

			if(ijt<=now)
				inject_new_object(view);
			else{
				
				P<TimeJob>	j=new	InjectionJob(view,ijt);
				time_job_queue->push(j);
			}
		}
	}

	////////////////////////////////////////////////////////////////

	template<class	O>	void	Mem<O>::inject_new_object(View	*view){

		Group	*host=view->get_host();

		uint64	now=Now();

		switch(view->object->code(0).getDescriptor()){
		case	Atom::GROUP:
			bind<Group>(view,now);

			host->enter();
			host->injectGroup(view,now);
			host->leave();
			break;
		default:
			object_registerCS.enter();
			((O	*)view->object)->position_in_object_register=object_register.insert((O	*)view->object).first;
			object_registerCS.leave();

			bind<O>(view,now);

			host->enter();
			host->inject(view,now);
			host->leave();
			break;
		}
	}

	template<class	O>	void	Mem<O>::inject_notification(View	*view,bool	lock){	//	no notification for notifications; no registration either (object_register and object_io_map) and no cov.
																										//	notifications are ephemeral: they are not held by the marker sets of the object they refer to; this implies no propagation of saliency changes trough notifications.
		Group	*host=view->get_host();
		LObject	*object=(LObject	*)view->object;

		uint64	now=Now();
		
		bind<LObject>(view,now);

		view->code(VIEW_RES)=Atom::Float(ntf_mk_res);
		
		if(lock)
			host->enter();
		host->inject_notification(view);
		if(lock)
			host->leave();
	}
}