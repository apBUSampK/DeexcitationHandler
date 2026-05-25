#include "Deexcitation/DeexcitationModule.h"

#include <COLA.hh>

extern "C" cola::VModule* LoadCOLAModule() { return new cola::DeexcitationModule(); }
