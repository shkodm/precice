#pragma once

#include "MeshHandle.hpp"
#include "Constants.hpp"
#include <string>
#include <vector>
#include <set>
#include <memory>

/**
 * Pre-declarations.
 */
namespace precice {
  namespace impl {
    class SolverInterfaceImpl;
  }
}

// Forward declaration to friend the boost test struct
namespace PreciceTests {
  namespace Parallel {
    struct TestFinalize;
    struct TestMasterSlaveSetup;
    struct GlobalRBFPartitioning;
    struct LocalRBFPartitioning;
    struct TestQN;
    struct testDistributedCommunications;
    struct CouplingOnLine;
  }
  namespace Serial {
    struct TestExplicit;
    struct TestConfiguration;
    struct testExplicitWithSubcycling;
    struct testExplicitWithDataExchange;
    struct testExplicitWithDataInitialization;
    struct testExplicitWithBlockDataExchange;
    struct testExplicitWithSolverGeometry;
    struct testExplicitWithDisplacingGeometry;
    struct testExplicitWithDataScaling;
    struct testImplicit;
    struct testStationaryMappingWithSolverMesh;
    struct testBug;
    struct testThreeSolvers;
    struct testMultiCoupling;
  }
  namespace Server {
    struct testCouplingModeWithOneServer;
    struct testCouplingModeParallelWithOneServer;
  }
}

// ----------------------------------------------------------- CLASS DEFINITION

namespace precice {

/**
 * @brief Interface to be used by solvers when using preCICE.
 *
 * A solver, i.e. simulation program, using preCICE has to use SolverInterface such
 *
 * -# Create an object of SolverInterface with SolverInterface()
 * -# Configure the object with SolverInterface::configure()
 * -# Initialize preCICE with SolverInterface::initialize()
 * -# Advance to the next (time)step with SolverInterface::advance()
 * -# Finalize preCICE with SolverInterface::finalize()
 */
class SolverInterface
{
public:

    ///@name Construction and Configuration
    ///@{

  /**
   * @brief Constructor.
   *
   * @param[in] participantName Name of the participant using the interface. Has to
   *        match the name given for a participant in the xml configuration file.
   * @param[in] solverProcessIndex If the solver code runs with several processes,
   *        each process using preCICE has specify its index, which has to start
   *        from 0 and end with solverProcessSize - 1.
   * @param[in] solverProcessSize The number of solver processes using preCICE.
   */
  SolverInterface (
    const std::string& participantName,
    int                solverProcessIndex,
    int                solverProcessSize );

  /**
   * @brief Destructor.
   */
  ~SolverInterface();

  /**
   * @brief Configures preCICE from the given xml file.
   *
   * Only after the configuration a usable state of a SolverInterface
   * object is achieved. However, most of the functionalities in preCICE can be
   * used only after initialize() has been called. Some actions, e.g. specifying
   * the solvers interface mesh, have to be done before initialize is called.
   *
   * In configure, the following is done:
   * - The XML configuration for preCICE is parsed and all objects containing
   *   data are created, but not necessarily filled with data.
   * - If a server is used, communication to that server is established.
   *
   * @param[in] configurationFileName Name (with path) of the xml configuration file to be read.
   */
  void configure ( const std::string& configurationFileName );

  ///@}

  /// @name Steering Methods
  ///@{

  /**
   * @brief Fully initializes preCICE to be used.
   *
   * @pre configure() has been called successfully.
   *
   * @post Communication to the coupling partner/s is setup.
   * @post Meshes are are sent/received to/from coupling partners and the parallel partitions are created.
   * @post If the solver is not starting the simulation, coupling data is received
   * from the coupling partner's first computation.
   * @post The length limitation of the first solver timestep is computed and returned.
   *
   * @return Maximum length of first timestep to be computed by the solver.
   */
  double initialize();

  /**
   * @brief Initializes coupling data.
   *
   * When in a coupled simulation an implicit coupling scheme is used, the
   * starting values for the coupling data are assumed to be zero by default. If
   * this is not the desired behavior, this method can be used to specify values
   * different from zero using the write data methods. Only the first participant
   * of the coupled simulation has to call this method, the second participant
   * receives the values on calling initialize(). For parallel coupling, values in
   * both directions are exchanged. Both participants need to call initializeData then.
   *
   * @pre initialize() has been called successfully.
   * @pre The coupling data to be set is written.
   * @pre advance() has not yet been called.
   * @pre finalize() has not yet been called.
   *
   * @post Initial coupling data values are sent to the coupling partner.
   * @post Written coupling data values are reset to zero, in order to allow writing 
   * of values of first computed timestep.
   */
  void initializeData();

