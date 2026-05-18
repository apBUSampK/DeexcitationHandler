#include <CLHEP/Units/PhysicalConstants.h>
#include <COLA.hh>
#include <gtest/gtest.h>

#include "Deexcitation/G4HandlerFactory.h"

#include <cmath>
#include <sstream>

namespace {

  constexpr auto kPipelineConfigXml = R"(<?xml version="1.0" encoding="UTF-8" ?>
<program>
    <generator name="generator"/>
    <converter name="G4DeexcitationHandler"/>
    <writer name="writer"/>
</program>
)";

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
    explicit TestGeneratorFactory(cola::EventParticles particles) : particles_(std::move(particles)) {}

    std::unique_ptr<cola::VFilter> Create(const std::unordered_map<std::string, std::string>&) override {
      return std::make_unique<TestGenerator>(particles_);
    }

    const std::string& GetFilterName() const override {
      static const std::string kName{"generator"};
      return kName;
    }

   private:
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

  cola::MetaProcessor BuildMetaProcessor(std::shared_ptr<std::vector<std::unique_ptr<cola::EventData>>> sink,
                                         cola::EventParticles particles) {
    cola::MetaProcessor mp;
    mp.Register(std::make_unique<TestGeneratorFactory>(std::move(particles)));
    mp.Register(std::make_unique<cola::G4HandlerFactory>());
    mp.Register(std::make_unique<TestWriterFactory>(std::move(sink)));
    return mp;
  }

}  // namespace

TEST(TestModule, TestG4Handler) {
  auto sink = std::make_shared<std::vector<std::unique_ptr<cola::EventData>>>();

  {
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
    sink->clear();
    cola::MetaProcessor mp = BuildMetaProcessor(sink, {particle});
    std::istringstream xml(kPipelineConfigXml);
    cola::ColaRunManager manager(mp.Parse(xml));
    manager.Run(1);

    ASSERT_EQ(sink->size(), 1u);
    EXPECT_GE((*sink)[0]->particles.size(), 1u);
  }

  {
    const cola::Particle particle{
        .position = cola::LorentzVector{},
        .momentum =
            cola::LorentzVector{
                .e = std::sqrt(3 * std::pow(100 * CLHEP::MeV, 2) + std::pow(5 * 938 * CLHEP::MeV, 2)),
                .x = 100 * CLHEP::MeV,
                .y = 100 * CLHEP::MeV,
                .z = 100 * CLHEP::MeV,
            },
        .pdgCode = cola::AZToPdg({5, 3}),
        .pClass = cola::ParticleClass::kSpectatorA,
    };
    sink->clear();
    cola::MetaProcessor mp = BuildMetaProcessor(sink, {particle});
    std::istringstream xml(kPipelineConfigXml);
    cola::ColaRunManager manager(mp.Parse(xml));
    manager.Run(1);

    ASSERT_EQ(sink->size(), 1u);
    EXPECT_GE((*sink)[0]->particles.size(), 1u);
  }
}

TEST(ModuleExport, LoadCOLAModule_ReturnsModule) {
  std::unique_ptr<cola::VModule> mod(LoadCOLAModule());
  ASSERT_NE(mod, nullptr);
}

TEST(ModuleExport, LoadCOLAModule_ExposesSingleG4DeexcitationFactory) {
  std::unique_ptr<cola::VModule> mod(LoadCOLAModule());
  ASSERT_NE(mod, nullptr);

  const cola::FactoryMap filters = mod->GetModuleFilters();
  ASSERT_EQ(filters.size(), 1u);
  ASSERT_TRUE(filters.contains("G4DeexcitationHandler"));

  const cola::VFactory* factory = filters.at("G4DeexcitationHandler").get();
  ASSERT_NE(factory, nullptr);
  EXPECT_EQ(factory->GetFilterName(), "G4DeexcitationHandler");
  EXPECT_EQ(factory->GetFilterType(), cola::FilterType::kConverter);
}

TEST(ModuleExport, LoadCOLAModule_FactoryCreatesConverter) {
  std::unique_ptr<cola::VModule> mod(LoadCOLAModule());
  ASSERT_NE(mod, nullptr);

  cola::FactoryMap filters = mod->GetModuleFilters();
  auto it = filters.find("G4DeexcitationHandler");
  ASSERT_NE(it, filters.end());

  const std::unordered_map<std::string, std::string> meta;
  std::unique_ptr<cola::VFilter> filter = it->second->Create(meta);
  ASSERT_NE(filter, nullptr);
  auto* as_converter = dynamic_cast<cola::VConverter*>(filter.get());
  ASSERT_NE(as_converter, nullptr);
}
