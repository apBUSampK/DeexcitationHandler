//
// Created by Artem Novikov on 17.05.2023.
//

#include "ExcitationHandler.h"

#include <CLHEP/Units/PhysicalConstants.h>
#include <G4BaryonConstructor.hh>
#include <G4BosonConstructor.hh>
#include <G4Electron.hh>
#include <G4Evaporation.hh>
#include <G4FermiBreakUpAN.hh>
#include <G4IonConstructor.hh>
#include <G4Ions.hh>
#include <G4LeptonConstructor.hh>
#include <G4LorentzVector.hh>
#include <G4MesonConstructor.hh>
#include <G4NistManager.hh>
#include <G4ParticleTable.hh>
#include <G4ParticleTypes.hh>
#include <G4PhotonEvaporation.hh>
#include <G4ProcessManager.hh>
#include <G4RunManager.hh>
#include <G4StatMF.hh>
#include <G4StateManager.hh>

#include <string>

namespace {
  constexpr size_t EvaporationIterationThreshold = 1e3;

  static const std::string ErrorNoModel = "no model was applied, check conditions";

  class FermiBreakUpWrapper : public G4FermiBreakUpAN {
   public:
    using G4FermiBreakUpAN::G4FermiBreakUpAN;

    void BreakFragment(G4FragmentVector* results, G4Fragment* theNucleus) override {
      auto deletableFragment = new G4Fragment(*theNucleus);
      auto oldSize = results->size();
      G4FermiBreakUpAN::BreakFragment(results, deletableFragment);
      if (oldSize == results->size()) {
        // NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDelete)
        delete deletableFragment;
      }
    }
  };

  class DataCleaner {
   public:
    DataCleaner(G4FragmentVector& results) : results_(results) {}

    ~DataCleaner() {
      for (auto ptr : results_) {
        delete ptr;
      }
    }

   private:
    G4FragmentVector& results_;
  };

  void ClearSingularResults(G4FragmentVector& results, G4Fragment* initial) {
    for (auto fragmentPtr : results) {
      if (fragmentPtr != initial) {
        delete fragmentPtr;
      }
    }
  }

  constexpr G4int HashParticle(G4int A, G4int Z) { return A * 1000 + Z; }

  G4ParticleDefinition* SpecialParticleDefinition(const G4Fragment& fragment) {
    switch (HashParticle(fragment.GetA_asInt(), fragment.GetZ_asInt())) {
      case HashParticle(0, 0): {
        return G4Gamma::GammaDefinition();
      }

      case HashParticle(0, -1): {
        return G4Electron::ElectronDefinition();
      }

      case HashParticle(-1, 1): {
        return G4PionPlus::PionPlus();
      }

      case HashParticle(-1, -1): {
        return G4PionMinus::PionMinus();
      }

      case HashParticle(-1, 0): {
        return G4PionZero::PionZero();
      }

      case HashParticle(1, 0): {
        return G4Neutron::NeutronDefinition();
      }

      case HashParticle(1, 1): {
        return G4Proton::ProtonDefinition();
      }

      case HashParticle(2, 1): {
        return G4Deuteron::DeuteronDefinition();
      }

      case HashParticle(3, 1): {
        return G4Triton::TritonDefinition();
      }

      case HashParticle(3, 2): {
        return G4He3::He3Definition();
      }

      case HashParticle(4, 2): {
        return G4Alpha::AlphaDefinition();
      }
    }

    return nullptr;
  }

  void EvaporationError(const G4Fragment& fragment, const G4Fragment& currentFragment, size_t iter) {
    G4ExceptionDescription ed;
    ed << "Infinite loop in the de-excitation module: " << iter << " iterations \n"
       << "      Initial fragment: \n"
       << fragment << "\n      Current fragment: \n"
       << currentFragment;
    G4Exception("ExcitationHandler::BreakItUp", "", FatalException, ed, "Stop execution");
  }
}  // namespace