  /**
   * @brief Advances preCICE after the solver has computed one timestep.
   * @param[in] computedTimestepLength Length of timestep computed by solver.
   *
   *
   * @pre initialize() has been called successfully.
   * @pre The solver calling advance() has computed one timestep.
   * @pre The solver has read and written all coupling data.
   * @pre The solver has the length of the timestep used available to be passed to preCICE.
   * @pre finalize() has not yet been called.
   *
   * @post Coupling data values specified to be exchanged in the configuration are
   *   exchanged with coupling partner/s.
   * @post Coupling scheme state (computed time, computed timesteps, ...) is updated.
   * @post For staggered coupling schemes, the coupling partner has computed one
   *   timestep/iteration with the coupling data written by this participant
   *   available.
   * @post The coupling state is printed.
   * @post Meshes with data are exported to files if specified in the configuration.
   * @post The length limitation of the next solver timestep is computed and returned.
   *
   * @return Maximum length of next timestep to be computed by solver.
   */
  double advance ( double computedTimestepLength );

  /**
   * @brief Finalizes preCICE.
   *
   * @pre initialize() has been called successfully.
   * @pre isCouplingOngoing() has returned false.
   *
   * @post Communication channels are closed.
   * @post Meshes and data are deallocated
   */
  void finalize();

  ///@}
  
  ///@name Status Queries
  ///@{

  /**
   * @brief Returns the number of spatial dimensions configured.
   *
   * Currently, two and three dimensional problems can be solved using preCICE.
   * The dimension is specified in the XML configuration.
   *
   * @pre configure() has been called successfully.
   */
  int getDimensions() const;

  /**
   * @brief Returns true, if the coupled simulation is still ongoing.
   *
   * @pre initialize() has been called successfully.
   */
  bool isCouplingOngoing();

  /**
   * @brief Returns true, if new data to be read is available.
   *
   * Data is classified to be new, if it has been received while calling
   * initialize() and before calling advance(), or in the last call of advance().
   * This is always true, if a participant does not make use of subcycling, i.e.
   * choosing smaller timesteps than the limits returned in intitialize() and
   * advance().
   *
   * @pre initialize() has been called successfully.
   */
  bool isReadDataAvailable();

  /**
   * @brief Returns true, if new data has to be written before calling advance().
   *
   * This is always true, if a participant does not make use of subcycling, i.e.
   * choosing smaller timesteps than the limits returned in intitialize() and
   * advance().
   *
   * @pre initialize() has been called successfully.
   */
  bool isWriteDataRequired ( double computedTimestepLength );

  /**
   * @brief Returns true, if the current coupling timestep is completed.
   *
   * The following reasons require several solver time steps per coupling time
   * step:
   * - A solver chooses to perform subcycling.
   * - An implicit coupling timestep iteration is not yet converged.
   *
   * @pre initialize() has been called successfully.
   */
  bool isTimestepComplete();

  /**
   * @brief Returns whether the solver has to evaluate the surrogate model representation
   *        It does not automatically imply, that the solver does not have to evaluate the
   *        fine model representation
   */
  bool hasToEvaluateSurrogateModel();

  /**
   * @brief Returns whether the solver has to evaluate the fine model representation
   *        It does not automatically imply, that the solver does not have to evaluate the
   *        surrogate model representation
   */
  bool hasToEvaluateFineModel();

  ///@}

  ///@name Action Methods
  ///@{

  /**
   * @brief Returns true, if provided name of action is required.
   *
   * Some features of preCICE require a solver to perform specific actions, in
   * order to be in valid state for a coupled simulation. A solver is made
   * eligible to use those features, by querying for the required actions,
   * performing them on demand, and calling fulfilledAction() to signalize
   * preCICE the correct behavior of the solver.
   */
  bool isActionRequired ( const std::string& action );

  /**
   * @brief Tells preCICE that a required action has been fulfilled by a solver.
   *
   * For more details see method requireAction().
   */
  void fulfilledAction ( const std::string& action );

  ///@}

  ///@name Mesh Access
  ///@{

  /**
   * @brief Resets mesh with given ID.
   *
   * Has to be called, everytime the positions for data to be mapped
   * changes. Only has an effect, if the mapping used is non-stationary and
   * non-incremental.
   */
//  void resetMesh ( int meshID );

  /**
   * @brief Returns true, if the mesh with given name is used.
   */
  bool hasMesh ( const std::string& meshName ) const;

  /**
   * @brief Returns the ID belonging to the mesh with given name.
   *
   * The existing names are determined from the configuration.
   */
  int getMeshID ( const std::string& meshName );

  /**
   * @brief Returns all mesh IDs (besides sub-ids).
   */
  std::set<int> getMeshIDs();

  /// Returns a handle to a created mesh.
  MeshHandle getMeshHandle ( const std::string& meshName );

  /**
   * @brief Sets position of surface mesh vertex, returns ID.
   */
  int setMeshVertex (
    int           meshID,
    const double* position );

