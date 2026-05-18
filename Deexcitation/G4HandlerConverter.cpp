// NOLINTNEXTLINE(misc-include-cleaner)
#include "Deexcitation/handler/ExcitationHandler.h"

#include "Deexcitation/G4HandlerConverter.h"

namespace {

  G4Fragment ColaToG4(const cola::Particle& particle) {
    const auto [A, Z] = particle.GetAZ();

    return G4Fragment(
        static_cast<G4int>(A),
        static_cast<G4int>(Z),
        G4LorentzVector(particle.momentum.x, particle.momentum.y, particle.momentum.z, particle.momentum.e));
  }

  cola::Particle G4ToCola(const G4ReactionProduct& fragment) {
    return cola::Particle{
        .position = cola::LorentzVector{.e = 0., .x = 0., .y = 0., .z = 0.},
        .momentum =
            cola::LorentzVector{
                .e = fragment.GetTotalEnergy(),
                .x = fragment.GetMomentum().x(),
                .y = fragment.GetMomentum().y(),
                .z = fragment.GetMomentum().z(),
            },
        .pdgCode = cola::AZToPdg({static_cast<uint16_t>(fragment.GetDefinition()->GetAtomicMass()),
                                  static_cast<uint16_t>(fragment.GetDefinition()->GetAtomicNumber())}),
        .pClass = cola::ParticleClass::kProduced,
    };
  }

}  // namespace

namespace cola {

  G4HandlerConverter::G4HandlerConverter(std::unique_ptr<ExcitationHandler>&& model) : model_(std::move(model)) {}

  std::unique_ptr<EventData> G4HandlerConverter::operator()(std::unique_ptr<EventData>&& data) {
    EventParticles results;
    for (const auto& particle : data->particles) {
      const auto pClass = particle.pClass;
      if (pClass == ParticleClass::kSpectatorA || pClass == ParticleClass::kSpectatorB) {
        const auto modelResult = model_->BreakItUp(ColaToG4(particle));

        for (const auto& fragment : modelResult) {
          results.emplace_back(G4ToCola(fragment));
          results.back().pClass = pClass;
        }
      } else {
        results.push_back(particle);
      }
    }

    data->particles = std::move(results);
    return std::move(data);
  }

}  // namespace cola
