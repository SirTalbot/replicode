#ifndef	correlator_h
#define	correlator_h

#include "../types.h"
#include "r_exec/overlay.h"

// switch to use WinEpi instead of LSTM for finding correlations
#define USE_WINEPI


extern "C" {
    r_exec::Controller REPLICODE_EXPORT *correlator(r_code::View *view);
}

////////////////////////////////////////////////////////////////////////////////

//#include <iostream>
//#include <fstream>
#include <string>
#include <map>
#include <set>
#include <algorithm>
#include <iterator>
#include <utility> // pair
#include <math.h>
#include <CoreLibrary/base.h>
#include <r_code/object.h>

#ifdef USE_WINEPI
#include "winepi.h"
#else
#include "CorrelatorCore.h"
#endif

class State:
    public core::_Object
{
public:
    double confidence;
    virtual void trace() = 0;
};

// Set of time-invariant objects.
// reads as "if objects then states hold".
class IPGMContext:
    public State
{
public:
    std::vector<P<r_code::Code> > objects;
    std::vector<P<State> > states;

    void trace()
    {
        std::cout << "IPGMContext\n";
        std::cout << "Objects\n";

        for (uint64_t i = 0; i < objects.size(); ++i) {
            objects[i]->trace();
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
    std::vector<P<r_code::Code> > left;
    std::vector<P<r_code::Code> > right;

    void trace()
    {
        std::cout << "Pattern\n";
        std::cout << "Left\n";

        for (uint64_t i = 0; i < left.size(); ++i) {
            left[i]->trace();
        }

        std::cout << "Right\n";

        for (uint64_t i = 0; i < right.size(); ++i) {
            right[i]->trace();
        }
    }
};

class CorrelatorOutput
{
public:
    std::vector<P<State> > states; // changed from vector<P<IPGMContext>>

    void trace()
    {
        std::cout << "CorrelatorOutput: " << states.size() << " states" << std::endl;

        for (uint64_t i = 0; i < states.size(); ++i) {
            states[i]->trace();
        }
    }
};


#ifdef USE_WINEPI

//typedef uint64_t timestamp_t;
//typedef P<r_code::Code> event_t;
typedef std::vector<std::pair<timestamp_t, event_t> > Episode;

class Correlator
{
private:
    Episode episode;
    size_t episode_start; // index of start of current episode
    WinEpi winepi;

public:
    Correlator();

    void take_input(r_code::View* input);
    CorrelatorOutput* get_output(bool useEntireHistory = false);

    void dump(std::ostream& out = std::cout, std::string(*oid2str)(uint64_t) = NULL) const;
};

#else // USE_WINEPI

// Lots of typedefs, not because I'm too lazy to type "std::" all the time,
// but because this makes it easier to change container types;
// also, it makes the code easier to follow
typedef std::vector<double> LSTMState; // values of LSTM output nodes; length=32 always!
typedef std::vector<LSTMState> LSTMInput;
struct Source { // an antecedent of an event
    P<r_code::Code> source; // the Replicode source object
    uint16_t deltaTime; // #timesteps this antecedent preceeds the event
};
typedef std::vector<Source> Sources;
struct JacobianRule { // target <- [source_1,..,source_n]
    double confidence;
    P<r_code::Code> target; // begin of a container of (at least) 32 floating point number
    Sources sources; // a container of Source structs
};
typedef std::vector<JacobianRule> JacobianRules;
typedef std::vector<LSTMState> JacobianSlice; // N vectors of size 32
//typedef std::vector<JacobianSlice> JacobianMatrix3D; // 3D Jacobian matrix
typedef std::list<P<r_code::Code> > SmallWorld;
typedef std::vector<SmallWorld> SmallWorlds;
typedef std::vector<P<State> > Correlations;
typedef uint64_t OID_t; // object identifier type
typedef uint64_t enc_t; // binary encoding type
typedef std::vector<enc_t> Episode;
typedef std::map<OID_t, enc_t> Table_OID2Enc;
typedef std::map<enc_t, P<r_code::Code> > Table_Enc2Obj;

class Correlator
{
private:
    Episode episode; // chronological list of binary encodings of observed objects
    Table_Enc2Obj enc2obj; // binary encoding => object (P<Code>)
    Table_OID2Enc oid2enc; // object identifier => binary encoding
    uint64_t episode_start; // index of start of current episode
    CorrelatorCore corcor; // holds and maintains the learning core

    // finds a sparse binary encoding of the provided identifier
    // sets is_new to false iff an encoding already exists
    // currently, the encoding is generated randomly,
    // but in the future we may implement a better algorithm
    enc_t encode(OID_t id, bool& is_new);

    // extracts rules of the form Target <= {(Src_1,Dt_1),..,(Src_n,Dt_n)}
    // for deltaTimes Dt_1 < .. < Dt_n
    // Note: expensive function!
    void extract_rules(JacobianRules& rules, uint64_t episode_size);

    // returns the object whose binary encoding best matches
    // the contents of the container starting at "first"
    // in case of ties, returns an unspecified best match
    // returns NULL iff enc2obj is empty
    // NOTE: assumes there are (at least) 32 elements accessible from "first"
    template<class InputIterator>
    r_code::Code* findBestMatch(InputIterator first, double& bestMatch) const;

public:
    Correlator();

    // stores the object pointed to by the provided View
    // runtime: O(log N) where N = size of episode so far
    void take_input(r_code::View* input);

    // trains the correlator on the episode so far and
    // returns the found correlations
    // useEntireHistory determines whether to use entire episode
    // or only from last call to get_output
    // Note: expensive function!
    CorrelatorOutput* get_output(bool useEntireHistory = false);

    // dump a listing of object IDs and their encodings to some output
    // the oid2str function can be used to provide a way of printing objects
    void dump(std::ostream& out = std::cout, std::string(*oid2str)(OID_t) = NULL) const;

    // magic numbers!!1
    static uint16_t NUM_BLOCKS; // #hidden blocks in LSTM network
    static uint16_t CELLS_PER_BLOCK; // #cells per block
    static uint64_t NUM_EPOCHS; // max number of epochs to train
    static double TRAIN_TIME_SEC; // time-out for training in seconds
    static double MSE_THR; // time-out threshold for mean-squared error of training
    static double LEARNING_RATE;
    static double MOMENTUM;
    static uint64_t SLICE_SIZE; // size of Jacobian matrix slices
    static double OBJECT_THR; // threshold for matching LSTM output to a Replicode object
    static double RULE_THR; // threshold for Jacobian rule confidence
};

#endif // USE_WINEPI

#endif