  /**
   * @brief Returns the number of vertices of a mesh.
   */
  int getMeshVertexSize(int meshID);

  /**
   * @brief Sets several spatial vertex positions.
   *
   * Pre-conditions:
   * - A not incremental write-mapping is configured for the mesh with given
   *   meshID.
   * Post-conditions:
   * - If no write mapping is configured, the ids are not changed.
   * - If a (not incremental) write mapping is configured, the ids are filled.
   *
   * @param[in] meshID ID of mesh on which the vertices live
   * @param[in] size Number of vertices
   * @param[in] positions Positions of vertics, Format is (d0x, d0y, d0z, d1x, d1y, d1z, ...., dnx, dny, dnz), 
   *                      where n * the number of vector values. In 2D, the z-components are removed.
   * @param[out] ids IDs for data to be written from given positions.
   */
  void setMeshVertices (
    int     meshID,
    int     size,
    double* positions,
    int*    ids );

  /**
   * @brief Gets spatial vertex positions for given IDs.
   *
   * @param[in] meshID ID of the mesh to retrieve positions from
   * @param[in] size Number of positions and ids
   * @param[in] ids IDs obtained when setting write positions.
   * @param[in] positions Positions corresponding to IDs.
   */
  void getMeshVertices (
    int     meshID,
    int     size,
    int*    ids,
    double* positions );

  /**
   * @brief Gets mesh vertex IDs from positions.
   *
   * @param[in] meshID ID of the mesh to retrieve positions from
   * @param[in] size Number of positions and ids.
   * @param[in] positions Positions (x,y,z,x,y,z,...) to find ids for.
   * @param[in] ids IDs corresponding to positions.
   */
  void getMeshVertexIDsFromPositions (
    int     meshID,
    int     size,
    double* positions,
    int*    ids );

  /**
   * @brief Sets surface mesh edge from vertex IDs, returns edge ID.
   */
  int setMeshEdge (
    int meshID,
    int firstVertexID,
    int secondVertexID );

  /**
   * @brief Sets surface mesh triangle from edge IDs.
   */
  void setMeshTriangle (
    int meshID,
    int firstEdgeID,
    int secondEdgeID,
    int thirdEdgeID );

  /**
   * @brief Sets surface mesh triangle from vertex IDs.
   *
   * This routine is supposed to be used, when no edge information is available
   * per se. Edges are created on the fly within preCICE. This routine is
   * significantly slower than the one using edge IDs, since it needs to check,
   * whether an edge is created already or not.
   */
  void setMeshTriangleWithEdges (
    int meshID,
    int firstVertexID,
    int secondVertexID,
    int thirdVertexID );

  /**
   * @brief Sets surface mesh quadrangle from edge IDs.
   */
  void setMeshQuad (
    int meshID,
    int firstEdgeID,
    int secondEdgeID,
    int thirdEdgeID,
    int fourthEdgeID );

  /**
   * @brief Sets surface mesh quadrangle from vertex IDs.
   *
   * This routine is supposed to be used, when no edge information is available
   * per se. Edges are created on the fly within preCICE. This routine is
   * significantly slower than the one using edge IDs, since it needs to check,
   * whether an edge is created already or not.
   */
  void setMeshQuadWithEdges (
    int meshID,
    int firstVertexID,
    int secondVertexID,
    int thirdVertexID,
    int fourthVertexID );

  ///@}

  ///@name Data Access
  ///@{

  /**
   * @brief Returns true, if the data with given name is used.
   */
  bool hasData ( const std::string& dataName, int meshID ) const;

  /**
   * @brief Returns data id corresponding to the given name (from configuration)
   */
  int getDataID ( const std::string& dataName, int meshID );


  /**
   * @brief Computes and maps all read data mapped to the mesh with given ID.
   *
   */
  void mapReadDataTo ( int toMeshID );

  /**
   * @brief Computes and maps all write data mapped from the mesh with given ID.
   */
  void mapWriteDataFrom ( int fromMeshID );

  /**
   * @brief Writes vector data values given as block.
   *
   * The block must contain the vector values in the following form:
   * values = (d0x, d0y, d0z, d1x, d1y, d1z, ...., dnx, dny, dnz), where n is
   * the number of vector values. In 2D, the z-components are removed.
   *
   * @param[in] dataID ID of the data to be written.
   * @param[in] size Number n of points to be written, not size of array values.
   * @param[in] valueIndices Indizes of vertices, from SolverInterface::setMeshVertex() e.g.
   * @param[in] values Values of the data to be written.
   */
  void writeBlockVectorData (
    int     dataID,
    int     size,
    int*    valueIndices,
    double* values );

  /**
   * @brief Write vectorial data to the interface mesh
   *
   * The exact mapping and communication must be specified in XYZ.
   *
   * @param[in] dataID     ID of the data to be written, e.g. 1 = forces
   * @param[in] valueIndex Position (coordinate, e.g.) of data to be written
   * @param[in] value      Value of the data to be written
   */
  void writeVectorData (
    int           dataID,
    int           valueIndex,
    const double* value );


