#pragma once

#include <functional>
#include <memory>
#include <vector>
#include <queue>

#include <G4Fragment.hh>
#include <G4ReactionProductVector.hh>
#include <G4IonTable.hh>
#include <G4DeexPrecoParameters.hh>
#include <G4FermiPhaseDecay.hh>
#include <G4NistManager.hh>

#include <G4VMultiFragmentation.hh>
#include <G4VEvaporation.hh>
#include <G4VFermiBreakUp.hh>

class ExcitationHandler {
 private:
  using FragmentQueue = std::queue<std::unique_ptr<G4Fragment>>;

 public:
  class NeutronDecay {
   public:
    NeutronDecay() = default;

    void BreakFragment(G4FragmentVector& results, const G4Fragment& fragment);

   private:
    G4FermiPhaseDecay phaseSpaceDecay_;
  };

  using Condition = std::function<bool(const G4Fragment&)>;

  ExcitationHandler();

  ExcitationHandler(const ExcitationHandler&) = delete;

  ExcitationHandler(ExcitationHandler&&) = default;

  ~ExcitationHandler();

  ExcitationHandler& operator=(const ExcitationHandler&) = delete;

  ExcitationHandler& operator=(ExcitationHandler&&) = default;

  std::vector<G4ReactionProduct> BreakItUp(const G4Fragment& fragment);

  // parameters setters
  ExcitationHandler& SetMultiFragmentation(std::unique_ptr<G4VMultiFragmentation>&& model = DefaultMultiFragmentation()) {
    multiFragmentationModel_ = std::move(model);
    return *this;
  }

  ExcitationHandler& SetFermiBreakUp(std::unique_ptr<G4VFermiBreakUp>&& model = DefaultFermiBreakUp(),
                                     bool setModels = true) {
    fermiBreakUpModel_ = std::move(model);
    if (setModels) {
      evaporationModel_->SetFermiBreakUp(fermiBreakUpModel_.get());
    }
    return *this;
  }

  ExcitationHandler& SetEvaporation(std::unique_ptr<G4VEvaporation>&& model = DefaultEvaporation(),
                                    bool setModels = true) {
    evaporationModel_ = std::move(model);
    if (setModels) {
      evaporationModel_->SetPhotonEvaporation(photonEvaporationModel_.get());
      evaporationModel_->SetFermiBreakUp(fermiBreakUpModel_.get());
    }
    return *this;
  }

  ExcitationHandler& SetPhotonEvaporation(std::unique_ptr<G4VEvaporationChannel>&& model = DefaultPhotonEvaporation(),
                                          bool setModels = true) {
    photonEvaporationModel_ = std::move(model);
    if (setModels) {
      evaporationModel_->SetPhotonEvaporation(photonEvaporationModel_.get());
    }
    return *this;
  }

  ExcitationHandler& SetNeutronDecay(std::unique_ptr<NeutronDecay>&& model = DefaultNeutronDecay()) {
    neutronDecayModel_ = std::move(model);
    return *this;
  }

  template <class F>
  ExcitationHandler& SetMultiFragmentationCondition(F&& f) {
    multiFragmentationCondition_ = std::forward<F>(f);
    return *this;
  }

  ExcitationHandler& SetMultiFragmentationCondition() {
    return SetMultiFragmentationCondition(DefaultMultiFragmentationCondition());
  }

  template <class F>
  ExcitationHandler& SetFermiBreakUpCondition(F&& f) {
    fermiCondition_ = std::forward<F>(f);
    return *this;
  }

  ExcitationHandler& SetFermiBreakUpCondition() {
    return SetFermiBreakUpCondition(DefaultFermiBreakUpCondition());
  }

  template <class F>
  ExcitationHandler& SetEvaporationCondition(F&& f) {
    evaporationCondition_ = std::forward<F>(f);
    return *this;
  }

  ExcitationHandler& SetEvaporationCondition() {
    return SetEvaporationCondition(DefaultEvaporationCondition());
  }

  template <class F>
  ExcitationHandler& SetPhotonEvaporationCondition(F&& f) {
    photonEvaporationCondition_ = std::forward<F>(f);
    return *this;
  }

  ExcitationHandler& SetPhotonEvaporationCondition() {
    return SetPhotonEvaporationCondition(DefaultPhotonEvaporationCondition());
  }

  template <class F>
  ExcitationHandler& SetNeutronDecayCondition(F&& f) {
    neutronDecayCondition_ = std::forward<F>(f);
    return *this;
  }

  ExcitationHandler& SetNeutronDecayCondition() {
    return SetNeutronDecayCondition(DefaultNeutronDecayCondition());
  }

