#include "ConstantRelaxationPostProcessing.hpp"
#include <Eigen/Core>
#include "../CouplingData.hpp"
#include "mesh/Data.hpp"
#include "mesh/Mesh.hpp"
#include "utils/EigenHelperFunctions.hpp"
#include "utils/Helpers.hpp"

namespace precice
{
namespace cplscheme
{
namespace impl
{

ConstantRelaxationPostProcessing::ConstantRelaxationPostProcessing(
    double           relaxation,
    std::vector<int> dataIDs)
  : _relaxation(relaxation),
    _dataIDs(dataIDs)
{
  CHECK((relaxation > 0.0) && (relaxation <= 1.0),
        "Relaxation factor for constant relaxation post processing "
        << "has to be larger than zero and smaller or equal than one!");
}

void ConstantRelaxationPostProcessing::initialize(DataMap &cplData)
{
  CHECK(_dataIDs.size() == 0 || utils::contained(*_dataIDs.begin(), cplData),
        "Data with ID " << *_dataIDs.begin()
                        << " is not contained in data given at initialization!");

  // Append column for old values if not done by coupling scheme yet
  int entries = 0;
  for (auto &elem : _dataIDs) {
    entries += cplData[elem]->values->size();
  }
  _designSpecification = Eigen::VectorXd::Zero(entries);

  for (DataMap::value_type &pair : cplData) {
    int cols = pair.second->oldValues.cols();
    if (cols < 1) {
      assertion(pair.second->values->size() > 0, pair.first);
      utils::append(pair.second->oldValues, (Eigen::VectorXd) Eigen::VectorXd::Zero(pair.second->values->size()));
    }
  }
}

void ConstantRelaxationPostProcessing::performPostProcessing(DataMap &cplData)
{
  TRACE();
  double omega         = _relaxation;
  double oneMinusOmega = 1.0 - omega;
  for (DataMap::value_type &pair : cplData) {
    auto &      values    = *pair.second->values;
    const auto &oldValues = pair.second->oldValues.col(0);
    values *= omega;
    values += oldValues * oneMinusOmega;
    DEBUG("pp values" << values);
  }
}

/*
 * Returns the design specification corresponding to the given coupling data. 
 * 
 * This information is needed for convergence measurements in the coupling scheme.
 * @todo: Change to call by ref when Eigen is used.
 */
std::map<int, Eigen::VectorXd> ConstantRelaxationPostProcessing::getDesignSpecification(
    DataMap &cplData)
{

  std::map<int, Eigen::VectorXd> designSpecifications;
  int                            off = 0;
  for (int id : _dataIDs) {
    int             size = cplData[id]->values->size();
    Eigen::VectorXd q    = Eigen::VectorXd::Zero(size);
    for (int i = 0; i < size; i++) {
      q(i) = _designSpecification(i + off);
    }
    off += size;
    std::map<int, Eigen::VectorXd>::value_type pair = std::make_pair(id, q);
    designSpecifications.insert(pair);
  }
  return designSpecifications;
}

void ConstantRelaxationPostProcessing::setDesignSpecification(Eigen::VectorXd &q)
{
  _designSpecification = q;
  ERROR("Design specification for constant relaxation is not supported yet.");
}
}
}
} // namespace precice, cplscheme, impl
