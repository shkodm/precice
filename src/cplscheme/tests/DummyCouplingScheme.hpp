#pragma once

#include "../CouplingScheme.hpp"
#include "logging/Logger.hpp"
#include "utils/assertion.hpp"

namespace precice {
namespace cplscheme {
namespace tests {

/**
 * @brief Used to test CompositionalCouplingScheme.
 */
class DummyCouplingScheme : public CouplingScheme
{
public:

  /**
   * @brief Constructor.
   *
   * @param[in] numberIterations If 1, models and explicit coupling scheme,
   *        otherwise and implicit one.
   */
  DummyCouplingScheme(
    int numberIterations,
    int maxTimesteps );

  /**
   * @brief Destructor, empty.
   */
  virtual ~DummyCouplingScheme() {}

  /**
   * @brief
   */
  virtual void initialize (
     double startTime,
     int    startTimesteps );

  /**
   * @brief Not implemented.
   */
  virtual bool isInitialized() const { assertion(false); return false; }

  /**
   * @brief Not implemented.
   */
  virtual void initializeData() { assertion(false); }

  /**
   * @brief Not implemented.
   */
  virtual void addComputedTime(double timeToAdd) { /* Do nothing */ }

  /**
   * @brief
   */
  virtual void advance();

  /**
   * @brief
   */
  virtual void finalize();

  /*
   * @brief Not implemented.
   */
  virtual std::vector<std::string> getCouplingPartners() const { assertion(false); return std::vector<std::string>(); }

  /**
   * @brief Not implemented.
   */
  virtual bool willDataBeExchanged(double lastSolverTimestepLength) const { assertion(false); return false; }

  /**
   * @brief Not implemented.
   */
  virtual bool hasDataBeenExchanged() const { assertion(false); return false; }

  /**
   * @brief Not implemented.
   */
  virtual double getTime() const { assertion(false); return 0; }

  /**
   * @brief Not implemented.
   */
  virtual int getTimesteps() const { return _timesteps; return 0; }

  /**
   * @brief Not implemented.
   */
  virtual double getMaxTime() const { assertion(false); return 0; }

  /**
   * @brief Not implemented.
   */
  virtual int getMaxTimesteps() const { assertion(false); return 0; }

  /**
   * @brief Not implemented.
   */
  virtual bool hasTimestepLength() const { assertion(false); return false; }

  /**
   * @brief Not implemented.
   */
  virtual double getTimestepLength() const { assertion(false); return 0; }

  /**
   * @brief Not implemented.
   */
  virtual double getThisTimestepRemainder() const { assertion(false); return 0; }

  /**
   * @brief Not implemented.
   */
  virtual double getComputedTimestepPart() const { assertion(false); return 0; }

  /**
   * @brief Not implemented.
   */
  virtual double getNextTimestepMaxLength() const { assertion(false); return 0; }

  /**
   * @brief Not implemented.
   */
  virtual bool isCouplingOngoing() const;

  /**
   * @brief Not implemented.
   */
  virtual bool isCouplingTimestepComplete() const { assertion(false); return false; }

  /**
   * @brief
   */
  virtual bool isActionRequired(const std::string& actionName) const;

  /**
   * @brief Not implemented.
   */
  virtual void performedAction(const std::string& actionName) { assertion(false); }

  /**
   * @brief Not implemented.
   */
  virtual int getCheckpointTimestepInterval() const { assertion(false); return 0; }

  /**
   * @brief Not implemented.
   */
  virtual void requireAction(const std::string& actionName) { assertion(false); }

  /**
   * @brief Empty.
   */
  virtual std::string printCouplingState() const { return std::string(); }

  /**
   * @brief Empty.
   */
  virtual void exportState(const std::string& filenamePrefix) const {}

  /**
   * @brief Empty.
   */
  virtual void importState(const std::string& filenamePrefix) {}

  /**
   * @brief Empty.
   */
  virtual void sendState (
    com::PtrCommunication communication,
    int                   rankReceiver ) {}

  /**
   * @brief Empty.
   */
  virtual void receiveState (
    com::PtrCommunication communication,
    int                   rankSender ) {}

private:

  mutable logging::Logger _log{"cplscheme::tests::DummyCouplingScheme"};

  // @brief Number of iterations performed per timestep. 1 --> explicit.
  int _numberIterations;

  // @brief Performed iterations in the current timestep.
  int _iterations = 0;

  // @brief Maximal number of timesteps to be performed.
  int _maxTimesteps;

  // @brief Performed number of timesteps.
  int _timesteps = 0;

  // @brief True, if initialize has been called.
  bool _isInitialized = false;

  // @brief True, if timesteps are left to be performed.
  bool _isOngoing = false;
};

}}} // namespace precice, cplscheme, tests