ExcitationHandler::ExcitationHandler()
    : multiFragmentationModel_(DefaultMultiFragmentation())
    , fermiBreakUpModel_(DefaultFermiBreakUp())
    , evaporationModel_(DefaultEvaporation())
    , photonEvaporationModel_(DefaultPhotonEvaporation())
    , neutronDecayModel_(DefaultNeutronDecay())
    , multiFragmentationCondition_(DefaultMultiFragmentationCondition())
    , fermiCondition_(DefaultFermiBreakUpCondition())
    , photonEvaporationCondition_(DefaultPhotonEvaporationCondition())
    , evaporationCondition_(DefaultEvaporationCondition())
    , neutronDecayCondition_(DefaultNeutronDecayCondition()) {
  evaporationModel_->SetFermiBreakUp(fermiBreakUpModel_.get());
  evaporationModel_->SetPhotonEvaporation(photonEvaporationModel_.get());

  G4BosonConstructor pCBos;
  pCBos.ConstructParticle();

  G4LeptonConstructor pCLept;
  pCLept.ConstructParticle();

  G4MesonConstructor pCMes;
  pCMes.ConstructParticle();

  G4BaryonConstructor pCBar;
  pCBar.ConstructParticle();

  G4IonConstructor pCIon;
  pCIon.ConstructParticle();

  G4GenericIon* gion = G4GenericIon::GenericIon();
  auto manager = new G4ProcessManager(gion);
  manager->SetVerboseLevel(0);
  gion->SetProcessManager(manager);

  G4StateManager::GetStateManager()->SetNewState(G4State_Init);  // To let create ions
  G4ParticleTable* particleTable = G4ParticleTable::GetParticleTable();
  G4IonTable* ionTable = particleTable->GetIonTable();
  particleTable->SetReadiness();
  ionTable->CreateAllIon();
  ionTable->CreateAllIsomer();
}

ExcitationHandler::~ExcitationHandler() {
  photonEvaporationModel_.release();  // otherwise, SegFault in evaporation destructor
}

std::vector<G4ReactionProduct> ExcitationHandler::BreakItUp(const G4Fragment& fragment) {
  auto nist = G4NistManager::Instance();
  G4FragmentVector results;
  const auto cleaner = DataCleaner(results);
  FragmentQueue evaporationQueue;
  FragmentQueue photonEvaporationQueue;

  // In case A <= 1 the fragment will not perform any nucleon emission
  auto initialFragmentPtr = std::make_unique<G4Fragment>(fragment);
  if (neutronDecayCondition_(fragment)) {
    ApplyPureNeutronDecay(std::move(initialFragmentPtr), results);
  } else if (IsStable(fragment, nist)) {
    results.push_back(initialFragmentPtr.release());
  } else {
    if (multiFragmentationCondition_(fragment)) {
      ApplyMultiFragmentation(std::move(initialFragmentPtr), results, evaporationQueue);
    } else {
      evaporationQueue.emplace(std::move(initialFragmentPtr));
    }

    for (size_t iterationCount = 0; !evaporationQueue.empty(); ++iterationCount) {
      auto fragmentPtr = std::move(evaporationQueue.front());
      evaporationQueue.pop();

      // infinite loop check
      if (iterationCount == EvaporationIterationThreshold) {
        EvaporationError(fragment, *fragmentPtr, iterationCount);
        return {};
        // process is dead
      }

      // NeutronDecay part
      if (neutronDecayCondition_(*fragmentPtr)) {
        ApplyPureNeutronDecay(std::move(fragmentPtr), results);
        continue;
      }

      // FermiBreakUp part
      if (fermiCondition_(*fragmentPtr)) {
        ApplyFermiBreakUp(std::move(fragmentPtr), results, photonEvaporationQueue);
        continue;
      }

      // Evaporation part
      if (evaporationCondition_(*fragmentPtr)) {
        ApplyEvaporation(std::move(fragmentPtr), results, evaporationQueue);
        continue;
      }

      throw std::runtime_error(ErrorNoModel);
    }

    // Photon Evaporation part
    while (!photonEvaporationQueue.empty()) {
      auto fragmentPtr = std::move(photonEvaporationQueue.front());
      photonEvaporationQueue.pop();

      if (photonEvaporationCondition_(*fragmentPtr)) {
        ApplyPhotonEvaporation(std::move(fragmentPtr), results);
        continue;
      }

      throw std::runtime_error(ErrorNoModel);
    }
  }

  // Geant4 randomly throws neutron fragments, here's a bandaid for it

  G4FragmentVector buffer;
  for (auto it = results.begin(); it != results.end();) {
    if (neutronDecayCondition_(**it)) {
      buffer.push_back(*it);
      it = results.erase(it);
    } else {
      ++it;
    }
  }

  for (auto& to_decay : buffer) {
    ApplyPureNeutronDecay(std::unique_ptr<G4Fragment>(to_decay), results);
  }

  auto reactionProducts = ConvertResults(results);

  return reactionProducts;
}

