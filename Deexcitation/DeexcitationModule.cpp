#include "Deexcitation/DeexcitationModule.h"

extern "C" cola::VModule* LoadCOLAModule() { return new cola::DeexcitationModule(); }