  /**
   * @brief Writes scalar data values given as block.
   *
   * @param[in] dataID ID of the data to be written.
   * @param[in] size   Number of valueIndices, and number of values.
   * @param[in] values Values of the data to be written.
   */
  void writeBlockScalarData (
    int     dataID,
    int     size,
    int*    valueIndices,
    double* values );

  /**
   * @brief Write scalar data to the interface mesh
   *
   * The exact mapping and communication must be specified in XYZ.
   *
   * @param[in] dataID       ID of the data to be written (2 = temperature, e.g.)
   * @param[in] dataPosition Position (coordinate, e.g.) of data to be written
   * @param[in] dataValue    Value of the data to be written
   */
  void writeScalarData (
    int    dataID,
    int    valueIndex,
    double value );

  /**
   * @brief Reads vector data values given as block.
   *
   * The block contains the vector values in the following form:
   * values = (d0x, d0y, d0z, d1x, d1y, d1z, ...., dnx, dny, dnz), where n is
   * the number of vector values. In 2D, the z-components are removed.
   *
   * @param[in] dataID       ID of the data to be read.
   * @param[in] size         Number n of points to be read, not size of array values.
   * @param[in] valueIndices Indices (from setReadPosition()) of data values.
   * @param[out] values      Values of the data to be read.
   */
  void readBlockVectorData (
    int     dataID,
    int     size,
    int*    valueIndices,
    double* values );

  /**
   * @brief Reads vector data from the coupling mesh.
   *
   * @param[in]  dataID ID of the data to be read, e.g. 1 = forces
   * @param[in]  dataPosition Position (coordinate, e.g.) of data to be read
   * @param[out] dataValue Read data value
   */
  void readVectorData (
    int     dataID,
    int     valueIndex,
    double* value );

  /**
   * @brief Reads scalar data values given as block.
   *
   * @param[in]  dataID ID of the data to be written.
   * @param[in]  size Number of valueIndices, and number of values.
   * @param[out] values Values of the data to be read.
   */
  void readBlockScalarData (
    int     dataID,
    int     size,
    int*    valueIndices,
    double* values );

  /**
   * @brief Read scalar data from the interface mesh.
   *
   * The exact mapping and communication must be specified in XYZ.
   *
   * @param[in]  dataID ID of the data to be read, e.g. 2 = temperatures
   * @param[in]  valueIndex Position (coordinate, e.g.) of data to be read
   * @param[out] Value Read data value
   */
  void readScalarData (
    int     dataID,
    int     valueIndex,
    double& value );

  ///@}

private:

  /// Pointer to implementation of SolverInterface.
  std::unique_ptr<impl::SolverInterfaceImpl> _impl;

  /// Disable copy construction by making copy constructor private.
  SolverInterface ( const SolverInterface& copy );

  /// Disable assignment construction by making assign. constructor private.
  SolverInterface& operator= ( const SolverInterface& assign );

  // @brief To allow white box tests.
  friend struct PreciceTests::Parallel::TestFinalize;
  friend struct PreciceTests::Parallel::TestMasterSlaveSetup;
  friend struct PreciceTests::Parallel::GlobalRBFPartitioning;
  friend struct PreciceTests::Parallel::LocalRBFPartitioning;
  friend struct PreciceTests::Parallel::TestQN;
  friend struct PreciceTests::Parallel::testDistributedCommunications;
  friend struct PreciceTests::Parallel::CouplingOnLine;
  friend struct PreciceTests::Serial::TestExplicit;
  friend struct PreciceTests::Serial::TestConfiguration;
  friend struct PreciceTests::Serial::testExplicitWithSubcycling;
  friend struct PreciceTests::Serial::testExplicitWithDataExchange;
  friend struct PreciceTests::Serial::testExplicitWithDataInitialization;
  friend struct PreciceTests::Serial::testExplicitWithBlockDataExchange;
  friend struct PreciceTests::Serial::testExplicitWithSolverGeometry;
  friend struct PreciceTests::Serial::testExplicitWithDisplacingGeometry;
  friend struct PreciceTests::Serial::testExplicitWithDataScaling;
  friend struct PreciceTests::Serial::testImplicit;
  friend struct PreciceTests::Serial::testStationaryMappingWithSolverMesh;
  friend struct PreciceTests::Serial::testBug;
  friend struct PreciceTests::Serial::testThreeSolvers;
  friend struct PreciceTests::Serial::testMultiCoupling;
  friend struct PreciceTests::Server::testCouplingModeWithOneServer;
  friend struct PreciceTests::Server::testCouplingModeParallelWithOneServer;

};

} // namespace precice
