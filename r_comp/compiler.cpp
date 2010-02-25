#include	"compiler.h"
#include <string.h>


namespace	r_comp{

	static	bool	Output=true;

	Compiler::Compiler():out_stream(NULL),current_object(NULL){
	}

	Compiler::~Compiler(){

		if(out_stream)
			delete	out_stream;
	}

	Compiler::State	Compiler::save_state(){

		State	s(this);
		return	s;
	}

	void	Compiler::restore_state(State	s){

		in_stream->seekg(s.stream_ptr);
		state=s;
	}

	void	Compiler::set_error(std::string	s){

		if(!err	&&	Output){

			err=true;
			*error=s;
		}
	}

	////////////////////////////////////////////////////////////////////////////////////////////////

	bool	Compiler::compile(std::istream	*stream,r_comp::Image	*_image,r_code::Image	*&image,std::string	*error){

		this->in_stream=stream;
		this->image=image=new	r_code::Image();
		this->error=error;
		this->err=false;

		this->_image=_image;
		current_object_index=0;
		while(!in_stream->eof()){

			switch(in_stream->peek()){
			case	'!':
				set_error("error: found preprocessor directive");
				delete	_image;
				return	false;
			default:
				if(in_stream->eof())
					return	true;
				if(!compile()){

					delete	_image;
					return	false;
				}
				current_object_index++;
				break;
			}
			char	c=(char)in_stream->get();
			if(c!=-1)
				in_stream->putback(c);
		}
		*_image>>image;

		return	true;
	}

	bool	Compiler::compile(){

		local_references.clear();
		bool	indented=false;
		bool	lbl=false;

		current_view_index=-1;

		std::string	l;
		while(indent(false));
		if(label(l)){

			lbl=true;
			OUTPUT<<"lbl_"<<l<<":";
		}
		if(!expression_begin(indented)){

			if(lbl)
				set_error("error: label not followed by an expression");
			else
				set_error("syntax error: missing expression opening");
			return	false;
		}
		indent(false);

		if(!sys_object(current_class)){

			set_error("error: unknown class");
			return	false;
		}else{

			current_object=new	SysObject();
			if(lbl)
				global_references[l]=Reference(_image->code_segment.objects.size(),current_class);
		}

		OUTPUT<<current_class.str_opcode;
		
		current_object->code[0]=current_class.atom;
		if(current_class.atom.getAtomCount()){

			if(!right_indent(true)){

				if(!separator(false)){

					set_error("syntax error: missing separator/right_indent after head");
					return	false;
				}
			}else
				OUTPUT<<" ";
			uint16	extent_index=current_class.atom.getAtomCount()+1;
			if(!expression_tail(indented,current_class,1,extent_index,true))
				return	false;
		}

		SysObject	*sys_object=(SysObject	*)current_object;	//	current_object will point to views, if any

		OUTPUT<<"\n\n";
		
		//	compile view set
		//	input format:
		//	[]
		//		[view-data]
		//		...
		//		[view-data]
		//	or:
		//	|[]

		while(indent(false));
		std::streampos	i=in_stream->tellg();
		if(!match_symbol("|[]",false)){

			in_stream->seekg(i);
			if(!set_begin(indented))
				return	false;

			indent(false);
			OUTPUT<<"[";

			current_class=*current_class.get_member_class(_image->definition_segment,"vw");
			current_class.use_as=StructureMember::CLASS;

			uint16	count=0;
			bool	_indented=false;
			while(!in_stream->eof()){

				current_object=new	SysView();
				current_view_index=count;
				uint16	extent_index=0;

				if(set_end(indented)){

					if(!count){

						set_error(" syntax error: use |[] for empty sets");
						delete	current_object;
						return	false;
					}else{

						delete	current_object;
						OUTPUT<<"]";
						break;
					}
				}
				if(count){
					
					if(!_indented){

						if(!right_indent(true)){

							if(!separator(false)){

								set_error("syntax error: missing separator between 2 elements");
								delete	current_object;
								return	false;
							}
						}else
							OUTPUT<<" ";
					}else{

						_indented=false;
						OUTPUT<<" ";
					}
				}
				if(!read_set(_indented,true,&current_class,0,extent_index,true)){
							
					set_error(" error: illegal element in set");
					delete	current_object;
					return	false;
				}
				count++;
				sys_object->view_set.push_back((SysView	*)current_object);
			}
		}else
			OUTPUT<<"|[]";
		OUTPUT<<"\n\n";

		OUTPUT<<"OBJECT\n";
		sys_object->trace();//uint32	a;std::cin>>a;
		OUTPUT<<"\n";

		_image->addObject(sys_object);
		return	true;
	}

	bool	Compiler::read(const	StructureMember	&m,bool	&indented,bool	enforce,uint16	write_index,uint16	&extent_index,bool	write){

		if(Class	*p=m.get_class(_image->definition_segment)){

			p->use_as=m.getIteration();
			return	(this->*m.read())(indented,enforce,p,write_index,extent_index,write);
		}
		return	(this->*m.read())(indented,enforce,NULL,write_index,extent_index,write);
	}

	bool	Compiler::getReferenceIndex(const	std::string	reference_name,const	ReturnType	t,ImageObject	*object,uint16	&index,Class	*&_class){

		UNORDERED_MAP<std::string,Reference>::iterator	it=global_references.find(reference_name);
		if(it!=global_references.end()	&&	(t==ANY	||	(t!=ANY	&&	it->second._class.type==t))){

			_class=&it->second._class;
			for(uint16	j=0;j<object->reference_set.size();++j)
				if(object->reference_set[j]==it->second.index){	//	the object has already been referenced

					index=j;	//	rptr points to object->reference_set[j], which in turn points to it->second.index
					return	true;
				}
			object->reference_set.push_back(it->second.index);	//	add new reference to the object
			index=object->reference_set.size()-1;	//	//	rptr points to the last element of object->reference_set, which in turn points to it->second.index
			return	true;
		}
		return	false;
	}
	////////////////////////////////////////////////////////////////////////////////////////////////

	bool	Compiler::comment(){

		std::streampos	i=in_stream->tellg();
		bool	started=false;
		bool	continuation=false;	//	continuation mark detected
		bool	period=false;		//	to detect 2 subsequent '.'
		while(!in_stream->eof()){

			switch(char	c=(char)in_stream->get()){
			case	';':
				if(!started)
					started=true;
				break;
			case	'.':
				if(!started)
					goto	return_false;
				if(continuation){
				
					set_error(" syntax error: ...");
					goto	return_false;
				}
				if(period)
					continuation=true;
				period=true;
				break;
			case	NEWLINE:
				if(!continuation){

					in_stream->putback(c);
					return	true;
				}
				continuation=period=false;
				break;
			default:
				if(!started)
					goto	return_false;
				period=false;
				break;
			}
		}
return_false:
		in_stream->seekg(i);
		in_stream->clear();
		return	false;
	}

	bool	Compiler::indent(bool	pushback){

		comment();
		std::string	s;
		s+=NEWLINE;
		for(uint16	j=0;j<3*state.indents;j++)
			s+=' ';
		return	match_symbol(s.c_str(),pushback);
	}

