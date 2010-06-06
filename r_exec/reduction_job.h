#ifndef	reduction_job_h
#define	reduction_job_h

#include	"pgm_overlay.h"
#include	"object.h"


namespace	r_exec{

	class	ReductionJob{
	public:
		r_code::P<View>		input;
		r_code::P<Overlay>	overlay;
		uint64				deadline;	//	for ipgm with inputs.
		ReductionJob();
		ReductionJob(View	*input,Overlay	*overlay,uint64	deadline);
	};
}


#endif