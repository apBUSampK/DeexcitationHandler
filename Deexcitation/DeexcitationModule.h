#ifndef DEEXCITATION_DEEXCITATIONMODULE_H_
#define DEEXCITATION_DEEXCITATIONMODULE_H_

#include <COLA.hh>

#include "Deexcitation/G4HandlerFactory.h"

namespace cola {

  using DeexcitationModule = GenericModule<G4HandlerFactory>;

}  // namespace cola

#endif  // DEEXCITATION_DEEXCITATIONMODULE_H_
