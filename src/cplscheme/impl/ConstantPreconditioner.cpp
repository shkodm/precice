#include "ConstantPreconditioner.hpp"

namespace precice
{
namespace cplscheme
{
namespace impl
{

ConstantPreconditioner::ConstantPreconditioner(std::vector<double> factors)
    : Preconditioner(-1),
      _factors(factors)
{
}

void ConstantPreconditioner::initialize(std::vector<size_t> & svs)
{
  TRACE();
  Preconditioner::initialize(svs);

  // is always constant by definition
  _freezed = true;
  assertion(_maxNonConstTimesteps == -1, _maxNonConstTimesteps);

  assertion(_factors.size() == _subVectorSizes.size());

  int offset = 0;
  for (size_t k = 0; k < _subVectorSizes.size(); k++) {
    for (size_t i = 0; i < _subVectorSizes[k]; i++) {
      _weights[i + offset]    = 1.0 / _factors[k];
      _invWeights[i + offset] = _factors[k];
    }
    offset += _subVectorSizes[k];
  }
}

void ConstantPreconditioner::_update_(bool timestepComplete,
                                      const Eigen::VectorXd &oldValues,
                                      const Eigen::VectorXd &res)
{

  //nothing to do here
}
}
}
} // namespace precice, cplscheme
