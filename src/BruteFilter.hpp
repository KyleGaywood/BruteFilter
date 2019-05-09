/*
ID : vdvrack_brutefilter
	vendor : KyleGaywood
	version : 0.6.0
	name : VCVRack BruteFilter
	description : multimode filter for VCVRack



*/


//VCVRack Libraries
#include "rack.hpp"
#include "dsp\filter.hpp"


/* JUCE breaks the system
#define JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED
#include "juce_dsp\juce_dsp.h"
*/

using namespace rack;

// Forward-declare the Plugin, defined in Template.cpp
extern Plugin *plugin;

// Forward-declare each Model, defined in each module source file
extern Model *modelBruteFilter;
