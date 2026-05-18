#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include <G4FermiBreakUpAN.hh>
#include <Randomize.hh>

#include "Deexcitation/G4HandlerConverter.h"
#include "Deexcitation/G4HandlerFactory.h"
#include "Deexcitation/handler/ExcitationHandler.h"

namespace {

  bool EndsWith(const std::string& str, const std::string& suffix) {
    return suffix.size() <= str.size()
        && str.compare(str.size() - suffix.size(), suffix.size(), suffix) == 0;
  }

  double StodWithFactor(const std::string& value) {
    const double num = std::stod(value);
    if (EndsWith(value, "eV")) {
      return num * CLHEP::eV;
    }
    if (EndsWith(value, "MeV")) {
      return num * CLHEP::MeV;
    }
    if (EndsWith(value, "keV")) {
      return num * CLHEP::keV;
    }
    if (EndsWith(value, "GeV")) {
      return num * CLHEP::GeV;
    }
    return num;
  }

  struct Config {
    explicit Config(const std::unordered_map<std::string, std::string>& params) {
      if (const auto it = params.find("A"); it != params.end()) {
        A = std::stoi(it->second);
      }
      if (const auto it = params.find("Z"); it != params.end()) {
        Z = std::stoi(it->second);
      }
      if (const auto it = params.find("lowerMfThreshold"); it != params.end()) {
        lowerMfThreshold = StodWithFactor(it->second);
      }
      if (const auto it = params.find("upperMfThreshold"); it != params.end()) {
        upperMfThreshold = StodWithFactor(it->second);
      }
      if (const auto it = params.find("stableThreshold"); it != params.end()) {
        stableThreshold = StodWithFactor(it->second);
      }
    }

    std::optional<int> A;
    std::optional<int> Z;
    std::optional<double> stableThreshold;
    std::optional<double> lowerMfThreshold;
    std::optional<double> upperMfThreshold;
  };

}  // namespace

namespace cola {

  std::unique_ptr<VFilter> G4HandlerFactory::Create(const std::unordered_map<std::string, std::string>& metaData) {
    const Config config(metaData);

    auto model = std::make_unique<ExcitationHandler>();

    if (config.stableThreshold.has_value()) {
      model->SetStableThreshold(*config.stableThreshold);
    }

    model->SetFermiBreakUpCondition(
        [maxA = config.A.value_or(static_cast<int>(MAX_A)), maxZ = config.Z.value_or(static_cast<int>(MAX_Z))](
            const G4Fragment& fragment) {
          return fragment.GetZ_asInt() < maxZ && fragment.GetA_asInt() < maxA;
        });

    model->SetMultiFragmentationCondition(
        [maxA = config.A.value_or(static_cast<int>(MAX_A)), maxZ = config.Z.value_or(static_cast<int>(MAX_Z)),
         lowerBoundTransitionMF = config.lowerMfThreshold.value_or(3 * CLHEP::MeV),
         upperBoundTransitionMF = config.upperMfThreshold.value_or(5 * CLHEP::MeV)](const G4Fragment& fragment) {
          const auto A = fragment.GetA_asInt();
          const auto Z = fragment.GetZ_asInt();
          const auto Ex = fragment.GetExcitationEnergy();
          if (A < maxA && Z < maxZ) {
            return false;
          }
          const G4double aE = 1 / (2. * (upperBoundTransitionMF - lowerBoundTransitionMF));
          const G4double E0 = (upperBoundTransitionMF + lowerBoundTransitionMF) / 2.;
          const G4double w = G4RandFlat::shoot();
          const G4double transF = 0.5 * std::tanh((Ex / A - E0) / aE) + 0.5;

          if (Ex < lowerBoundTransitionMF * A) {
            return false;
          }
          if (w < transF && Ex < upperBoundTransitionMF * A) {
            return true;
          }
          if (w > transF && Ex < upperBoundTransitionMF * A) {
            return false;
          }
          if (Ex > upperBoundTransitionMF * A) {
            return true;
          }
          return false;
        });

    return std::make_unique<G4HandlerConverter>(std::move(model));
  }

}  // namespace cola
