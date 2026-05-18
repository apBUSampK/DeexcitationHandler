//
// Created by Artem Novikov on 19.07.2023.
//

#include "Deexcitation/handler/ExcitationHandler.h"
#include "Deexcitation/handler/FermiBreakUpWrapper.h"
#include "FermiBreakUp/FermiBreakUp.h"
#include "FermiBreakUp/Splitter.h"
#include "FermiBreakUp/util/Cache.h"
#include "FermiBreakUp/util/DataTypes.h"
#include "FermiBreakUp/util/Logger.h"
#include "FermiBreakUp/util/Randomizer.h"

#include <gtest/gtest.h>

#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace {
  std::unique_ptr<fbu::FermiBreakUp::SplitCache> GetCache(const std::string_view name) {
    if (name == "simple") {
      return std::make_unique<fbu::SimpleCache<fbu::NucleiData, fbu::FragmentSplits>>();
    } else if (name == "lfu") {
      return std::make_unique<fbu::LFUCache<fbu::NucleiData, fbu::FragmentSplits>>(10);
    }

    std::stringstream ss;
    ss << "unknown cache name: " << name;
    throw std::runtime_error(ss.str());
    return nullptr;
  }

  class ConfigurationsFixture : public ::testing::TestWithParam<std::string_view> {
   protected:
    std::string_view CacheType;
  };

  INSTANTIATE_TEST_SUITE_P(ConservationTests, ConfigurationsFixture,
                           ::testing::Values("lfu"
                                             // "simple"
                                             ));

}  // namespace

TEST_P(ConfigurationsFixture, MassAndChargeConservation) {
  auto model = ExcitationHandler();
  auto fbuModel = std::make_unique<FermiBreakUpWrapper>(fbu::FermiBreakUp(GetCache(ConfigurationsFixture::GetParam())));
  fbuModel->Initialise();
  model.SetFermiBreakUp(std::move(fbuModel));
  const int seed = 1;
  srand(seed);
  const size_t tries = 10;
  const size_t runs = 1e3;
  const int maxNuclei = 100;
  fbu::Logger::GlobalLevel = fbu::LogLevel::WARN;

  for (size_t t = 0; t < tries; ++t) {
    const G4int mass = rand() % maxNuclei + 1;
    const G4int charge = rand() % (mass + 1);
    const G4double energyPerNuclei = (rand() % 10 + 1);
    const G4double energy = energyPerNuclei * CLHEP::MeV * mass;
    const auto particle =
        G4Fragment(mass, charge, G4LorentzVector(0, 0, 0, G4NucleiProperties::GetNuclearMass(mass, charge) + energy));
    G4int chargeTotal = 0;
    for (size_t i = 0; i < runs; ++i) {
      G4int massTotal = 0;
      auto fragments = model.BreakItUp(particle);
      for (const auto& fragment : fragments) {
        massTotal += fragment.GetDefinition()->GetAtomicMass();
        chargeTotal += fragment.GetDefinition()->GetAtomicNumber();
      }

      ASSERT_EQ(massTotal, mass) << "violates mass conservation: " << mass << ' ' << charge << ' ' << energyPerNuclei;
    }

    // test mean, because of multifragmentation model
    ASSERT_NEAR(G4double(chargeTotal) / runs, charge, 2 * charge / std::sqrt(runs))
        << "violates charge conservation: " << mass << ' ' << charge << ' ' << energyPerNuclei;
  }
}

// Is doesn't work because of multi-fragmentation model *(
// TEST_P(ConfigurationsFixture, Vector4Conservation) {
//   auto model = ExcitationHandler();
//   auto fbuModel =
//   std::make_unique<FermiBreakUpWrapper>(fbu::FermiBreakUp(GetCache(ConfigurationsFixture::GetParam())));
//   fbuModel->Initialise();
//   model.SetFermiBreakUp(std::move(fbuModel));
//   const int seed = 7;
//   srand(seed);
//   const size_t tries = 10;
//   const size_t runs = 1e3;
//   const int maxNuclei = 200;
//   for (size_t t = 0; t < tries; ++t) {
//     const G4int mass = rand() % maxNuclei + 1;
//     const G4int charge = rand() % (mass + 1);
//     const G4double energy = (rand() % 10 + 1) * CLHEP::MeV * mass;
//     const auto vec = fbu::Randomizer::IsotropicVector() * (rand() % 10 + 1) * CLHEP::MeV * mass;
//     const auto totalMomentum =
//         G4LorentzVector(vec, std::sqrt(std::pow(G4NucleiProperties::GetNuclearMass(mass, charge) + energy, 2) +
//             vec.mag2()));
//     const auto particle = G4Fragment(mass, charge, totalMomentum);

//     G4ThreeVector sumMomentum(0, 0, 0);
//     G4double sumEnergy = 0;
//     for (size_t i = 0; i < runs; ++i) {
//       auto fragments = model.BreakItUp(particle);
//       for (const auto& fragment : fragments) {
//         sumMomentum += fragment.GetMomentum();
//         sumEnergy += fragment.GetTotalEnergy();
//       }
//     }

//     ASSERT_NEAR(sumMomentum.x() / runs, totalMomentum.x(), std::max(1e-3, std::abs(totalMomentum.x()) /
//     std::sqrt(runs))); ASSERT_NEAR(sumMomentum.y() / runs, totalMomentum.y(), std::max(1e-3,
//     std::abs(totalMomentum.y()) / std::sqrt(runs))); ASSERT_NEAR(sumMomentum.z() / runs, totalMomentum.z(),
//     std::max(1e-3, std::abs(totalMomentum.z()) / std::sqrt(runs))); ASSERT_NEAR(sumEnergy / runs, totalMomentum.e(),
//     std::max(1e-3, std::abs(totalMomentum.e()) / std::sqrt(runs)));
//   }
// }
