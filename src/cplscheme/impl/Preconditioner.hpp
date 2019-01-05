#pragma once

#include <Eigen/Core>
#include <vector>
#include "../SharedPointer.hpp"
#include "utils/assertion.hpp"

namespace precice
{
namespace cplscheme
{
namespace impl
{

/**
 * @brief Interface for preconditioner variants that can be applied to quasi-Newton post-processing schemes.
 *
 * Preconditioning (formerly also known as scaling) improves the balance between several sub-vectors for parallel or multi coupling.
 *
 * apply() applies the weighting, i.e. transforms from physical values to balanced values.
 * revert() reverts the weighting, i.e. transforms from balanced values back to physical values.
 * update() updates the preconditioner, after every FSI iteration (though some variants might only be updated after a complete timestep)
 */
class Preconditioner
{
public:
  Preconditioner(int maxNonConstTimesteps)
    : _maxNonConstTimesteps(maxNonConstTimesteps)
  {}
      
  /// Destructor, empty.
  virtual ~Preconditioner() {}
  
  /**
   * @brief initialize the preconditioner
   * @param size of the pp system (e.g. rows of V)
   */
  virtual void initialize(std::vector<size_t> & svs)
  {
    TRACE();

    assertion(_weights.size() == 0);
    _subVectorSizes = svs;

    size_t N = 0;
    for (auto elem : _subVectorSizes) {
      N += elem;
    }
    // cannot do this already in the constructor as the size is unknown at that point
    _weights.resize(N, 1.0);
    _invWeights.resize(N, 1.0);
  }

  /**
   * @brief Apply preconditioner to matrix
   * @param transpose: false = from left, true = from right
   */
  void apply(Eigen::MatrixXd &M, bool transpose)
  {
    TRACE();
    if (transpose) {
      assertion(M.cols() == (int) _weights.size(), M.cols(), _weights.size());
      for (int i = 0; i < M.cols(); i++) {
        for (int j = 0; j < M.rows(); j++) {
          M(j, i) *= _weights[i];
        }
      }
    } else {
      assertion(M.rows() == (int) _weights.size(), M.rows(), (int) _weights.size());
      for (int i = 0; i < M.cols(); i++) {
        for (int j = 0; j < M.rows(); j++) {
          M(j, i) *= _weights[j];
        }
      }
    }
  }

  /**
   * @brief Apply inverse preconditioner to matrix
   * @param transpose: false = from left, true = from right
   */
  void revert(Eigen::MatrixXd &M, bool transpose)
  {
    TRACE();
    //assertion(_needsGlobalWeights);
    if (transpose) {
      assertion(M.cols() == (int) _invWeights.size());
      for (int i = 0; i < M.cols(); i++) {
        for (int j = 0; j < M.rows(); j++) {
          M(j, i) *= _invWeights[i];
        }
      }
    } else {
      assertion(M.rows() == (int) _invWeights.size(), M.rows(), (int) _invWeights.size());
      for (int i = 0; i < M.cols(); i++) {
        for (int j = 0; j < M.rows(); j++) {
          M(j, i) *= _invWeights[j];
        }
      }
    }
  }

  /// To transform physical values to balanced values. Matrix version
  void apply(Eigen::MatrixXd &M)
  {
    TRACE();
    assertion(M.rows() == (int) _weights.size(), M.rows(), (int) _weights.size());

    // scale matrix M
    for (int i = 0; i < M.cols(); i++) {
      for (int j = 0; j < M.rows(); j++) {
        M(j, i) *= _weights[j];
      }
    }
  }

  /// To transform physical values to balanced values. Vector version
  void apply(Eigen::VectorXd &v)
  {
    TRACE();

    assertion(v.size() == (int) _weights.size());

    // scale residual
    for (int j = 0; j < v.size(); j++) {
      v[j] *= _weights[j];
    }
  }

  /// To transform balanced values back to physical values. Matrix version
  void revert(Eigen::MatrixXd &M)
  {
    TRACE();

    assertion(M.rows() == (int) _weights.size());

    // scale matrix M
    for (int i = 0; i < M.cols(); i++) {
      for (int j = 0; j < M.rows(); j++) {
        M(j, i) *= _invWeights[j];
      }
    }
  }

  /// To transform balanced values back to physical values. Vector version
  void revert(Eigen::VectorXd &v)
  {
    TRACE();

    assertion(v.size() == (int) _weights.size());

    // scale residual
    for (int j = 0; j < v.size(); j++) {
      v[j] *= _invWeights[j];
    }
  }

  /**
   * @brief Update the scaling after every FSI iteration and require a new QR decomposition (if necessary)
   *
   * @param[in] timestepComplete True if this FSI iteration also completed a timestep
   */
  void update(bool timestepComplete, const Eigen::VectorXd &oldValues, const Eigen::VectorXd &res)
  {
    TRACE(_nbNonConstTimesteps, _freezed);

    // if number of allowed non-const time steps is exceeded, do not update weights
    if (_freezed)
      return;

    // increment number of time steps that has been scaled with changing preconditioning weights
    if (timestepComplete) {
      _nbNonConstTimesteps++;
      if (_nbNonConstTimesteps >= _maxNonConstTimesteps && _maxNonConstTimesteps > 0)
        _freezed = true;
    }

    // type specific update functionality
    _update_(timestepComplete, oldValues, res);
  }

  /// returns true if a QR decomposition from scratch is necessary
  bool requireNewQR()
  {
    TRACE(_requireNewQR);
    return _requireNewQR;
  }

  /// to tell the preconditioner that QR-decomposition has been recomputed
  void newQRfulfilled()
  {
    _requireNewQR = false;
  }

  std::vector<double> &getWeights()
  {
    return _weights;
  }

  bool isConst()
  {
    return _freezed;
  }

protected:
  /// Weights used to scale the matrix V and the residual
  std::vector<double> _weights;

  /// Inverse weights (for efficiency reasons)
  std::vector<double> _invWeights;

  /// Sizes of each sub-vector, i.e. each coupling data
  std::vector<size_t> _subVectorSizes;

  /** @brief maximum number of non-const time steps, i.e., after this number of time steps,
   *  the preconditioner is freezed with the current weights and becomes a constant preconditioner
   */
  int _maxNonConstTimesteps;

  /// Counts the number of completed time steps with a non-const weighting
  int _nbNonConstTimesteps = 0;

  /// True if a QR decomposition from scratch is necessary
  bool _requireNewQR = false;

  /// True if _nbNonConstTimesteps >= _maxNonConstTimesteps, i.e., preconditioner is not updated any more.
  bool _freezed = false;

  /**
   * @brief Update the scaling after every FSI iteration and require a new QR decomposition (if necessary)
   *
   * @param[in] timestepComplete True if this FSI iteration also completed a timestep
   */
  virtual void _update_(bool timestepComplete, const Eigen::VectorXd &oldValues, const Eigen::VectorXd &res) = 0;

private:
  logging::Logger _log{"cplscheme::Preconditioner"};
};
}
}
} // namespace precice, cplscheme, impl
