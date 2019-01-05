#ifndef PRECICE_NO_MPI

#include "MVQNPostProcessing.hpp"
#include <Eigen/Core>
#include "com/Communication.hpp"
#include "com/MPIPortsCommunication.hpp"
#include "com/SocketCommunication.hpp"
#include "cplscheme/CouplingData.hpp"
#include "mesh/Mesh.hpp"
#include "mesh/Vertex.hpp"
#include "utils/EigenHelperFunctions.hpp"
#include "utils/MasterSlave.hpp"
#include "utils/Publisher.hpp"

#include <fstream>
#include <sstream>

using precice::utils::Publisher;

namespace precice
{
namespace cplscheme
{
namespace impl
{

// ==================================================================================
MVQNPostProcessing::MVQNPostProcessing(
    double            initialRelaxation,
    bool              forceInitialRelaxation,
    int               maxIterationsUsed,
    int               timestepsReused,
    int               filter,
    double            singularityLimit,
    std::vector<int>  dataIDs,
    PtrPreconditioner preconditioner,
    bool              alwaysBuildJacobian,
    int               imvjRestartType,
    int               chunkSize,
    int               RSLSreusedTimesteps,
    double            RSSVDtruncationEps)
    : BaseQNPostProcessing(initialRelaxation, forceInitialRelaxation, maxIterationsUsed, timestepsReused,
                           filter, singularityLimit, dataIDs, preconditioner),
      //  _secondaryOldXTildes(),
      _invJacobian(),
      _oldInvJacobian(),
      _Wtil(),
      _WtilChunk(),
      _pseudoInverseChunk(),
      _matrixV_RSLS(),
      _matrixW_RSLS(),
      _matrixCols_RSLS(),
      _cyclicCommLeft(nullptr),
      _cyclicCommRight(nullptr),
      _parMatrixOps(nullptr),
      _svdJ(RSSVDtruncationEps, preconditioner),
      _alwaysBuildJacobian(alwaysBuildJacobian),
      _imvjRestartType(imvjRestartType),
      _imvjRestart(false),
      _chunkSize(chunkSize),
      _RSLSreusedTimesteps(RSLSreusedTimesteps),
      _usedColumnsPerTstep(5),
      _nbRestarts(0),
      //_info2(),
      _avgRank(0)
{
}

// ==================================================================================
MVQNPostProcessing::~MVQNPostProcessing()
{
  //if(utils::MasterSlave::_masterMode ||utils::MasterSlave::_slaveMode){ // not possible because of tests, MasterSlave is deactivated when PP is killed

  // close and shut down cyclic communication connections
  if (not _imvjRestart) {
    if (_cyclicCommRight != nullptr || _cyclicCommLeft != nullptr) {
      if ((utils::MasterSlave::_rank % 2) == 0) {
        _cyclicCommLeft->closeConnection();
        _cyclicCommRight->closeConnection();
      } else {
        _cyclicCommRight->closeConnection();
        _cyclicCommLeft->closeConnection();
      }
      _cyclicCommRight = nullptr;
      _cyclicCommLeft  = nullptr;
    }
  }
}

// ==================================================================================
void MVQNPostProcessing::initialize(
    DataMap &cplData)
{
  TRACE();
  
  // do common QN post processing initialization
  BaseQNPostProcessing::initialize(cplData);

  if (_imvjRestartType > 0)
    _imvjRestart = true;

  // only need cyclic communication if no MVJ restart mode is used
  if (not _imvjRestart) {
    if (utils::MasterSlave::_masterMode || utils::MasterSlave::_slaveMode) {
    /*
     * @todo: FIXME: This is a temporary and hacky realization of the cyclic commmunication between slaves
     *        Therefore the requesterName and accessorName are not given (cf solverInterfaceImpl).
     *        The master-slave communication should be modified such that direct communication between
     *        slaves is possible (via MPIDirect)
     */

#ifdef SuperMUC_WORK
      try {
        // auto addressDirectory = std::to_string(".");
        if (utils::MasterSlave::_masterMode) {
          for (int rank = 0; rank < utils::MasterSlave::_size; ++rank) {
            Publisher::createDirectory(std::string(".") + "/" + "." + "cyclicComm-" + std::to_string(rank) + ".address");
          }
        }
        utils::Parallel::synchronizeProcesses();
      } catch (...) {
      }
#endif

      _cyclicCommLeft  = com::PtrCommunication(new com::MPIPortsCommunication());
      _cyclicCommRight = com::PtrCommunication(new com::MPIPortsCommunication());
      //_cyclicCommLeft = com::Communication::SharedPointer(new com::SocketCommunication(0, false, "ib0", "."));
      //_cyclicCommRight = com::Communication::SharedPointer(new com::SocketCommunication(0, false, "ib0", "."));

      // initialize cyclic communication between successive slaves
      int prevProc = (utils::MasterSlave::_rank - 1 < 0) ? utils::MasterSlave::_size - 1 : utils::MasterSlave::_rank - 1;
      if ((utils::MasterSlave::_rank % 2) == 0) {
#ifdef SuperMUC_WORK
        Publisher::ScopedPushDirectory spd1(std::string(".") + "cyclicComm-" + std::to_string(prevProc) + ".address");
#endif
        _cyclicCommLeft->acceptConnection("cyclicComm-" + std::to_string(prevProc), "", utils::MasterSlave::_rank);
#ifdef SuperMUC_WORK
        Publisher::ScopedPushDirectory spd2(std::string(".") + "cyclicComm-" + std::to_string(utils::MasterSlave::_rank) + ".address");
#endif
        _cyclicCommRight->requestConnection("cyclicComm-" + std::to_string(utils::MasterSlave::_rank), "", 0, 1);
      } else {
#ifdef SuperMUC_WORK
        Publisher::ScopedPushDirectory spd3(std::string(".") + "cyclicComm-" + std::to_string(utils::MasterSlave::_rank) + ".address");
#endif
        _cyclicCommRight->requestConnection("cyclicComm-" + std::to_string(utils::MasterSlave::_rank), "", 0, 1);
#ifdef SuperMUC_WORK
        Publisher::ScopedPushDirectory spd4(std::string(".") + "cyclicComm-" + std::to_string(prevProc) + ".address");
#endif
        _cyclicCommLeft->acceptConnection("cyclicComm-" + std::to_string(prevProc), "", utils::MasterSlave::_rank);
      }
    }
  }

  // initialize parallel matrix-matrix operation module
  _parMatrixOps = impl::PtrParMatrixOps(new impl::ParallelMatrixOperations());
  _parMatrixOps->initialize(_cyclicCommLeft, _cyclicCommRight, not _imvjRestart);
  _svdJ.initialize(_parMatrixOps, getLSSystemRows());

  int entries  = _residuals.size();
  int global_n = 0;

  if (not utils::MasterSlave::_masterMode && not utils::MasterSlave::_slaveMode) {
    global_n = entries;
  } else {
    global_n = _dimOffsets.back();
  }

  if (not _imvjRestart) {
    // only need memory for Jacobain of not in restart mode
    _invJacobian    = Eigen::MatrixXd::Zero(global_n, entries);
    _oldInvJacobian = Eigen::MatrixXd::Zero(global_n, entries);
  }
  // initialize V, W matrices for the LS restart
  if (_imvjRestartType == RS_LS) {
    _matrixCols_RSLS.push_front(0);
    _matrixV_RSLS = Eigen::MatrixXd::Zero(entries, 0);
    _matrixW_RSLS = Eigen::MatrixXd::Zero(entries, 0);
  }
  _Wtil = Eigen::MatrixXd::Zero(entries, 0);

  if (utils::MasterSlave::_masterMode || (not utils::MasterSlave::_masterMode && not utils::MasterSlave::_slaveMode))
    _infostringstream << " IMVJ restart mode: " << _imvjRestart << "\n chunk size: " << _chunkSize << "\n trunc eps: " << _svdJ.getThreshold() << "\n R_RS: " << _RSLSreusedTimesteps << "\n--------\n"
                      << std::endl;
}

// ==================================================================================
void MVQNPostProcessing::computeUnderrelaxationSecondaryData(
    DataMap &cplData)
{
  // Perform underrelaxation with initial relaxation factor for secondary data
  for (int id : _secondaryDataIDs) {
    PtrCouplingData  data   = cplData[id];
    Eigen::VectorXd &values = *(data->values);
    values *= _initialRelaxation; // new * omg
    Eigen::VectorXd &secResiduals = _secondaryResiduals[id];
    secResiduals                  = data->oldValues.col(0); // old
    secResiduals *= 1.0 - _initialRelaxation;               // (1-omg) * old
    values += secResiduals;                                 // (1-omg) * old + new * omg
  }
}

// ==================================================================================
void MVQNPostProcessing::updateDifferenceMatrices(
    DataMap &cplData)
{
  /**
   *  Matrices and vectors used in this method as well as the result Wtil are
   *  NOT SCALED by the preconditioner.
   */

  TRACE();
  
  // call the base method for common update of V, W matrices
  // important that base method is called before updating _Wtil
  BaseQNPostProcessing::updateDifferenceMatrices(cplData);

  // update _Wtil if the efficient computation of the quasi-Newton update is used
  // or update current Wtil if the restart mode of imvj is used
  if (not _alwaysBuildJacobian || _imvjRestart) {
    if (_firstIteration && (_firstTimeStep || _forceInitialRelaxation)) {
      // do nothing: constant relaxation
    } else {
      if (not _firstIteration) {
        // Update matrix _Wtil = (W - J_prev*V) with newest information

        Eigen::VectorXd v = _matrixV.col(0);
        Eigen::VectorXd w = _matrixW.col(0);

        // here, we check for _Wtil.cols() as the matrices V, W need to be updated before hand
        // and thus getLSSystemCols() does not yield the correct result.
        bool columnLimitReached = _Wtil.cols() == _maxIterationsUsed;
        bool overdetermined     = _Wtil.cols() <= getLSSystemRows();

        Eigen::VectorXd wtil = Eigen::VectorXd::Zero(_matrixV.rows());

        // add column: Wtil(:,0) = W(:,0) - sum_q [ Wtil^q * ( Z^q * V(:,0)) ]
        //                                         |--- J_prev ---|
        // iterate over all stored Wtil and Z matrices in current chunk
        if (_imvjRestart) {
          for (int i = 0; i < (int) _WtilChunk.size(); i++) {
            int colsLSSystemBackThen = _pseudoInverseChunk[i].rows();
            assertion(colsLSSystemBackThen == _WtilChunk[i].cols(), colsLSSystemBackThen, _WtilChunk[i].cols());
            Eigen::VectorXd Zv = Eigen::VectorXd::Zero(colsLSSystemBackThen);
            // multiply: Zv := Z^q * V(:,0) of size (m x 1)
            _parMatrixOps->multiply(_pseudoInverseChunk[i], v, Zv, colsLSSystemBackThen, getLSSystemRows(), 1);
            // multiply: Wtil^q * Zv  dimensions: (n x m) * (m x 1), fully local
            wtil += _WtilChunk[i] * Zv;
          }

          // store columns if restart mode = RS-LS
          if (_imvjRestartType == RS_LS) {
            if (_matrixCols_RSLS.front() < _usedColumnsPerTstep) {
              utils::appendFront(_matrixV_RSLS, v);
              utils::appendFront(_matrixW_RSLS, w);
              _matrixCols_RSLS.front()++;
            }
          }

          // imvj without restart is used, but efficient update, i.e. no Jacobian assembly in each iteration
          // add column: Wtil(:,0) = W(:,0) - J_prev * V(:,0)
        } else {
          // compute J_prev * V(0) := wtil the new column in _Wtil of dimension: (n x n) * (n x 1) = (n x 1),
          //                                        parallel: (n_global x n_local) * (n_local x 1) = (n_local x 1)
          _parMatrixOps->multiply(_oldInvJacobian, v, wtil, _dimOffsets, getLSSystemRows(), getLSSystemRows(), 1, false);
        }
        wtil *= -1;
        wtil += w;

        if (not columnLimitReached && overdetermined) {
          utils::appendFront(_Wtil, wtil);
        } else {
          utils::shiftSetFirst(_Wtil, wtil);
        }
      }
    }
  }
}

// ==================================================================================
void MVQNPostProcessing::computeQNUpdate(
    PostProcessing::DataMap &cplData,
    Eigen::VectorXd &        xUpdate)
{
  /**
   * The inverse Jacobian
   *
   *        J_inv = J_inv_n + (W - J_inv_n*V)*(V^T*V)^-1*V^T
   *
   * is computed and the resulting quasi-Newton update is returned.
   * Used matrices (V, W, Wtil, invJacobian, oldINvJacobian) are
   * scaled with the used preconditioner.
   */

  /**
   *  The MVJ- quasi-Newton update
   *  Either compute efficient, omitting to build the Jacobian in each iteration or inefficient.
   *  INVARIANT: All objects, J_inv, J_old_inv, W, V, Wtil, xUpdate, res, etc. are unscaled.
   *             Only the QR-decomposition of V is scaled and thus needs to be unscaled before
   *             using it in multiplications with the other matrices.
   */
  if (_alwaysBuildJacobian) {
    computeNewtonUpdate(cplData, xUpdate);
  } else {
    computeNewtonUpdateEfficient(cplData, xUpdate);
  }
}

// ==================================================================================
void MVQNPostProcessing::pseudoInverse(
    Eigen::MatrixXd &pseudoInverse)
{
  TRACE();
  /**
   *   computation of pseudo inverse matrix Z = (V^TV)^-1 * V^T as solution
   *   to the equation R*z = Q^T(i) for all columns i,  via back substitution.
   */
  auto Q = _qrV.matrixQ();
  auto R = _qrV.matrixR();

  assertion(pseudoInverse.rows() == _qrV.cols(), pseudoInverse.rows(), _qrV.cols());
  assertion(pseudoInverse.cols() == _qrV.rows(), pseudoInverse.cols(), _qrV.rows());

  Eigen::VectorXd yVec(pseudoInverse.rows());

  // assertions for the case of processors with no vertices
  if (!_hasNodesOnInterface) {
    assertion(_qrV.cols() == getLSSystemCols(), _qrV.cols(), getLSSystemCols());
    assertion(_qrV.rows() == 0, _qrV.rows());
    assertion(Q.size() == 0, Q.size());
  }

  // backsubstitution
  for (int i = 0; i < Q.rows(); i++) {
    Eigen::VectorXd Qrow = Q.row(i);
    yVec                 = R.triangularView<Eigen::Upper>().solve<Eigen::OnTheLeft>(Qrow);
    pseudoInverse.col(i) = yVec;
  } // ----------------

  // scale pseudo inverse back Z := Z' * P,
  // Z' is scaled pseudo inverse i.e, Z' = R^-1 * Q^T * P^-1
  _preconditioner->apply(pseudoInverse, true);
  //  e.stop(true);
}

// ==================================================================================
void MVQNPostProcessing::buildWtil()
{
  /**
   * PRECONDITION: Assumes that V, W, J_prev are already preconditioned,
   */
  TRACE();

  assertion(_matrixV.rows() == _qrV.rows(), _matrixV.rows(), _qrV.rows());
  assertion(getLSSystemCols() == _qrV.cols(), getLSSystemCols(), _qrV.cols());

  _Wtil = Eigen::MatrixXd::Zero(_qrV.rows(), _qrV.cols());

  // imvj restart mode: re-compute Wtil: Wtil = W - sum_q [ Wtil^q * (Z^q*V) ]
  //                                                      |--- J_prev ---|
  // iterate over all stored Wtil and Z matrices in current chunk
  if (_imvjRestart) {
    for (int i = 0; i < (int) _WtilChunk.size(); i++) {
      int colsLSSystemBackThen = _pseudoInverseChunk[i].rows();
      assertion(colsLSSystemBackThen == _WtilChunk[i].cols(), colsLSSystemBackThen, _WtilChunk[i].cols());
      Eigen::MatrixXd ZV = Eigen::MatrixXd::Zero(colsLSSystemBackThen, _qrV.cols());
      // multiply: ZV := Z^q * V of size (m x m) with m=#cols, stored on each proc.
      _parMatrixOps->multiply(_pseudoInverseChunk[i], _matrixV, ZV, colsLSSystemBackThen, getLSSystemRows(), _qrV.cols());
      // multiply: Wtil^q * ZV  dimensions: (n x m) * (m x m), fully local and embarrassingly parallel
      _Wtil += _WtilChunk[i] * ZV;
    }

    // imvj without restart is used, i.e., recompute Wtil: Wtil = W - J_prev * V
  } else {
    // multiply J_prev * V = W_til of dimension: (n x n) * (n x m) = (n x m),
    //                                    parallel:  (n_global x n_local) * (n_local x m) = (n_local x m)
    _parMatrixOps->multiply(_oldInvJacobian, _matrixV, _Wtil, _dimOffsets, getLSSystemRows(), getLSSystemRows(), getLSSystemCols(), false);
  }

  // W_til = (W-J_inv_n*V) = (W-V_tilde)
  _Wtil *= -1.;
  _Wtil = _Wtil + _matrixW;

  _resetLS = false;
  //  e.stop(true);
}

// ==================================================================================
void MVQNPostProcessing::buildJacobian()
{
  TRACE();
  /**      --- compute inverse Jacobian ---
  *
  * J_inv = J_inv_n + (W - J_inv_n*V)*(V^T*V)^-1*V^T
  *
  * assumes that J_prev, V, W are already preconditioned
  */

  /**
   *  (1) computation of pseudo inverse Z = (V^TV)^-1 * V^T
   */
  Eigen::MatrixXd Z(_qrV.cols(), _qrV.rows());
  pseudoInverse(Z);

  /**
  *  (2) Multiply J_prev * V =: W_tilde
  */
  assertion(_matrixV.rows() == _qrV.rows(), _matrixV.rows(), _qrV.rows());
  assertion(getLSSystemCols() == _qrV.cols(), getLSSystemCols(), _qrV.cols());
  if (_resetLS) {
    buildWtil();
    WARN(" ATTENTION, in buildJacobian call for buildWtill() - this should not be the case except the coupling did only one iteration");
  }

  /** (3) compute invJacobian = W_til*Z
  *
  *  where Z = (V^T*V)^-1*V^T via QR-dec and back-substitution       dimension: (n x n) * (n x m) = (n x m),
  *  and W_til = (W - J_inv_n*V)                                     parallel:  (n_global x n_local) * (n_local x m) = (n_local x m)
  */
  _parMatrixOps->multiply(_Wtil, Z, _invJacobian, _dimOffsets, getLSSystemRows(), getLSSystemCols(), getLSSystemRows());
  // --------

  // update Jacobian
  _invJacobian = _invJacobian + _oldInvJacobian;
}

// ==================================================================================
void MVQNPostProcessing::computeNewtonUpdateEfficient(
    PostProcessing::DataMap &cplData,
    Eigen::VectorXd &        xUpdate)
{
  TRACE();
  
  /**      --- update inverse Jacobian efficient, ---
  *   If normal mode is used:
  *   Do not recompute W_til in every iteration and do not build
  *   the entire Jacobian matrix. This is only necessary if the coupling
  *   iteration has converged, namely in the last iteration.
  *
  *   If restart-mode is used:
  *   The Jacobian is never build. Store matrices Wtil^q and Z^q for the last M time steps.
  *   After M time steps, a restart algorithm is performed basedon the restart-mode type, either
  *   Least-Squares restart (IQN-ILS like) or maintaining of a updated truncated SVD decomposition
  *   of the SVD.
  *
  * J_inv = J_inv_n + (W - J_inv_n*V)*(V^T*V)^-1*V^T
  *
  * ASSUMPTION: All objects are scaled with the active preconditioner
  */

  /**
   *  (1) computation of pseudo inverse Z = (V^TV)^-1 * V^T
   */
  Eigen::MatrixXd Z(_qrV.cols(), _qrV.rows());
  pseudoInverse(Z);

  /**
  *  (2) Construction of _Wtil = (W - J_prev * V), should be already present due to updated computation
  */
  assertion(_matrixV.rows() == _qrV.rows(), _matrixV.rows(), _qrV.rows());
  assertion(getLSSystemCols() == _qrV.cols(), getLSSystemCols(), _qrV.cols());

  // rebuild matrix Wtil if V changes substantially.
  if (_resetLS) {
    buildWtil();
  }

  /**
   *  Avoid computation of Z*Wtil = Jtil \in (n x n). Rather do matrix-vector computations
   *  [ J_prev*(-res) ] + [Wtil * [Z * (-res)] ]
   *  '----- 1 -------'           '----- 2 ----'
   *                      '-------- 3 ---------'
   *
   *  (3) compute r_til = Z*(-residual)   where Z = (V^T*V)^-1*V^T via QR-dec and back-substitution
   *
   *  dimension: (m x n) * (n x 1) = (m x 1),
   *  parallel:  (m x n_local) * (n x 1) = (m x 1)
   */
  Eigen::VectorXd negativeResiduals = -_residuals;
  Eigen::VectorXd r_til             = Eigen::VectorXd::Zero(getLSSystemCols());
  _parMatrixOps->multiply(Z, negativeResiduals, r_til, getLSSystemCols(), getLSSystemRows(), 1); // --------

  /**
   * (4) compute _Wtil * r_til
   *
   * dimension: (n x m) * (m x 1) = (n x 1),
   * parallel:  (n_local x m) * (m x 1) = (n_local x 1)
   *
   * Note: r_til is not distributed but locally stored on each proc (dimension m x 1)
   */
  Eigen::VectorXd xUptmp(_residuals.size());
  xUpdate = Eigen::VectorXd::Zero(_residuals.size());
  xUptmp  = _Wtil * r_til; // local product, result is naturally distributed.

  /**
   *  (5) xUp = J_prev * (-res) + Wtil*Z*(-res)
   *
   *  restart-mode: sum_q { Wtil^q * [ Z^q * (-res) ] },
   *  where r_til = Z^q * (-res) is computed first and then xUp := Wtil^q * r_til
   */
  if (_imvjRestart) {
    for (int i = 0; i < (int) _WtilChunk.size(); i++) {
      int colsLSSystemBackThen = _pseudoInverseChunk[i].rows();
      assertion(colsLSSystemBackThen == _WtilChunk[i].cols(), colsLSSystemBackThen, _WtilChunk[i].cols());
      r_til = Eigen::VectorXd::Zero(colsLSSystemBackThen);
      // multiply: r_til := Z^q * (-res) of size (m x 1) with m=#cols of LS at that time, result stored on each proc.
      _parMatrixOps->multiply(_pseudoInverseChunk[i], negativeResiduals, r_til, colsLSSystemBackThen, getLSSystemRows(), 1);
      // multiply: Wtil^q * r_til  dimensions: (n x m) * (m x 1), fully local and embarrassingly parallel
      xUpdate += _WtilChunk[i] * r_til;
    }

    // imvj without restart is used, i.e., compute directly J_prev * (-res)
  } else {
    _parMatrixOps->multiply(_oldInvJacobian, negativeResiduals, xUpdate, _dimOffsets, getLSSystemRows(), getLSSystemRows(), 1, false);
    DEBUG("Mult J*V DONE");
  }

  xUpdate += xUptmp;

  // pending deletion: delete Wtil
  if (_firstIteration && _timestepsReused == 0 && not _forceInitialRelaxation) {
    _Wtil.conservativeResize(0, 0);
    _resetLS = true;
  }
}

// ==================================================================================
void MVQNPostProcessing::computeNewtonUpdate(PostProcessing::DataMap &cplData, Eigen::VectorXd &xUpdate)
{
  TRACE();
  
  /**      --- update inverse Jacobian ---
	*
	* J_inv = J_inv_n + (W - J_inv_n*V)*(V^T*V)^-1*V^T
	*/

  /**  (1) computation of pseudo inverse Z = (V^TV)^-1 * V^T
   */
  Eigen::MatrixXd Z(_qrV.cols(), _qrV.rows());
  pseudoInverse(Z);

  /**  (2) Multiply J_prev * V =: W_tilde
	*/
  buildWtil();

  /**  (3) compute invJacobian = W_til*Z
	*
	*  where Z = (V^T*V)^-1*V^T via QR-dec and back-substitution             dimension: (n x n) * (n x m) = (n x m),
	*  and W_til = (W - J_inv_n*V)                                           parallel:  (n_global x n_local) * (n_local x m) = (n_local x m)
	*/
  _parMatrixOps->multiply(_Wtil, Z, _invJacobian, _dimOffsets, getLSSystemRows(), getLSSystemCols(), getLSSystemRows()); // --------

  // update Jacobian
  _invJacobian = _invJacobian + _oldInvJacobian;

  /**  (4) solve delta_x = - J_inv * res
	 */
  Eigen::VectorXd negativeResiduals = -_residuals;

  // multiply J_inv * (-res) = x_Update of dimension: (n x n) * (n x 1) = (n x 1),
  //                                        parallel: (n_global x n_local) * (n_local x 1) = (n_local x 1)
  _parMatrixOps->multiply(_invJacobian, negativeResiduals, xUpdate, _dimOffsets, getLSSystemRows(), getLSSystemRows(), 1, false); // --------
}

// ==================================================================================
void MVQNPostProcessing::restartIMVJ()
{
  TRACE();
  
  //int used_storage = 0;
  //int theoreticalJ_storage = 2*getLSSystemRows()*_residuals.size() + 3*_residuals.size()*getLSSystemCols() + _residuals.size()*_residuals.size();
  //               ------------ RESTART SVD ------------
  if (_imvjRestartType == MVQNPostProcessing::RS_SVD) {

    // we need to compute the updated SVD of the scaled Jacobian matrix
    // |= APPLY PRECONDITIONING  J_prev = Wtil^q, Z^q  ===|
    for (int i = 0; i < (int) _WtilChunk.size(); i++) {
      _preconditioner->apply(_WtilChunk[i]);
      _preconditioner->revert(_pseudoInverseChunk[i], true);
    }
    // |===================                            ===|

    int rankBefore = _svdJ.isSVDinitialized() ? _svdJ.rank() : 0;

    // if it is the first time step, there is no initial SVD, so take all Wtil, Z matrices
    // otherwise, the first element of each container holds the decomposition of the current
    // truncated SVD, i.e., Wtil^0 = \phi, Z^0 = S\psi^T, this should not be added to the SVD.
    int q = _svdJ.isSVDinitialized() ? 1 : 0;

    // perform M-1 rank-1 updates of the truncated SVD-dec of the Jacobian
    for (; q < (int) _WtilChunk.size(); q++) {
      // update SVD, i.e., PSI * SIGMA * PHI^T <-- PSI * SIGMA * PHI^T + Wtil^q * Z^q
      _svdJ.update(_WtilChunk[q], _pseudoInverseChunk[q].transpose());
      //  used_storage += 2*_WtilChunk.size();
    }
    // int m = _WtilChunk[q].cols(), n = _WtilChunk[q].rows();
    //used_storage += 2*rankBefore*m + 4*m*n + 2*m*m + (rankBefore+m)*(rankBefore+m) + 2*n*(rankBefore+m);

    // drop all stored Wtil^q, Z^q matrices
    _WtilChunk.clear();
    _pseudoInverseChunk.clear();

    auto &psi   = _svdJ.matrixPsi();
    auto &sigma = _svdJ.singularValues();
    auto &phi   = _svdJ.matrixPhi();

    // multiply sigma * phi^T, phi is distributed block-row wise, phi^T is distributed block-column wise
    // sigma is stored local on each proc, thus, the multiplication is fully local, no communication.
    // Z = sigma * phi^T
    Eigen::MatrixXd Z(phi.cols(), phi.rows());
    for (int i = 0; i < (int) Z.rows(); i++)
      for (int j = 0; j < (int) Z.cols(); j++)
        Z(i, j) = phi(j, i) * sigma[i];

    int rankAfter = _svdJ.rank();
    int waste     = _svdJ.getWaste();
    _avgRank += rankAfter;

    // store factorized truncated SVD of J
    _WtilChunk.push_back(psi);
    _pseudoInverseChunk.push_back(Z);

    // |= REVERT PRECONDITIONING  J_prev = Wtil^0, Z^0  ==|
    _preconditioner->revert(_WtilChunk.front());
    _preconditioner->apply(_pseudoInverseChunk.front(), true);
    // |===================                             ==|

    DEBUG("MVJ-RESTART, mode=SVD. Rank of truncated SVD of Jacobian " << rankAfter << ", new modes: " << rankAfter - rankBefore << ", truncated modes: " << waste << " avg rank: " << _avgRank / _nbRestarts);
    //double percentage = 100.0*used_storage/(double)theoreticalJ_storage;
    if (utils::MasterSlave::_masterMode || (not utils::MasterSlave::_masterMode && not utils::MasterSlave::_slaveMode))
      _infostringstream << " - MVJ-RESTART " << _nbRestarts << ", mode= SVD -\n  new modes: " << rankAfter - rankBefore << "\n  rank svd: " << rankAfter << "\n  avg rank: " << _avgRank / _nbRestarts << "\n  truncated modes: " << waste << "\n"
                        << std::endl;

    //        ------------ RESTART LEAST SQUARES ------------
  } else if (_imvjRestartType == MVQNPostProcessing::RS_LS) {
    // drop all stored Wtil^q, Z^q matrices
    _WtilChunk.clear();
    _pseudoInverseChunk.clear();

    if (_matrixV_RSLS.cols() > 0) {
      // avoid that the syste mis getting too squared
      while (_matrixV_RSLS.cols() * 2 >= getLSSystemRows()) {
        removeMatrixColumnRSLS(_matrixV_RSLS.cols() - 1);
      }

      // preconditioning
      // V needs to be sclaed to compute the pseudo inverse
      // W only needs to be scaled, as the design requires to store scaled
      // matrices Wtil^0 and Z^0 as initial guess after the restart
      _preconditioner->apply(_matrixV_RSLS);
      _preconditioner->apply(_matrixW_RSLS);

      QRFactorization qr(_filter);
      qr.setGlobalRows(getLSSystemRows());
      // for QR2-filter, the QR-dec is computed in qr-applyFilter()
      if (_filter != PostProcessing::QR2FILTER) {
        for (int i = 0; i < (int) _matrixV_RSLS.cols(); i++) {
          Eigen::VectorXd v = _matrixV_RSLS.col(i);
          qr.pushBack(v); // same order as matrix V_RSLS
        }
      }

      // apply filter
      if (_filter != PostProcessing::NOFILTER) {
        std::vector<int> delIndices(0);
        qr.applyFilter(_singularityLimit, delIndices, _matrixV_RSLS);
        // start with largest index (as V,W matrices are shrinked and shifted
        for (int i = delIndices.size() - 1; i >= 0; i--) {
          removeMatrixColumnRSLS(delIndices[i]);
        }
        assertion(_matrixV_RSLS.cols() == qr.cols(), _matrixV_RSLS.cols(), qr.cols());
      }

      /**
      *   computation of pseudo inverse matrix Z = (V^TV)^-1 * V^T as solution
      *   to the equation R*z = Q^T(i) for all columns i,  via back substitution.
      */
      auto            Q = qr.matrixQ();
      auto            R = qr.matrixR();
      Eigen::MatrixXd pseudoInverse(qr.cols(), qr.rows());
      Eigen::VectorXd yVec(pseudoInverse.rows());

      // backsubstitution
      for (int i = 0; i < Q.rows(); i++) {
        Eigen::VectorXd Qrow = Q.row(i);
        yVec                 = R.triangularView<Eigen::Upper>().solve<Eigen::OnTheLeft>(Qrow);
        pseudoInverse.col(i) = yVec;
      }

      // scale pseudo inverse back Z := Z' * P,
      // Z' is scaled pseudo inverse i.e, Z' = R^-1 * Q^T * P^-1
      //_preconditioner->apply(pseudoInverse, true, false);

      // store factorization of least-squares initial guess for Jacobian
      _WtilChunk.push_back(_matrixW_RSLS);
      _pseudoInverseChunk.push_back(pseudoInverse);

      // |= REVERT PRECONDITIONING  J_prev = Wtil^0, Z^0  ==|
      _preconditioner->revert(_WtilChunk.front());
      _preconditioner->apply(_pseudoInverseChunk.front(), true);
      _preconditioner->revert(_matrixW_RSLS);
      _preconditioner->revert(_matrixV_RSLS);
      // |===================                             ==|
    }

    DEBUG("MVJ-RESTART, mode=LS. Restart with " << _matrixV_RSLS.cols() << " columns from " << _RSLSreusedTimesteps << " time steps.");
    if (utils::MasterSlave::_masterMode || (not utils::MasterSlave::_masterMode && not utils::MasterSlave::_slaveMode))
      _infostringstream << " - MVJ-RESTART" << _nbRestarts << ", mode= LS -\n  used cols: " << _matrixV_RSLS.cols() << "\n  R_RS: " << _RSLSreusedTimesteps << "\n"
                        << std::endl;

    //            ------------ RESTART ZERO ------------
  } else if (_imvjRestartType == MVQNPostProcessing::RS_ZERO) {
    // drop all stored Wtil^q, Z^q matrices
    _WtilChunk.clear();
    _pseudoInverseChunk.clear();

    DEBUG("MVJ-RESTART, mode=Zero");

  } else if (_imvjRestartType == MVQNPostProcessing::RS_SLIDE) {

    // re-compute Wtil -- compensate for dropping of Wtil_0 ond Z_0:
    //                    Wtil_q <-- Wtil_q +  Wtil^0 * (Z^0*V_q)
    for (int i = (int) _WtilChunk.size() - 1; i >= 1; i--) {

      int colsLSSystemBackThen = _pseudoInverseChunk.front().rows();
      assertion(colsLSSystemBackThen == _WtilChunk.front().cols(), colsLSSystemBackThen, _WtilChunk.front().cols());
      Eigen::MatrixXd ZV = Eigen::MatrixXd::Zero(colsLSSystemBackThen, _qrV.cols());
      // multiply: ZV := Z^q * V of size (m x m) with m=#cols, stored on each proc.
      _parMatrixOps->multiply(_pseudoInverseChunk.front(), _matrixV, ZV, colsLSSystemBackThen, getLSSystemRows(), _qrV.cols());
      // multiply: Wtil^0 * (Z_0*V)  dimensions: (n x m) * (m x m), fully local and embarrassingly parallel
      Eigen::MatrixXd tmp = Eigen::MatrixXd::Zero(_qrV.rows(), _qrV.cols());
      tmp                 = _WtilChunk.front() * ZV;
      _WtilChunk[i] += tmp;

      // drop oldest pair Wtil_0 and Z_0
      assertion(not _WtilChunk.empty());
      assertion(not _pseudoInverseChunk.empty())
          _WtilChunk.erase(_WtilChunk.begin());
      _pseudoInverseChunk.erase(_pseudoInverseChunk.begin());
    }

  } else if (_imvjRestartType == MVQNPostProcessing::NO_RESTART) {
    assertion(false); // should not happen, in this case _imvjRestart=false
  } else {
    assertion(false);
  }
}

// ==================================================================================
void MVQNPostProcessing::specializedIterationsConverged(
    DataMap &cplData)
{
  TRACE();
  
  // truncate V_RSLS and W_RSLS matrices according to _RSLSreusedTimesteps
  if (_imvjRestartType == RS_LS) {
    if (_matrixCols_RSLS.front() == 0) { // Did only one iteration
      _matrixCols_RSLS.pop_front();
    }
    if (_RSLSreusedTimesteps == 0) {
      _matrixV_RSLS.resize(0, 0);
      _matrixW_RSLS.resize(0, 0);
      _matrixCols_RSLS.clear();
    } else if ((int) _matrixCols_RSLS.size() > _RSLSreusedTimesteps) {
      int toRemove = _matrixCols_RSLS.back();
      assertion(toRemove > 0, toRemove);
      if (_matrixV_RSLS.size() > 0) {
        assertion(_matrixV_RSLS.cols() > toRemove, _matrixV_RSLS.cols(), toRemove);
      }
      
      // remove columns
      for (int i = 0; i < toRemove; i++) {
        utils::removeColumnFromMatrix(_matrixV_RSLS, _matrixV_RSLS.cols() - 1);
        utils::removeColumnFromMatrix(_matrixW_RSLS, _matrixW_RSLS.cols() - 1);
      }
      _matrixCols_RSLS.pop_back();
    }
    _matrixCols_RSLS.push_front(0);
  }

  //_info2<<std::endl;

  // if efficient update of imvj is enabled
  if (not _alwaysBuildJacobian || _imvjRestart) {
    // need to apply the preconditioner, as all data structures are reverted after
    // call to computeQNUpdate. Need to call this before the preconditioner is updated.

    // |= REBUILD QR-dec if needed     ============|
    // apply scaling to V, V' := P * V (only needed to reset the QR-dec of V)
    _preconditioner->apply(_matrixV);

    if (_preconditioner->requireNewQR()) {
      if (not(_filter == PostProcessing::QR2FILTER)) { //for QR2 filter, there is no need to do this twice
        _qrV.reset(_matrixV, getLSSystemRows());
      }
      _preconditioner->newQRfulfilled();
    }
    // apply the configured filter to the LS system
    // as it changed in BaseQNPostProcessing::iterationsConverged()
    BaseQNPostProcessing::applyFilter();
    _preconditioner->revert(_matrixV);
    // |===================          ============|

    //              ------- RESTART/ JACOBIAN ASSEMBLY -------
    if (_imvjRestart) {

      // add the matrices Wtil and Z of the converged configuration to the storage containers
      Eigen::MatrixXd Z(_qrV.cols(), _qrV.rows());
      // compute pseudo inverse using QR factorization and back-substitution
      // also compensates for the scaling of V, i.e.,
      // reverts Z' = R^-1 * Q^T * P^-1 as Z := Z' * P
      pseudoInverse(Z);

      // push back unscaled pseudo Inverse, Wtil is also unscaled.
      // all objects in Wtil chunk and Z chunk are NOT PRECONDITIONED
      _WtilChunk.push_back(_Wtil);
      _pseudoInverseChunk.push_back(Z);

      /**
       *  Restart the IMVJ according to restart type
       */
      if ((int) _WtilChunk.size() >= _chunkSize + 1) {

        // < RESTART >
        _nbRestarts++;
        restartIMVJ();
      }

      // only in imvj normal mode with efficient update:
    } else {

      // compute explicit representation of Jacobian
      buildJacobian();
    }

    /** in case of enforced initial relaxation, the matrices are cleared
     *  in case of timestepsReused > 0, the columns in _Wtil are outdated, as the Jacobian changes, hence clear
     *  in case of timestepsReused == 0 and no initial relaxation, pending deletion in performPostProcessing
     */
    if (_timestepsReused > 0 || (_timestepsReused == 0 && _forceInitialRelaxation)) {
      //_Wtil.conservativeResize(0, 0);
      _resetLS = true;
    }
  }

  // only store Jacobian if imvj is in normal mode, i.e., the Jacobian is build
  if (not _imvjRestart) {
    // store inverse Jacobian from converged time step. NOT SCALED with preconditioner
    _oldInvJacobian = _invJacobian;
  }
}

// ==================================================================================
void MVQNPostProcessing::removeMatrixColumn(
    int columnIndex)
{
  TRACE(columnIndex, _matrixV.cols());
  assertion(_matrixV.cols() > 1, _matrixV.cols());
  assertion(_Wtil.cols() > 1);

  // remove column from matrix _Wtil
  if (not _resetLS && not _alwaysBuildJacobian)
    utils::removeColumnFromMatrix(_Wtil, columnIndex);

  BaseQNPostProcessing::removeMatrixColumn(columnIndex);
}

// ==================================================================================
void MVQNPostProcessing::removeMatrixColumnRSLS(
    int columnIndex)
{
  TRACE(columnIndex, _matrixV_RSLS.cols());
  assertion(_matrixV_RSLS.cols() > 1);

  utils::removeColumnFromMatrix(_matrixV_RSLS, columnIndex);
  utils::removeColumnFromMatrix(_matrixW_RSLS, columnIndex);

  // Reduce column count
  std::deque<int>::iterator iter = _matrixCols_RSLS.begin();
  int                       cols = 0;
  while (iter != _matrixCols_RSLS.end()) {
    cols += *iter;
    if (cols > columnIndex) {
      assertion(*iter > 0);
      *iter -= 1;
      if (*iter == 0) {
        _matrixCols_RSLS.erase(iter);
      }
      break;
    }
    iter++;
  }
}
}
}
} // namespace precice, cplscheme, impl

#endif
