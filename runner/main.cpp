#include "Deexcitation/G4HandlerFactory.h"

#include <CLHEP/Units/PhysicalConstants.h>
#include <COLA.hh>

#include <cmath>

class TestGenerator final : public cola::VGenerator {
 public:
  explicit TestGenerator(cola::EventParticles particles) : particles_(std::move(particles)) {}

  std::unique_ptr<cola::EventData> operator()() override {
    auto data = std::make_unique<cola::EventData>();
    data->particles = particles_;
    return data;
  }

 private:
  cola::EventParticles particles_;
};

class TestGeneratorFactory final : public cola::VGeneratorFactory {
 public:
  std::unique_ptr<cola::VFilter> Create(const std::unordered_map<std::string, std::string>&) override {
    return std::make_unique<TestGenerator>(particles_);
  }

  const std::string& GetFilterName() const override {
    static const std::string kName{"generator"};
    return kName;
  }

  cola::EventParticles particles_;
};

class TestWriter final : public cola::VWriter {
 public:
  explicit TestWriter(std::shared_ptr<std::vector<std::unique_ptr<cola::EventData>>> sink) : sink_(std::move(sink)) {}

  void operator()(std::unique_ptr<cola::EventData>&& data) override { sink_->emplace_back(std::move(data)); }

 private:
  std::shared_ptr<std::vector<std::unique_ptr<cola::EventData>>> sink_;
};

class TestWriterFactory final : public cola::VWriterFactory {
 public:
  explicit TestWriterFactory(std::shared_ptr<std::vector<std::unique_ptr<cola::EventData>>> sink)
      : sink_(std::move(sink)) {}

  std::unique_ptr<cola::VFilter> Create(const std::unordered_map<std::string, std::string>&) override {
    return std::make_unique<TestWriter>(sink_);
  }

  const std::string& GetFilterName() const override {
    static const std::string kName{"writer"};
    return kName;
  }

 private:
  std::shared_ptr<std::vector<std::unique_ptr<cola::EventData>>> sink_;
};

int main() {
  const cola::Particle particle{
      .position = cola::LorentzVector{},
      .momentum =
          cola::LorentzVector{
              .e = std::sqrt(3 * std::pow(100 * CLHEP::MeV, 2) + std::pow(4 * 938 * CLHEP::MeV, 2)),
              .x = 100 * CLHEP::MeV,
              .y = 100 * CLHEP::MeV,
              .z = 100 * CLHEP::MeV,
          },
      .pdgCode = cola::AZToPdg({4, 2}),
      .pClass = cola::ParticleClass::kSpectatorA,
  };

  auto sink = std::make_shared<std::vector<std::unique_ptr<cola::EventData>>>();

  auto genFactory = std::make_unique<TestGeneratorFactory>();
  genFactory->particles_ = cola::EventParticles{particle};

  cola::MetaProcessor mp;
  mp.Register(std::move(genFactory));
  mp.Register(std::make_unique<cola::G4HandlerFactory>());
  mp.Register(std::make_unique<TestWriterFactory>(sink));

  cola::ColaRunManager manager(mp.Parse("config.xml"));
  manager.Run(1);
}