	bool	Compiler::right_indent(bool	pushback){	//	no look ahead when pushback==true

		comment();
		if(pushback){

			if(state.right_indents_ahead)
				return	true;
			std::string	s;
			s+=NEWLINE;
			for(uint16	j=0;j<3*(state.indents+1);j++)
				s+=' ';
			return	match_symbol(s.c_str(),true);
		}
		if(state.right_indents_ahead){

			state.indents++;
			state.right_indents_ahead--;
			return	true;
		}
		std::string	s;
		s+=NEWLINE;
		for(uint16	j=0;j<3*(state.indents+1);j++)
			s+=' ';
		if(!match_symbol(s.c_str(),false))
			return	false;
		state.indents++;
		s="   ";
		while(match_symbol(s.c_str(),false))	//	look ahead for more indents
			state.right_indents_ahead++;
		return	true;
	}

	bool	Compiler::left_indent(bool	pushback){	//	no look ahead when pushback==true

		comment();
		if(indent(true))
			return	false;
		if(pushback){

			if(state.left_indents_ahead)
				return	true;
			std::string	s;
			s+=NEWLINE;
			for(uint16	j=0;j<3*(state.indents-1);j++)
				s+=' ';
			return	match_symbol(s.c_str(),true);
		}
		if(state.left_indents_ahead){

			if(state.indents)
				state.indents--;
			state.left_indents_ahead--;
			return	true;
		}
		std::string	s;
		s+=NEWLINE;
		if(!match_symbol(s.c_str(),false))
			return	false;
		uint16	expected=state.indents-1;
		if(expected<=0){

			if(state.indents)
				state.indents--;
			return	true;
		}
		state.left_indents_ahead=expected;	//	look ahead for more indents
		s="   ";
		for(uint16	j=0;j<expected;j++){

			if(match_symbol(s.c_str(),false))
				state.left_indents_ahead--;
		}
		if(state.indents)
			state.indents--;
		return	true;
	}

	bool	Compiler::separator(bool	pushback){

		if(indent(pushback)){

			if(!pushback)
				OUTPUT<<" ";
			return	true;
		}
		char	c=(char)in_stream->get();
		if(c==' '){

			if(pushback)
				in_stream->putback(c);
			else
				OUTPUT<<" ";
			return	true;
		}
		in_stream->clear();
		in_stream->putback(c);
		return	false;
	}