  ExcitationHandler& SetStableThreshold(double threshold) {
    stableThreshold_ = threshold;
    return *this;
  }

  // parameters getters
  std::unique_ptr<NeutronDecay>& GetNeutronDecay() { return neutronDecayModel_; }

  const std::unique_ptr<NeutronDecay>& GetNeutronDecay() const { return neutronDecayModel_; }

  std::unique_ptr<G4VMultiFragmentation>& GetMultiFragmentation() { return multiFragmentationModel_; }

  const std::unique_ptr<G4VMultiFragmentation>& GetMultiFragmentation() const { return multiFragmentationModel_; }

  std::unique_ptr<G4VFermiBreakUp>& GetFermiBreakUp() { return fermiBreakUpModel_; }

  const std::unique_ptr<G4VFermiBreakUp>& GetFermiBreakUp() const { return fermiBreakUpModel_; }

  std::unique_ptr<G4VEvaporation>& GetEvaporation() { return evaporationModel_; }

  const std::unique_ptr<G4VEvaporation>& GetEvaporation() const { return evaporationModel_; }

  Condition& GetMultiFragmentationCondition() { return multiFragmentationCondition_; }

  const Condition& GetMultiFragmentationCondition() const { return multiFragmentationCondition_; }

  Condition& GetFermiBreakUpCondition() { return fermiCondition_; }

  const Condition& GetFermiBreakUpCondition() const { return fermiCondition_; }

  Condition& GetEvaporationCondition() { return evaporationCondition_; }

  const Condition& GetEvaporationCondition() const { return evaporationCondition_; }

  Condition& GetPhotonEvaporationCondition() { return photonEvaporationCondition_; }

  const Condition& GetPhotonEvaporationCondition() const { return photonEvaporationCondition_; }

  Condition& GetNeutronDecayCondition() { return neutronDecayCondition_; }

  const Condition& GetNeutronDecayCondition() const { return neutronDecayCondition_; }

  double GetStableThreshold() const { return stableThreshold_; }

 protected:
  // default models and conditions
  static std::unique_ptr<G4VMultiFragmentation> DefaultMultiFragmentation();

  static std::unique_ptr<G4VFermiBreakUp> DefaultFermiBreakUp();

  static std::unique_ptr<G4VEvaporation> DefaultEvaporation();

  static std::unique_ptr<G4VEvaporationChannel> DefaultPhotonEvaporation();

  static std::unique_ptr<NeutronDecay> DefaultNeutronDecay();

  static Condition DefaultMultiFragmentationCondition();

  static Condition DefaultFermiBreakUpCondition();

  static Condition DefaultEvaporationCondition();

  static Condition DefaultPhotonEvaporationCondition();

  static Condition DefaultNeutronDecayCondition();

  bool IsGroundState(const G4Fragment& fragment) const;

  bool IsStable(const G4Fragment& fragment, const G4NistManager* nist) const;

  void ApplyMultiFragmentation(std::unique_ptr<G4Fragment>&& fragment, G4FragmentVector& results,
                               FragmentQueue& nextStage);

  void ApplyFermiBreakUp(std::unique_ptr<G4Fragment>&& fragment, G4FragmentVector& results,
                         FragmentQueue& nextStage);

  void ApplyEvaporation(std::unique_ptr<G4Fragment>&& fragment, G4FragmentVector& results,
                        FragmentQueue& nextStage);

  void ApplyPhotonEvaporation(std::unique_ptr<G4Fragment>&& fragment, G4FragmentVector& results);

  void ApplyPureNeutronDecay(std::unique_ptr<G4Fragment>&& fragment, G4FragmentVector& results);

  void GroupFragments(G4FragmentVector&& fragments, G4FragmentVector& results,
                      FragmentQueue& nextStage);

  std::vector<G4ReactionProduct> ConvertResults(const G4FragmentVector& results);

  std::unique_ptr<G4VMultiFragmentation> multiFragmentationModel_;
  std::unique_ptr<G4VFermiBreakUp> fermiBreakUpModel_;
  std::unique_ptr<G4VEvaporation> evaporationModel_;
  std::unique_ptr<G4VEvaporationChannel> photonEvaporationModel_;
  std::unique_ptr<NeutronDecay> neutronDecayModel_;

  Condition multiFragmentationCondition_;
  Condition fermiCondition_;
  Condition photonEvaporationCondition_;
  Condition evaporationCondition_;
  Condition neutronDecayCondition_;

  double stableThreshold_ = 0.;
};