void ExcitationHandler::NeutronDecay::BreakFragment(G4FragmentVector& results, const G4Fragment& fragment) {
  if (fragment.GetZ_asInt() != 0) {
    throw std::runtime_error("only Z = 0 particles can be decayed by NeutronDecay, but got: A = " +
                             std::to_string(fragment.GetA_asInt()) + ", Z = " + std::to_string(fragment.GetZ_asInt()));
  }

  if (fragment.GetA_asInt() == 1) {
    return;
  }

  auto masses = std::vector<G4double>(fragment.GetA_asInt(), CLHEP::neutron_mass_c2);
  auto momentum = fragment.GetMomentum();
  const auto diff = momentum.m() - CLHEP::neutron_mass_c2 * fragment.GetA_asInt();
  if (diff < 10. * CLHEP::eV) {
    momentum.setE(momentum.e() + 10. * CLHEP::eV - diff);
  }

  const auto particlesMomentum = phaseSpaceDecay_.CalculateDecay(momentum, masses);
  if (particlesMomentum.empty()) {
    std::stringstream ss;
    ss << "NeutronDecay is unable to break particle with "
       << "A = " << fragment.GetA_asInt() << ", Z = " << fragment.GetZ_asInt() << ", P = " << momentum;
    throw std::runtime_error(ss.str());
  }

  for (const auto& particleMomentum : particlesMomentum) {
    results.emplace_back(new G4Fragment(1, 0, particleMomentum));
  }
}

std::unique_ptr<G4VMultiFragmentation> ExcitationHandler::DefaultMultiFragmentation() {
  return std::make_unique<G4StatMF>();
}

std::unique_ptr<G4VFermiBreakUp> ExcitationHandler::DefaultFermiBreakUp() {
  auto model = std::make_unique<FermiBreakUpWrapper>();
  model->Initialise();
  return model;
}

std::unique_ptr<G4VEvaporation> ExcitationHandler::DefaultEvaporation() {
  auto evaporation = std::make_unique<G4Evaporation>();
  return evaporation;
}

std::unique_ptr<G4VEvaporationChannel> ExcitationHandler::DefaultPhotonEvaporation() {
  return std::make_unique<G4PhotonEvaporation>();
}

std::unique_ptr<ExcitationHandler::NeutronDecay> ExcitationHandler::DefaultNeutronDecay() {
  return std::make_unique<NeutronDecay>();
}