	bool	Compiler::symbol_expr(std::string	&s){

		std::streampos	i=in_stream->tellg();
		uint16	count=0;
		while(!in_stream->eof()){

			switch(char	c=(char)in_stream->get()){
			case	':':
			case	' ':
			case	NEWLINE:
			case	';':
			case	')':
				if(count){

					in_stream->putback(c);
					return	true;
				}
			case	'(':
			case	'[':
			case	']':
			case	'.':
				if(s=="mk"){

					s+='.';
					break;
				}
				in_stream->seekg(i);
				return	false;
			default:
				count++;
				s+=c;
				break;
			}
		}
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::symbol_expr_set(std::string	&s){

		std::streampos	i=in_stream->tellg();
		uint16	count=0;
		while(!in_stream->eof()){

			switch(char	c=(char)in_stream->get()){
			case	' ':
			case	NEWLINE:
			case	';':
			case	')':
			case	']':
				if(count){

					in_stream->putback(c);
					return	true;
				}
			case	'(':
			case	'[':
			case	'.':
			case	':':
				in_stream->seekg(i);
				return	false;
			default:
				count++;
				s+=c;
				break;
			}
		}
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::match_symbol_separator(const	char	*symbol,bool	pushback){

		if(match_symbol(symbol,pushback)){

			if(separator(true)	||	right_indent(true)	||	left_indent(true))
				return	true;
			char	c=(char)in_stream->peek();
			if(c==')'	||	c==']')					
				return	true;
		}
		return	false;
	}

	bool	Compiler::match_symbol(const	char	*symbol,bool	pushback){

		std::streampos	i=in_stream->tellg();
		for(uint32	j=0;j<strlen(symbol);j++){
			
			if(in_stream->eof()	||	((char)in_stream->get())!=symbol[j]){

				in_stream->clear();
				in_stream->seekg(i);
				return	false;
			}
		}
		if(pushback)
			in_stream->seekg(i);
		return	true;
	}

	bool	Compiler::member(std::string	&s){

		std::streampos	i=in_stream->tellg();
		s="";
		uint16	count=0;
		while(!in_stream->eof()){

			switch(char	c=(char)in_stream->get()){
			case	' ':
			case	NEWLINE:
			case	';':
			case	')':
			case	']':
			case	'.':
				if(count){

					in_stream->putback(c);
					return	true;
				}
			case	'(':
			case	'[':
			case	':':
				in_stream->seekg(i);
				return	false;
			default:
				count++;
				s+=c;
				break;
			}
		}
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::expression_begin(bool	&indented){

		if(right_indent(false)){

			indented=true;
			return	true;
		}
		std::streampos	i=in_stream->tellg();
		if(indent(false)){

			char	c=(char)in_stream->get();
			if(c=='(')
				return	true;
			in_stream->clear();
		}
		in_stream->seekg(i);
		char	c=(char)in_stream->get();
		if(c=='(')
			return	true;
		in_stream->clear();
		in_stream->putback(c);
		return	false;
	}

	bool	Compiler::expression_end(bool	indented){

		if(indented)
			return	left_indent(false);
		std::streampos	i=in_stream->tellg();
		if(indent(false)){

			char	c=(char)in_stream->get();
			if(c==')')
				return	true;
			in_stream->clear();
		}
		in_stream->seekg(i);
		char	c=(char)in_stream->get();
		if(c==')')
			return	true;
		in_stream->clear();
		in_stream->putback(c);
		return	false;
	}

	bool	Compiler::set_begin(bool	&indented){

		std::streampos	i=in_stream->tellg();
		if(match_symbol("[]",false)){

			if(right_indent(false)){

				indented=true;
				return	true;
			}else{

				set_error(" syntax error: [] not followed by indent");
				return	false;
			}
		}
		in_stream->seekg(i);
		if(indent(false)){

			char	c=(char)in_stream->get();
			if(c=='[')
				return	true;
			in_stream->clear();
		}
		in_stream->seekg(i);
		char	c=(char)in_stream->get();
		if(c=='[')
			return	true;
		in_stream->clear();
		in_stream->putback(c);
		return	false;
	}

	bool	Compiler::set_end(bool	indented){

		if(indented)
			return	left_indent(false);
		std::streampos	i=in_stream->tellg();
		if(indent(false)){

			char	c=(char)in_stream->get();
			if(c==']')
				return	true;
			in_stream->clear();
		}
		in_stream->seekg(i);
		char	c=(char)in_stream->get();
		if(c==']')
			return	true;
		in_stream->clear();
		in_stream->putback(c);
		return	false;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////

	bool	Compiler::nil(){

		std::streampos	i=in_stream->tellg();
		if(match_symbol_separator("nil",false))
			return	true;
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::nil_nb(){

		std::streampos	i=in_stream->tellg();
		if(match_symbol_separator("|nb",false))
			return	true;
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::nil_us(){

		std::streampos	i=in_stream->tellg();
		if(match_symbol_separator("|ms",false))
			return	true;
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::forever(){

		std::streampos	i=in_stream->tellg();
		if(match_symbol_separator("forever",false))
			return	true;
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::nil_nid(){

		std::streampos	i=in_stream->tellg();
		if(match_symbol_separator("|nid",false))
			return	true;
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::nil_did(){

		std::streampos	i=in_stream->tellg();
		if(match_symbol_separator("|did",false))
			return	true;
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::nil_fid(){

		std::streampos	i=in_stream->tellg();
		if(match_symbol_separator("|fid",false))
			return	true;
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::nil_bl(){

		std::streampos	i=in_stream->tellg();
		if(match_symbol_separator("|bl",false))
			return	true;
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::nil_st(){

		std::streampos	i=in_stream->tellg();
		if(match_symbol_separator("|st",false))
			return	true;
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::label(std::string	&l){

		std::streampos	i=in_stream->tellg();
		if(symbol_expr(l)	&&	(char)in_stream->get()==':')
			return	true;
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::variable(std::string	&l){

		std::streampos	i=in_stream->tellg();
		if(symbol_expr(l)	&&	(char)in_stream->get()==':'){

			in_stream->seekg(i);
			std::string	_l=l+':';
			if(match_symbol_separator(_l.c_str(),false))
				return	true;
		}
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::this_(){

		std::streampos	i=in_stream->tellg();
		if(match_symbol_separator("this",false))
			return	true;
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::self(){

		std::streampos	i=in_stream->tellg();
		if(match_symbol_separator("self",false))
			return	true;
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::stdin_(){

		std::streampos	i=in_stream->tellg();
		if(match_symbol_separator("stdin",false))
			return	true;
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::stdout_(){

		std::streampos	i=in_stream->tellg();
		if(match_symbol_separator("stdout",false))
			return	true;
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::root(){

		std::streampos	i=in_stream->tellg();
		if(match_symbol_separator("root",false))
			return	true;
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::local_reference(uint16	&index,const	ReturnType	t){

		std::streampos	i=in_stream->tellg();
		std::string	r;
		if(symbol_expr_set(r)){

			UNORDERED_MAP<std::string,Reference>::iterator	it=local_references.find(r);
			if(it!=local_references.end()	&&	(t==ANY	||	(t!=ANY	&&	it->second._class.type==t))){

				index=it->second.index;
				return	true;
			}
		}
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::global_reference(uint16	&index,const	ReturnType	t){

		std::streampos	i=in_stream->tellg();
		std::string	r;
		if(symbol_expr_set(r)){

			Class	*unused;
			if(getReferenceIndex(r,t,current_object,index,unused))
				return	true;
		}
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::this_indirection(std::vector<uint16>	&v,const	ReturnType	t){

		std::streampos	i=in_stream->tellg();
		if(match_symbol("this.",false)){

			Class		*p=&current_class;
			Class		*_p;
			std::string	m;
			uint16		index;
			ReturnType	type;
			while(member(m)){

				if(!p->get_member_index(_image->definition_segment,m,index,_p)){

					set_error(" error: "+m+" is not a member of "+p->str_opcode);
					break;
				}
				type=p->get_member_type(index);
				v.push_back(index);
				char	c=(char)in_stream->get();
				if(c=='.'){

					if(!_p){

						set_error(" error: "+m+" is not a structure");
						break;
					}
					p=_p;
				}else{

					if(t==ANY	||	(t!=ANY	&&	type==t)){
					
						in_stream->putback(c);
						return	true;
					}
					break;
				}
			}
		}
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::local_indirection(std::vector<uint16>	&v,const	ReturnType	t){

		std::streampos	i=in_stream->tellg();
		std::string	m;
		std::string	path="";
		Class		*p;
		if(member(m)	&&	(char)in_stream->get()=='.'){	//	first m is a reference to a label or a variable

			uint16		index;
			ReturnType	type;
			UNORDERED_MAP<std::string,Reference>::iterator	it=local_references.find(m);
			if(it!=local_references.end()){

				index=it->second.index;
				v.push_back(index);
				p=&it->second._class;
				Class	*_p;
				while(member(m)){

					if(!p->get_member_index(_image->definition_segment,m,index,_p)){

						set_error(" error: "+m+" is not a member of "+p->str_opcode);
						break;
					}
					type=p->get_member_type(index);
					v.push_back(index);
					path+='.';
					path+=m;
					char	c=(char)in_stream->get();
					if(c=='.'){

						if(!_p){

							set_error(" error: "+path+" is not an addressable structure");
							break;
						}
						p=_p;
					}else{

						if(t==ANY	||	(t!=ANY	&&	type==t)){
						
							in_stream->putback(c);
							return	true;
						}
						break;
					}
				}
			}
		}
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::global_indirection(std::vector<uint16>	&v,const	ReturnType	t){

		std::streampos	i=in_stream->tellg();
		std::string	m;
		Class		*p;
		if(member(m)	&&	(char)in_stream->get()=='.'){	//	first m is a reference

			uint16		index;
			ReturnType	type;
			if(getReferenceIndex(m,ANY,current_object,index,p)){

				v.push_back(index);
				Class	*_p;
				bool	first_member=true;
				while(member(m)){

					if(!p->get_member_index(_image->definition_segment,m,index,_p)){

						set_error(" error: "+m+" is not a member of "+p->str_opcode);
						break;
					}
					type=p->get_member_type(index);
					if(first_member	&&	index==0)	//	indicates the first member; store in the RObject, after the leading atom, hence index=1
						index=1;
					first_member=false;
					v.push_back(index);
					char	c=(char)in_stream->get();
					if(c=='.'){

						if(!_p){

							set_error(" error: "+m+" is not a structure");
							break;
						}
						p=_p;
					}else{

						if(t==ANY	||	(t!=ANY	&&	type==t)){
						
							in_stream->putback(c);
							return	true;
						}
						break;
					}
				}
			}
		}
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::wildcard(){

		std::streampos	i=in_stream->tellg();
		if(match_symbol_separator(":",false))
			return	true;
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::tail_wildcard(){

		std::streampos	i=in_stream->tellg();
		if(match_symbol("::",false)){
			
			if(left_indent(true)){

				state.no_arity_check=true;
				return	true;
			}
			char	c=(char)in_stream->peek();
			if(c==')'	||	c==']'){

				state.no_arity_check=true;
				return	true;
			}
		}
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::number(float32	&n){

		std::streampos	i=in_stream->tellg();
		if(match_symbol("0x",true)){

			in_stream->clear();
			in_stream->seekg(i);
			return	false;
		}
		*in_stream>>std::dec>>n;
		if(in_stream->fail()	||	in_stream->eof()){

			in_stream->clear();
			in_stream->seekg(i);
			return	false;
		}
		if(match_symbol("us",true)){

			in_stream->clear();
			in_stream->seekg(i);
			return	false;
		}
		return	true;
	}

	bool	Compiler::hex(uint32	&h){

		std::streampos	i=in_stream->tellg();
		if(!match_symbol("0x",false)){

			in_stream->clear();
			in_stream->seekg(i);
			return	false;
		}
		*in_stream>>std::hex>>h;
		if(in_stream->fail()	||	in_stream->eof()){

			in_stream->clear();
			in_stream->seekg(i);
			return	false;
		}
		return	true;
	}

	bool	Compiler::boolean(bool	&b){

		std::streampos	i=in_stream->tellg();
		if(match_symbol_separator("true",false))
			return	true;
		if(match_symbol_separator("false",false))
			return	true;
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::timestamp(uint64	&ts){

		std::streampos	i=in_stream->tellg();
		if(match_symbol("0x",true)){

			in_stream->clear();
			in_stream->seekg(i);
			return	false;
		}
		*in_stream>>std::dec>>ts;
		if(in_stream->fail()	||	in_stream->eof()){

			in_stream->clear();
			in_stream->seekg(i);
			return	false;
		}
		if(!match_symbol("us",false)){

			in_stream->clear();
			in_stream->seekg(i);
			return	false;
		}
		return	true;
	}

	bool	Compiler::str(std::string	&s){

		std::streampos	i=in_stream->tellg();
		uint16	count=0;
		bool	started=false;
		while(!in_stream->eof()){

			switch(char	c=(char)in_stream->get()){
			case	'"':
				if(!count){

					started=true;
					break;
				}
				return	true;
			default:
				if(!started){

					in_stream->clear();
					in_stream->seekg(i);
					return	false;
				}
				count++;
				s+=c;
				break;
			}
		}
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::object(Class	&p){

		if(sys_object(p))
			return	true;
		std::streampos	i=in_stream->tellg();
		std::string	s;
		if(!symbol_expr(s)){

			in_stream->seekg(i);
			return	false;
		}
		UNORDERED_MAP<std::string,Class>::const_iterator	it=_image->definition_segment->classes.find(s);
		if(it==_image->definition_segment->classes.end()){

			in_stream->seekg(i);
			return	false;
		}
		p=it->second;
		return	true;
	}

	bool	Compiler::object(const	Class	&p){

		if(sys_object(p))
			return	true;
		std::streampos	i=in_stream->tellg();
		if(!match_symbol_separator(p.str_opcode.c_str(),false)){

			in_stream->seekg(i);
			return	false;
		}
		return	true;
	}

	bool	Compiler::sys_object(Class	&p){

		std::streampos	i=in_stream->tellg();
		std::string	s;
		if(!symbol_expr(s)){

			in_stream->seekg(i);
			return	false;
		}
		UNORDERED_MAP<std::string,Class>::const_iterator	it=_image->definition_segment->sys_classes.find(s);
		if(it==_image->definition_segment->sys_classes.end()){

			in_stream->seekg(i);
			return	false;
		}
		p=it->second;
		return	true;
	}

	bool	Compiler::sys_object(const	Class	&p){

		std::streampos	i=in_stream->tellg();
		if(!match_symbol_separator(p.str_opcode.c_str(),false)){

			in_stream->seekg(i);
			return	false;
		}
		return	true;
	}

	bool	Compiler::marker(Class	&p){

		std::streampos	i=in_stream->tellg();
		if(!match_symbol("mk.",false)){

			in_stream->seekg(i);
			return	false;
		}
		std::string	s;
		if(!symbol_expr(s)){

			in_stream->seekg(i);
			return	false;
		}
		UNORDERED_MAP<std::string,Class>::const_iterator	it=_image->definition_segment->sys_classes.find("mk."+s);
		if(it==_image->definition_segment->sys_classes.end()){

			in_stream->seekg(i);
			return	false;
		}
		p=it->second;
		return	true;
	}

	bool	Compiler::op(Class	&p,const	ReturnType	t){	//	return true if type matches t or ANY

		std::streampos	i=in_stream->tellg();
		std::string	s;
		if(!symbol_expr(s)){

			in_stream->seekg(i);
			return	false;
		}
		UNORDERED_MAP<std::string,Class>::const_iterator	it=_image->definition_segment->classes.find(s);
		if(it==_image->definition_segment->classes.end()	|| (t!=ANY	&&	it->second.type!=ANY	&&	it->second.type!=t)){

			in_stream->seekg(i);
			return	false;
		}
		p=it->second;
		return	true;
	}

	bool	Compiler::op(const	Class	&p){

		std::streampos	i=in_stream->tellg();
		if(!match_symbol_separator(p.str_opcode.c_str(),false)){

			in_stream->seekg(i);
			return	false;
		}
		return	true;
	}

	bool	Compiler::function(Class	&p){

		std::streampos	i=in_stream->tellg();
		std::string	s;
		if(!symbol_expr(s)){

			in_stream->seekg(i);
			return	false;
		}
		UNORDERED_MAP<std::string,Class>::const_iterator	it=_image->definition_segment->classes.find(s);
		if(it==_image->definition_segment->classes.end()){

			in_stream->seekg(i);
			return	false;
		}
		p=it->second;
		return	true;
	}

	bool	Compiler::expression_head(Class	&p,const	ReturnType	t){
		
		indent(false);
		if(t==ANY){

			if(!object(p))
				if(!marker(p))
					if(!op(p,ANY))
						return	false;
		}else	if(!op(p,t))
			return	false;
		OUTPUT<<"("<<p.str_opcode;
		if(p.atom.getAtomCount()){

			if(!right_indent(true)){

				if(!separator(false)){

					set_error("syntax error: missing separator/right_indent after head");
					return	false;
				}
			}else
				OUTPUT<<" ";
		}
		return	true;
	}

	bool	Compiler::expression_head(const	Class	&p){
		
		indent(false);
		if(!object(p))
			if(!op(p))
				return	false;
		OUTPUT<<"("<<p.str_opcode;
		if(p.atom.getAtomCount()){

			if(!right_indent(true)){

				if(!separator(false)){

					set_error("syntax error: missing separator/right_indent after head");
					return	false;
				}
			}else
				OUTPUT<<" ";
		}
		return	true;
	}

	bool	Compiler::expression_tail(bool	indented,const	Class	&p,uint16	write_index,uint16	&extent_index,bool	write){	//	arity>0

		uint16	count=0;
		bool	_indented=false;
		bool	entered_pattern=p.is_pattern(_image->definition_segment);
		if(write	&&	state.pattern_lvl)	//	fill up with wildcards that will be overwritten up to ::
			for(uint16	j=write_index;j<write_index+p.atom.getAtomCount();++j)
				current_object->code[j]=Atom::Wildcard();
		std::streampos	i=in_stream->tellg();
		while(!in_stream->eof()){

			if(expression_end(indented)){

				if(state.no_arity_check){

					state.no_arity_check=false;
					return	true;
				}
				if(count==p.atom.getAtomCount())
					return	true;
				goto	return_arity_error;
			}
			if(count>=p.atom.getAtomCount())
				goto	return_arity_error;
			if(count){
				
				if(!_indented){

					if(!right_indent(true)){

						if(!separator(false)){

							set_error("syntax error: missing separator between 2 elements");
							return	false;
						}
					}else
						OUTPUT<<" ";
				}else{

					_indented=false;
					OUTPUT<<" ";
				}
			}
			if(entered_pattern	&&	count==0)	//	pattern skeleton begin
				++state.pattern_lvl;
			if(!read(p.things_to_read[count],_indented,true,write_index+count,extent_index,write)){

				set_error(" error: parsing element in expression");
				return	false;
			}
			if(entered_pattern	&&	count==0)	//	pattern skeleton end
				--state.pattern_lvl;
			++count;
		}
		return	false;
return_arity_error:
		char	buffer[255];
		std::string	s="error: got ";
		sprintf(buffer,"%d",count);
		s+=buffer;
		s+=" elements, expected ";
		sprintf(buffer,"%d",p.atom.getAtomCount());
		s+=buffer;
		set_error(s);
		return	false;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////

	bool	Compiler::expression(bool	&indented,const	ReturnType	t,uint16	write_index,uint16	&extent_index,bool	write){

		bool	lbl=false;
		std::streampos	i=in_stream->tellg();
		std::string	l;
		if(label(l)){

			lbl=true;
			OUTPUT<<"lbl_"<<l<<":";
		}
		if(!expression_begin(indented)){

			if(lbl)
				set_error(" error: label not followed by an expression");
			return	false;
		}
		Class	p;
		if(!expression_head(p,t)){

			in_stream->seekg(i);
			return	false;
		}
		if(lbl)
			local_references[l]=Reference(write_index,p);
		uint16	tail_write_index=0;
		if(write){

			current_object->code[write_index]=Atom::IPointer(extent_index);
			current_object->code[extent_index++]=p.atom;
			tail_write_index=extent_index;
			extent_index+=p.atom.getAtomCount();
		}
		if(!expression_tail(indented,p,tail_write_index,extent_index,write))
			return	false;
		OUTPUT<<")";
		return	true;
	}

	bool	Compiler::expression(bool	&indented,const	Class	&p,uint16	write_index,uint16	&extent_index,bool	write){
		
		bool	lbl=false;
		std::streampos	i=in_stream->tellg();
		std::string	l;
		if(label(l)){

			lbl=true;
			OUTPUT<<"lbl_"<<l<<":";
		}
		if(!expression_begin(indented)){

			if(lbl)
				set_error(" error: label not followed by an expression");
			return	false;
		}
		if(!expression_head(p)){

			in_stream->seekg(i);
			return	false;
		}
		if(lbl)
			local_references[l]=Reference(write_index,p);
		uint16	tail_write_index=0;
		if(write){

			current_object->code[write_index]=Atom::IPointer(extent_index);
			current_object->code[extent_index++]=p.atom;
			tail_write_index=extent_index;
			extent_index+=p.atom.getAtomCount();
		}
		if(!expression_tail(indented,p,tail_write_index,extent_index,write))
			return	false;
		OUTPUT<<")";
		return	true;
	}

	bool	Compiler::set(bool	&indented,uint16	write_index,uint16	&extent_index,bool	write){	//	[ ] illegal; use |[] instead, or [nil]

		std::streampos	i=in_stream->tellg();
		bool	lbl=false;
		std::string	l;
		if(label(l)){

			lbl=true;
			OUTPUT<<"lbl_"<<l<<":";
		}
		if(!set_begin(indented)){

			if(lbl)
				set_error(" error: label not followed by a structure");
			return	false;
		}
		if(lbl)
			local_references[l]=Reference(write_index,Class(SET));
		indent(false);
		OUTPUT<<"[";
		uint16	content_write_index=0;
		if(write){

			current_object->code[write_index]=Atom::IPointer(extent_index);
			uint16	element_count=set_element_count(indented);
			current_object->code[extent_index++]=Atom::Set(element_count);
			content_write_index=extent_index;
			extent_index+=element_count;
		}
		uint16	count=0;
		bool	_indented=false;
		while(!in_stream->eof()){

			if(set_end(indented)){

				OUTPUT<<"]";
				if(!count){

					set_error(" syntax error: use |[] for empty sets");
					return	false;
				}
				return	true;
			}
			if(count){
				
				if(!_indented){

					if(!right_indent(true)){

						if(!separator(false)){

							set_error("syntax error: missing separator between 2 elements");
							return	false;
						}
					}else
						OUTPUT<<" ";
				}else{

					_indented=false;
					OUTPUT<<" ";
				}
			}
			if(!read_any(_indented,false,NULL,content_write_index+count,extent_index,write)){
			
				set_error(" error: illegal element in set");
				return	false;
			}
			count++;
		}
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::set(bool	&indented,const	Class	&p,uint16	write_index,uint16	&extent_index,bool	write){	//	for class defs like member-name:[member-list] or !class (name[] member-list)
		
		std::streampos	i=in_stream->tellg();
		bool	lbl=false;
		std::string	l;
		if(label(l)){

			lbl=true;
			OUTPUT<<"lbl_"<<l<<":";
		}
		if(!set_begin(indented)){

			if(lbl)
				set_error(" error: label not followed by a structure");
			return	false;
		}
		if(lbl)
			local_references[l]=Reference(write_index,p);
		indent(false);
		OUTPUT<<"[";
		uint16	content_write_index=0;
		if(write){

			current_object->code[write_index]=Atom::IPointer(extent_index);
			uint16	element_count;
			if(p.atom.getDescriptor()==Atom::S_SET	&&	p.use_as!=StructureMember::SET){

				element_count=p.atom.getAtomCount();
				current_object->code[extent_index++]=p.atom;
			}else{

				element_count=set_element_count(indented);
				current_object->code[extent_index++]=Atom::Set(element_count);
			}
			content_write_index=extent_index;
			extent_index+=element_count;
		}
		uint16	count=0;
		bool	_indented=false;
		uint16	arity=0xFFFF;
		if(p.use_as==StructureMember::CLASS){	//	undefined arity for unstructured sets
		
			arity=p.atom.getAtomCount();
			if(write)	//	fill up with wildcards that will be overwritten up to ::
				for(uint16	j=content_write_index;j<content_write_index+arity;++j)
					current_object->code[j]=Atom::Wildcard();
		}
		while(!in_stream->eof()){

			if(set_end(indented)){

				if(!count){

					set_error(" syntax error: use |[] for empty sets");
					return	false;
				}else
					OUTPUT<<"]";
				if(count==arity	||	arity==0xFFFF)
					return	true;
				if(state.no_arity_check){

					state.no_arity_check=false;
					return	true;
				}
				goto	return_arity_error;
			}
			if(count>=arity)
				goto	return_arity_error;
			if(count){
				
				if(!_indented){

					if(!right_indent(true)){

						if(!separator(false)){

							set_error("syntax error: missing separator between 2 elements");
							return	false;
						}
					}else
						OUTPUT<<" ";
				}else{

					_indented=false;
					OUTPUT<<" ";
				}
			}
			bool	r;
			switch(p.use_as){
			case	StructureMember::EXPRESSION:
				r=read_expression(_indented,true,&p,content_write_index+count,extent_index,write);
				break;
			case	StructureMember::SET:
				{
				Class	_p=p;
				_p.use_as=StructureMember::CLASS;
				r=read_set(_indented,true,&_p,content_write_index+count,extent_index,write);
				break;
				}
			case	StructureMember::CLASS:
				r=read(p.things_to_read[count],_indented,true,content_write_index+count,extent_index,write);
				break;
			}
			if(!r){
						
				set_error(" error: illegal element in set");
				return	false;
			}
			count++;
		}
		in_stream->clear();
		in_stream->seekg(i);
		return	false;
return_arity_error:
		char	buffer[255];
		std::string	s="error: got ";
		sprintf(buffer,"%d",count);
		s+=buffer;
		s+=" elements, expected ";
		sprintf(buffer,"%d",arity);
		s+=buffer;
		set_error(s);
		return	false;
	}

	uint16	Compiler::set_element_count(bool	indented){	//	error checking is done in set(). This is a naive implementation: basically it parses the whole set depth-first. That's very slow and shall be replaced by a more clever design (avoiding deliving in the depths of the elements of the set).

		Output=false;
		uint16	count=0;
		State	s=save_state();
		indent(false);
		bool	_indented=false;
		uint16	unused_index=0;
		while(!in_stream->eof()){

			if(set_end(indented))
				break;
			if(count){
				
				if(!_indented){

					if(!right_indent(true)){

						if(!separator(false))
							break;
					}
				}else
					_indented=false;
			}
			if(!read_any(_indented,false,NULL,0,unused_index,false))
				break;
			count++;
		}
		in_stream->clear();
		restore_state(s);
		Output=true;
		return	count;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////

	bool	Compiler::read_any(bool	&indented,bool	enforce,const	Class	*p,uint16	write_index,uint16	&extent_index,bool	write){	//	enforce always false, p always NULL

		indented=false;
		if(read_number(indented,false,NULL,write_index,extent_index,write))
			return	true;
		if(err)
			return	false;
		if(read_timestamp(indented,false,NULL,write_index,extent_index,write))
			return	true;
		if(err)
			return	false;
		if(read_string(indented,false,NULL,write_index,extent_index,write))
			return	true;
		if(err)
			return	false;
		if(read_function(indented,false,NULL,write_index,extent_index,write))
			return	true;
		if(err)
			return	false;
		if(read_node(indented,false,NULL,write_index,extent_index,write))
			return	true;
		if(err)
			return	false;
		if(read_device(indented,false,NULL,write_index,extent_index,write))
			return	true;
		if(err)
			return	false;
		if(read_expression(indented,false,NULL,write_index,extent_index,write))
			return	true;
		if(err)
			return	false;
		if(read_set(indented,false,NULL,write_index,extent_index,write))
			return	true;
		if(write)
			set_error(" error: expecting more elements");
		return	false;
	}

	bool	Compiler::read_number(bool	&indented,bool	enforce,const	Class	*p,uint16	write_index,uint16	&extent_index,bool	write){	//	p always NULL

		if(read_nil_nb(write_index,extent_index,write))
			return	true;
		if(read_variable(write_index,extent_index,write,NUMBER))
			return	true;
		if(read_reference(write_index,extent_index,write,NUMBER))
			return	true;
		if(read_wildcard(write_index,extent_index,write))
			return	true;
		if(read_tail_wildcard(write_index,extent_index,write))
			return	true;
		
		float32	n;
		if(number(n)){

			OUTPUT<<n;
			if(write)
				current_object->code[write_index]=Atom::Float(n);
			return	true;
		}
		State	s=save_state();
		if(expression(indented,NUMBER,write_index,extent_index,write))
			return	true;
		restore_state(s);
		if(enforce){

			set_error(" error: expected a number or an expr evaluating to a number");
			return	false;
		}
		return	false;
	}

	bool	Compiler::read_boolean(bool	&indented,bool	enforce,const	Class	*p,uint16	write_index,uint16	&extent_index,bool	write){	//	p always NULL

		if(read_nil_bl(write_index,extent_index,write))
			return	true;
		if(read_variable(write_index,extent_index,write,BOOLEAN))
			return	true;
		if(read_reference(write_index,extent_index,write,BOOLEAN))
			return	true;
		if(read_wildcard(write_index,extent_index,write))
			return	true;
		if(read_tail_wildcard(write_index,extent_index,write))
			return	true;

		bool	b;
		if(boolean(b)){

			OUTPUT<<b;
			if(write)
				current_object->code[write_index]=Atom::Boolean(b);
			return	true;
		}
		State	s=save_state();
		if(expression(indented,BOOLEAN,write_index,extent_index,write))
			return	true;
		restore_state(s);
		if(enforce){

			set_error(" error: expected a Boolean or an expr evaluating to a Boolean");
			return	false;
		}
		return	false;
	}

	bool	Compiler::read_timestamp(bool	&indented,bool	enforce,const	Class	*p,uint16	write_index,uint16	&extent_index,bool	write){	//	p always NULL

		if(read_nil_us(write_index,extent_index,write))
			return	true;
		if(read_forever_us(write_index,extent_index,write))
			return	true;
		if(read_variable(write_index,extent_index,write,TIMESTAMP))
			return	true;
		if(read_reference(write_index,extent_index,write,TIMESTAMP))
			return	true;
		if(read_wildcard(write_index,extent_index,write))
			return	true;
		if(read_tail_wildcard(write_index,extent_index,write))
			return	true;

		uint64	ts;
		if(timestamp(ts)){

			OUTPUT<<ts;
			if(write){

				current_object->code[write_index]=Atom::IPointer(extent_index);
				current_object->code[extent_index++]=Atom::Timestamp();
				current_object->code[extent_index++]=ts>>32;
				current_object->code[extent_index++]=(ts	&	0x00000000FFFFFFFF);
			}
			return	true;
		}
		State	s=save_state();
		if(expression(indented,TIMESTAMP,write_index,extent_index,write))
			return	true;
		restore_state(s);
		if(enforce){

			set_error(" error: expected a timestamp or an expr evaluating to a timestamp");
			return	false;
		}
		return	false;
	}

	bool	Compiler::read_string(bool	&indented,bool	enforce,const	Class	*p,uint16	write_index,uint16	&extent_index,bool	write){	//	p always NULL

		if(read_nil_st(write_index,extent_index,write))
			return	true;
		if(read_variable(write_index,extent_index,write,STRING))
			return	true;
		if(read_reference(write_index,extent_index,write,STRING))
			return	true;
		if(read_wildcard(write_index,extent_index,write))
			return	true;
		if(read_tail_wildcard(write_index,extent_index,write))
			return	true;

		std::string	st;
		if(str(st)){

			OUTPUT<<"\""<<st<<"\"";
			if(write){

				uint16	l=(uint16)st.length();
				current_object->code[write_index]=Atom::IPointer(extent_index);
				current_object->code[extent_index++]=Atom::String(l);
				uint32	_st=0;
				int8	shift=24;
				for(uint16	i=0;i<l;++i){
					
					_st|=st[i]<<shift;
					shift-=8;
					if(shift==-8){

						current_object->code[extent_index++]=_st;
						shift=24;
					}
				}
				if(l%4)
					current_object->code[extent_index++]=_st;
			}
			return	true;
		}
		State	s=save_state();
		if(expression(indented,STRING,write_index,extent_index,write))
			return	true;
		restore_state(s);
		if(enforce){

			set_error(" error: expected a string");
			return	false;
		}
		return	false;
	}

	bool	Compiler::read_node(bool	&indented,bool	enforce,const	Class	*p,uint16	write_index,uint16	&extent_index,bool	write){	//	p always NULL

		if(read_nil_nid(write_index,extent_index,write))
			return	true;
		if(read_variable(write_index,extent_index,write,NODE_ID))
			return	true;
		if(read_reference(write_index,extent_index,write,NODE_ID))
			return	true;
		if(read_wildcard(write_index,extent_index,write))
			return	true;
		if(read_tail_wildcard(write_index,extent_index,write))
			return	true;

		std::streampos	i=in_stream->tellg();
		uint32	h;
		if(hex(h)	&&	Atom(h).getDescriptor()==Atom::NODE){

			OUTPUT<<std::hex<<h;
			if(write)
				current_object->code[write_index]=Atom::Atom(h);
			return	true;
		}
		in_stream->seekg(i);
		State	s=save_state();
		if(expression(indented,NODE_ID,write_index,extent_index,write))
			return	true;
		restore_state(s);
		if(enforce){

			set_error(" error: expected a node id");
			return	false;
		}
		return	false;
	}

	bool	Compiler::read_device(bool	&indented,bool	enforce,const	Class	*p,uint16	write_index,uint16	&extent_index,bool	write){	//	p always NULL

		if(read_nil_did(write_index,extent_index,write))
			return	true;
		if(read_variable(write_index,extent_index,write,DEVICE_ID))
			return	true;
		if(read_reference(write_index,extent_index,write,DEVICE_ID))
			return	true;
		if(read_wildcard(write_index,extent_index,write))
			return	true;
		if(read_tail_wildcard(write_index,extent_index,write))
			return	true;

		std::streampos	i=in_stream->tellg();
		uint32	h;
		if(hex(h)	&&	Atom(h).getDescriptor()==Atom::DEVICE){

			OUTPUT<<std::hex<<h;
			if(write)
				current_object->code[write_index]=Atom::Atom(h);
			return	true;
		}
		in_stream->seekg(i);
		State	s=save_state();
		if(expression(indented,DEVICE_ID,write_index,extent_index,write))
			return	true;
		restore_state(s);
		if(enforce){

			set_error(" error: expected a device id");
			return	false;
		}
		return	false;
	}

	bool	Compiler::read_function(bool	&indented,bool	enforce,const	Class	*p,uint16	write_index,uint16	&extent_index,bool	write){	//	p always NULL

		if(read_nil_fid(write_index,extent_index,write))
			return	true;
		if(read_variable(write_index,extent_index,write,FUNCTION_ID))
			return	true;
		if(read_reference(write_index,extent_index,write,FUNCTION_ID))
			return	true;
		if(read_wildcard(write_index,extent_index,write))
			return	true;
		if(read_tail_wildcard(write_index,extent_index,write))
			return	true;

		Class	_p;
		if(function(_p)){	//	TODO: _p shall be used to parse the args in the embedding expression

			OUTPUT<<_p.str_opcode;
			if(write)
				current_object->code[write_index]=_p.atom;
			return	true;
		}
		State	s=save_state();
		if(expression(indented,FUNCTION_ID,write_index,extent_index,write))
			return	true;
		restore_state(s);
		if(enforce){

			set_error(" error: expected a device function");
			return	false;
		}
		return	false;
	}

	bool	Compiler::read_expression(bool	&indented,bool	enforce,const	Class	*p,uint16	write_index,uint16	&extent_index,bool	write){

		if(read_nil(write_index,extent_index,write))
			return	true;
		if(p	&&	p->str_opcode!="expr"){

			if(read_variable(write_index,extent_index,write,*p))
				return	true;
			if(read_reference(write_index,extent_index,write,p->type))
				return	true;
		}else{

			if(read_variable(write_index,extent_index,write,Class()))
				return	true;
			if(read_reference(write_index,extent_index,write,ANY))
				return	true;
		}
		if(read_wildcard(write_index,extent_index,write))
			return	true;
		if(read_tail_wildcard(write_index,extent_index,write))
			return	true;
		
		indented=false;
		if(p	&&	p->str_opcode!="expr"){
			
			if(expression(indented,*p,write_index,extent_index,write))
				return	true;
		}else	if(expression(indented,ANY,write_index,extent_index,write))
				return	true;
		if(enforce){

			std::string	s=" error: expected an expression";
			if(p){

				s+=" of type: ";
				s+=p->str_opcode;
			}
			set_error(s);
			return	false;
		}
		return	false;
	}

	bool	Compiler::read_set(bool	&indented,bool	enforce,const	Class	*p,uint16	write_index,uint16	&extent_index,bool	write){

		if(read_nil_set(write_index,extent_index,write))
			return	true;
		if(read_variable(write_index,extent_index,write,Class(SET)))
			return	true;
		if(read_reference(write_index,extent_index,write,SET))
			return	true;
		if(read_wildcard(write_index,extent_index,write))
			return	true;
		if(read_tail_wildcard(write_index,extent_index,write))
			return	true;
		
		indented=false;
		if(p){
			
			if(set(indented,*p,write_index,extent_index,write))
				return	true;
		}else	if(set(indented,write_index,extent_index,write))
			return	true;
		if(enforce){

			set_error(" error: expected a set");
			return	false;
		}
		return	false;
	}

	bool	Compiler::read_view(bool	&indented,bool	enforce,const	Class	*p,uint16	write_index,uint16	&extent_index,bool	write){

		if(read_wildcard(write_index,extent_index,write))
			return	true;
		if(read_tail_wildcard(write_index,extent_index,write))
			return	true;

		if(state.pattern_lvl){	//	allow reading a structured set

			indented=false;
			if(set(indented,*p,write_index,extent_index,write))
				return	true;
			if(enforce){

				set_error(" error: expected a view");
				return	false;
			}
			return	false;
		}

		std::streampos	i=in_stream->tellg();
		if(match_symbol("|[]",false)){

			OUTPUT<<"|[]";
			if(write)
				current_object->code[write_index]=Atom::View();
			return	true;
		}
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::read_mks(bool	&indented,bool	enforce,const	Class	*p,uint16	write_index,uint16	&extent_index,bool	write){

		if(read_wildcard(write_index,extent_index,write))
			return	true;
		if(read_tail_wildcard(write_index,extent_index,write))
			return	true;

		std::streampos	i=in_stream->tellg();
		if(match_symbol("|[]",false)){

			OUTPUT<<"mks";
			if(write)
				current_object->code[write_index]=Atom::Mks();
			return	true;
		}
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::read_vws(bool	&indented,bool	enforce,const	Class	*p,uint16	write_index,uint16	&extent_index,bool	write){

		if(read_wildcard(write_index,extent_index,write))
			return	true;
		if(read_tail_wildcard(write_index,extent_index,write))
			return	true;
		
		std::streampos	i=in_stream->tellg();
		if(match_symbol("|[]",false)){

			OUTPUT<<"vws";
			if(write)
				current_object->code[write_index]=Atom::Vws();
			return	true;
		}
		in_stream->seekg(i);
		return	false;
	}

	////////////////////////////////////////////////////////////////////////////////////////////////

	bool	Compiler::read_nil(uint16	write_index,uint16	&extent_index,bool	write){

		if(nil()){

			OUTPUT<<"nil";
			if(write)
				current_object->code[write_index]=Atom::Nil();
			return	true;
		}
		return	false;
	}

	bool	Compiler::read_nil_set(uint16	write_index,uint16	&extent_index,bool	write){

		std::streampos	i=in_stream->tellg();
		if(match_symbol("|[]",false)){

			OUTPUT<<"|[]";
			if(write){

				current_object->code[write_index]=Atom::IPointer(extent_index);
				current_object->code[extent_index++]=Atom::Set(0);
			}
			return	true;
		}
		in_stream->seekg(i);
		return	false;
	}

	bool	Compiler::read_nil_nb(uint16	write_index,uint16	&extent_index,bool	write){

		if(nil_nb()){

			OUTPUT<<"|nb";
			if(write)
				current_object->code[write_index]=Atom::UndefinedFloat();
			return	true;
		}
		return	false;
	}

	bool	Compiler::read_nil_us(uint16	write_index,uint16	&extent_index,bool	write){

		if(nil_us()){

			OUTPUT<<"|us";
			if(write){

				current_object->code[write_index]=Atom::IPointer(extent_index);
				current_object->code[extent_index++]=Atom::UndefinedTimestamp();
				current_object->code[extent_index++]=0xFFFFFFFF;
				current_object->code[extent_index++]=0xFFFFFFFF;
			}
			return	true;
		}
		return	false;
	}

	bool	Compiler::read_forever_us(uint16	write_index,uint16	&extent_index,bool	write){

		if(forever()){

			OUTPUT<<"forever";
			if(write){

				current_object->code[write_index]=Atom::IPointer(extent_index);
				current_object->code[extent_index++]=Atom::Forever();
				current_object->code[extent_index++]=0xFFFFFFFF;
				current_object->code[extent_index++]=0xFFFFFFFF;
			}
			return	true;
		}
		return	false;
	}

	bool	Compiler::read_nil_nid(uint16	write_index,uint16	&extent_index,bool	write){

		if(nil_nid()){

			OUTPUT<<"|nid";
			if(write)
				current_object->code[write_index]=Atom::UndefinedNode();
			return	true;
		}
		return	false;
	}

	bool	Compiler::read_nil_did(uint16	write_index,uint16	&extent_index,bool	write){

		if(nil_did()){

			OUTPUT<<"|did";
			if(write)
				current_object->code[write_index]=Atom::UndefinedDevice();
			return	true;
		}
		return	false;
	}

	bool	Compiler::read_nil_fid(uint16	write_index,uint16	&extent_index,bool	write){

		if(nil_fid()){

			OUTPUT<<"|fid";
			if(write)
				current_object->code[write_index]=Atom::UndefinedDeviceFunction();
			return	true;
		}
		return	false;
	}

	bool	Compiler::read_nil_bl(uint16	write_index,uint16	&extent_index,bool	write){

		if(nil_bl()){

			OUTPUT<<"|bl";
			if(write)
				current_object->code[write_index]=Atom::UndefinedBoolean();
			return	true;
		}
		return	false;
	}

	bool	Compiler::read_nil_st(uint16	write_index,uint16	&extent_index,bool	write){

		if(nil_st()){

			OUTPUT<<"|st";
			if(write)
				current_object->code[write_index]=Atom::UndefinedString();
			return	true;
		}
		return	false;
	}

	bool	Compiler::read_variable(uint16	write_index,uint16	&extent_index,bool	write,const	Class	p){

		std::string	v;
		if(variable(v)){

			if(state.pattern_lvl){
			
				OUTPUT<<"var_"<<v<<":";
				local_references[v]=Reference(write_index,p);
				if(write)
					current_object->code[write_index]=Atom::Wildcard();	//	useless in skeleton expressions (already filled up in expression_head); usefull when the skeleton itself is a variable
				return	true;
			}else{

				set_error(" error: no variables allowed outside a pattern skeleton");
				return	false;
			}
		}
		return	false;
	}

	bool	Compiler::read_reference(uint16	write_index,uint16	&extent_index,bool	write,const	ReturnType	t){

		uint16	index;
		if((t==ANY	||	(t!=ANY	&&	current_class.type==t))	&&	this_()){

			if(write)
				current_object->code[write_index]=Atom::This();
			return	true;
		}
		if(local_reference(index,t)){

			if(write)
				current_object->code[write_index]=Atom::VLPointer(index);	//	local references are always pointing to the value array
			return	true;
		}
		if(global_reference(index,t)){	//	index is the index held by a reference pointer

			if(write){

				if(current_view_index==-1)
					_image->relocation_segment.addObjectReference(current_object->reference_set[index],current_object_index,index);
				else
					_image->relocation_segment.addViewReference(current_object->reference_set[index],current_object_index,current_view_index,index);
				current_object->code[write_index]=Atom::RPointer(index);
			}
			return	true;
		}
		std::vector<uint16>	v;
		if(this_indirection(v,t)){

			if(write){

				current_object->code[write_index]=Atom::IPointer(extent_index);
				current_object->code[extent_index++]=Atom::CPointer(v.size()+1);
				current_object->code[extent_index++]=Atom::This();
				for(uint16	i=0;i<v.size();++i)
					current_object->code[extent_index++]=Atom::IPointer(v[i]);
			}
			return	true;
		}
		if(local_indirection(v,t)){

			if(write){

				current_object->code[write_index]=Atom::IPointer(extent_index);
				current_object->code[extent_index++]=Atom::CPointer(v.size());
				current_object->code[extent_index++]=Atom::VLPointer(v[0]);
				for(uint16	i=1;i<v.size();++i)
					current_object->code[extent_index++]=Atom::IPointer(v[i]);
			}
			return	true;
		}
		if(global_indirection(v,t)){	//	v[0] is the index held by a reference pointer

			if(write){

				if(current_view_index==-1)
					_image->relocation_segment.addObjectReference(current_object->reference_set[v[0]],current_object_index,v[0]);
				else
					_image->relocation_segment.addViewReference(current_object->reference_set[v[0]],current_object_index,current_view_index,v[0]);
				current_object->code[write_index]=Atom::IPointer(extent_index);
				current_object->code[extent_index++]=Atom::CPointer(v.size());
				current_object->code[extent_index++]=Atom::RPointer(v[0]);
				for(uint16	i=1;i<v.size();++i)
					current_object->code[extent_index++]=Atom::IPointer(v[i]);
			}
			return	true;
		}
		return	false;
	}

	bool	Compiler::read_wildcard(uint16	write_index,uint16	&extent_index,bool	write){

		if(wildcard()){

			if(state.pattern_lvl){

				OUTPUT<<":";
				if(write)
					current_object->code[write_index]=Atom::Wildcard();
				return	true;
			}else{

				set_error(" error: no wildcards allowed outside a pattern skeleton");
				return	false;
			}
		}
		return	false;
	}

	bool	Compiler::read_tail_wildcard(uint16	write_index,uint16	&extent_index,bool	write){

		if(tail_wildcard()){

			if(state.pattern_lvl){

				OUTPUT<<"::";
				if(write)
					current_object->code[write_index]=Atom::TailWildcard();
				return	true;
			}else{

				set_error(" error: no wildcards allowed outside a pattern skeleton");
				return	false;
			}
		}
		return	false;
	}

}