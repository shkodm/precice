#pragma once

#include <Eigen/Core>
#include <vector>
#include "PostProcessing.hpp"
#include "logging/Logger.hpp"

namespace precice
{
namespace cplscheme
{
namespace impl
{

class HierarchicalAitkenPostProcessing : public PostProcessing
{
public:
  HierarchicalAitkenPostProcessing(
      double           initialRelaxation,
      std::vector<int> dataIDs);

  virtual ~HierarchicalAitkenPostProcessing(){};

  virtual std::vector<int> getDataIDs() const
  {
    return _dataIDs;
  }

  virtual void setDesignSpecification(
      Eigen::VectorXd &q);

  virtual std::map<int, Eigen::VectorXd> getDesignSpecification(DataMap &cplData);

  virtual void initialize(DataMap &cplData);

  virtual void performPostProcessing(DataMap &cplData);

  virtual void iterationsConverged(DataMap &cplData);

private:
  logging::Logger _log{"cplscheme::HierarchicalAitkenPostProcessing"};

  double _initialRelaxation;

  std::vector<int> _dataIDs;

  std::vector<double> _aitkenFactors;

  int _iterationCounter = 0;

  Eigen::VectorXd _residual;

  Eigen::VectorXd _designSpecification;

  void computeAitkenFactor(
      size_t level,
      double nominator,
      double denominator);
};
}
}
} // namespace precice, cplscheme, impl