ExcitationHandler::Condition ExcitationHandler::DefaultMultiFragmentationCondition() {
  return [](const G4Fragment& fragment) -> bool {
    constexpr G4int maxAtomicMass = 19;
    constexpr G4int maxCharge = 9;
    constexpr G4double lowerBoundTransitionMF = 3 * CLHEP::MeV;
    constexpr G4double upperBoundTransitionMF = 5 * CLHEP::MeV;

    const auto atomicMass = fragment.GetA_asInt();
    const auto charge = fragment.GetZ_asInt();

    if (atomicMass < maxAtomicMass && charge < maxCharge) {
      return false;
    }

    const auto exitationEnergy = fragment.GetExcitationEnergy();

    const auto scale = 1. / (2. * (upperBoundTransitionMF - lowerBoundTransitionMF));
    const auto energyOffset = (upperBoundTransitionMF + lowerBoundTransitionMF) / 2.;
    const auto transitionProb = 0.5 * std::tanh((exitationEnergy / atomicMass - energyOffset) / scale) + 0.5;

    const auto random = G4RandFlat::shoot();

    if (exitationEnergy < lowerBoundTransitionMF * atomicMass) {
      return false;
    }

    if (random < transitionProb && exitationEnergy < upperBoundTransitionMF * atomicMass) {
      return true;
    }

    if (random > transitionProb && exitationEnergy < upperBoundTransitionMF * atomicMass) {
      return false;
    }

    if (exitationEnergy > upperBoundTransitionMF * atomicMass) {
      return true;
    }

    return false;
  };
}

ExcitationHandler::Condition ExcitationHandler::DefaultFermiBreakUpCondition() {
  return
      [](const G4Fragment& fragment) -> bool { return fragment.GetZ_asInt() < MAX_Z && fragment.GetA_asInt() < MAX_A; };
}

ExcitationHandler::Condition ExcitationHandler::DefaultEvaporationCondition() {
  return [](const G4Fragment&) { return true; };
}

ExcitationHandler::Condition ExcitationHandler::DefaultPhotonEvaporationCondition() {
  return [](const G4Fragment&) { return true; };
}

ExcitationHandler::Condition ExcitationHandler::DefaultNeutronDecayCondition() {
  return [](const G4Fragment& fragment) { return fragment.GetA_asInt() > 1 && fragment.GetZ_asInt() == 0; };
}

bool ExcitationHandler::IsGroundState(const G4Fragment& fragment) const {
  return fragment.GetExcitationEnergy() < stableThreshold_;
}

bool ExcitationHandler::IsStable(const G4Fragment& fragment, const G4NistManager* nist) const {
  return fragment.GetA_asInt() <= 1 ||
         (IsGroundState(fragment) && nist->GetIsotopeAbundance(fragment.GetZ_asInt(), fragment.GetA_asInt()) > 0);
}

void ExcitationHandler::ApplyMultiFragmentation(std::unique_ptr<G4Fragment>&& fragment, G4FragmentVector& results,
                                                FragmentQueue& nextStage) {
  auto fragments = std::unique_ptr<G4FragmentVector>(multiFragmentationModel_->BreakItUp(*fragment));
  if (fragments == nullptr || fragments->size() <= 1) {
    ClearSingularResults(*fragments, fragment.get());
    nextStage.emplace(fragment.release());
    return;
  }

  GroupFragments(std::move(*fragments), results, nextStage);
}

void ExcitationHandler::ApplyFermiBreakUp(std::unique_ptr<G4Fragment>&& fragment, G4FragmentVector& results,
                                          FragmentQueue& nextStage) {
  G4FragmentVector fragments;
  fermiBreakUpModel_->BreakFragment(&fragments, fragment.get());

  if (fragments.size() <= 1) {
    ClearSingularResults(fragments, fragment.get());
    nextStage.emplace(fragment.release());
    return;
  }

  GroupFragments(std::move(fragments), results, nextStage);
}

void ExcitationHandler::ApplyEvaporation(std::unique_ptr<G4Fragment>&& fragment, G4FragmentVector& results,
                                         FragmentQueue& nextStage) {
  G4FragmentVector fragments;
  evaporationModel_->BreakFragment(&fragments, fragment.get());

  if (fragments.size() <= 1) {
    ClearSingularResults(fragments, fragment.get());
    results.emplace_back(fragment.release());
    return;
  }

  GroupFragments(std::move(fragments), results, nextStage);
}

