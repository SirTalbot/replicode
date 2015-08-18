#ifndef	correlator_h
#define	correlator_h

#include <../../CoreLibrary/trunk/CoreLibrary/base.h>
#include <../r_code/object.h>


class State:
    public core::_Object
{
public:
    virtual void trace() = 0;
};

// Set of time-invariant views.
// reads as "if views then states hold".
class IPGMContext:
    public State
{
public:
    std::vector<P<r_code::View> > views;
    std::vector<P<State> > states;

    void trace()
    {
        std::cout << "IPGMContext\n";
        std::cout << "Views\n";

        for (uint64_t i = 0; i < views.size(); ++i) {
            views[i]->object->trace();
        }

        std::cout << "States\n";

        for (uint64_t i = 0; i < states.size(); ++i) {
            states[i]->trace();
        }
    }
};

// Pattern that hold under some context.
// Read as "left implies right".
class Pattern:
    public State
{
public:
    std::vector<P<r_code::View> > left;
    std::vector<P<r_code::View> > right;

    void trace()
    {
        std::cout << "Pattern\n";
        std::cout << "Left\n";

        for (uint64_t i = 0; i < left.size(); ++i) {
            left[i]->object->trace();
        }

        std::cout << "Right\n";

        for (uint64_t i = 0; i < right.size(); ++i) {
            right[i]->object->trace();
        }
    }
};

class CorrelatorOutput
{
public:
    std::vector<P<IPGMContext> > contexts;

    void trace()
    {
        std::cout << "CorrelatorOutput\n";

        for (uint64_t i = 0; i < contexts.size(); ++i) {
            contexts[i]->trace();
        }
    }
};

class Correlator
{
private:
    std::vector<uint64> episode;
public:
    void take_input(r_code::View *input)
    {
        episode.push_back(input->code(VIEW_OID).atom);
    }

    CorrelatorOutput *get_output()
    {
        CorrelatorOutput *c = new CorrelatorOutput();
        // TODO: fill c.
        return c;
    }
};


#endif