void ExcitationHandler::ApplyPhotonEvaporation(std::unique_ptr<G4Fragment>&& fragment, G4FragmentVector& results) {
  // photon de-excitation only for hot fragments
  if (!IsGroundState(*fragment)) {
    G4FragmentVector fragments;

    photonEvaporationModel_->BreakUpChain(&fragments, fragment.get());

    for (auto fragmentPtr : fragments) {
      results.emplace_back(fragmentPtr);
    }
  }

  // primary fragment is kept
  results.emplace_back(fragment.release());
}

void ExcitationHandler::ApplyPureNeutronDecay(std::unique_ptr<G4Fragment>&& fragment, G4FragmentVector& results) {
  size_t oldSize = results.size();
  neutronDecayModel_->BreakFragment(results, *fragment);

  if (oldSize == results.size()) {
    results.emplace_back(fragment.release());
  }
}

void ExcitationHandler::GroupFragments(G4FragmentVector&& fragments, G4FragmentVector& results,
                                       FragmentQueue& nextStage) {
  auto nist = G4NistManager::Instance();

  for (auto fragmentPtr : fragments) {
    // gamma, p, n or stable nuclei
    if (neutronDecayCondition_(*fragmentPtr)) {
      ApplyPureNeutronDecay(std::unique_ptr<G4Fragment>(fragmentPtr), results);
    } else if (IsStable(*fragmentPtr, nist)) {
      results.emplace_back(fragmentPtr);
    } else {
      nextStage.emplace(fragmentPtr);
    }
  }
}

std::vector<G4ReactionProduct> ExcitationHandler::ConvertResults(const G4FragmentVector& results) {
  std::vector<G4ReactionProduct> reactionProducts;
  reactionProducts.reserve(results.size());
  auto ionTable = G4ParticleTable::GetParticleTable()->GetIonTable();

  for (const auto& fragmentPtr : results) {
    auto fragmentDefinition = SpecialParticleDefinition(*fragmentPtr);
    if (fragmentDefinition == nullptr) {
      auto excitationEnergy = fragmentPtr->GetExcitationEnergy();
      auto level = fragmentPtr->GetFloatingLevelNumber();
      if (IsGroundState(*fragmentPtr)) {
        excitationEnergy = 0;
        level = 0;
      }
      fragmentDefinition = ionTable->GetIon(fragmentPtr->GetZ_asInt(), fragmentPtr->GetA_asInt(), excitationEnergy,
                                            G4Ions::FloatLevelBase(level));
    }
    // fragment wasn't found, ground state is created
    if (fragmentDefinition == nullptr) {
      fragmentDefinition = ionTable->GetIon(fragmentPtr->GetZ_asInt(), fragmentPtr->GetA_asInt(), 0, noFloat, 0);
      if (fragmentDefinition == nullptr) {
        throw std::runtime_error("ion table isn't created");
      }
      G4double ionMass = fragmentDefinition->GetPDGMass();
      if (fragmentPtr->GetMomentum().e() <= ionMass) {
        fragmentPtr->SetMomentum(G4LorentzVector(ionMass));
      } else {
        auto momentum = fragmentPtr->GetMomentum();
        G4double momentumModulus = std::sqrt((momentum.e() - ionMass) * (momentum.e() + ionMass));
        momentum.setVect(momentum.vect().unit() * momentumModulus);
        fragmentPtr->SetMomentum(momentum);
      }
    }

    reactionProducts.emplace_back(fragmentDefinition);
    reactionProducts.back().SetMomentum(fragmentPtr->GetMomentum().vect());
    reactionProducts.back().SetTotalEnergy((fragmentPtr->GetMomentum()).e());
    reactionProducts.back().SetFormationTime(fragmentPtr->GetCreationTime());
  }

  return reactionProducts;
}
