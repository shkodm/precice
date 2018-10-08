#pragma once

#include "mapping/SharedPointer.hpp"
#include "mesh/SharedPointer.hpp"
#include "logging/Logger.hpp"
#include "xml/XMLTag.hpp"
#include <string>
#include <vector>

namespace precice {
namespace mapping {

/// How to handle the polynomial?
/**
 * ON: Include it in the system matrix
 * OFF: Omit it altogether
 * SEPARATE: Compute it separately using least-squares QR.
 */
enum class Polynomial {
  ON,
  OFF,
  SEPARATE
};

enum class Preallocation {
  OFF,
  COMPUTE,
  ESTIMATE,
  SAVE,
  TREE
};


/// Performs XML configuration and holds configured mappings.
class MappingConfiguration : public xml::XMLTag::Listener
{
public:

  /// Constants defining the direction of a mapping.
  enum Direction
  {
    WRITE,
    READ
  };

  enum Timing
  {
    INITIAL,
    ON_ADVANCE,
    ON_DEMAND
  };

  /// Configuration data for one mapping.
  struct ConfiguredMapping
  {
    /// Mapping object.
    PtrMapping mapping;
    /// Remote mesh to map from
    mesh::PtrMesh fromMesh;
    /// Remote mesh to map to
    mesh::PtrMesh toMesh;
    /// Direction of mapping (important to set input and output mesh).
    Direction direction;
    /// When the mapping should be executed.
    Timing timing;
    /// true for RBF mapping
    bool isRBF;
  };

  MappingConfiguration (
    xml::XMLTag&                    parent,
    const mesh::PtrMeshConfiguration& meshConfiguration );

  /**
   * @brief Callback function required for use of automatic configuration.
   *
   * @return True, if successful.
   */
  virtual void xmlTagCallback ( xml::XMLTag& callingTag );

  /**
   * @brief Callback function required for use of automatic configuration.
   *
   * @return True, if successful.
   */
  virtual void xmlEndTagCallback ( xml::XMLTag& callingTag );

  /// Returns all configured mappings.
  const std::vector<ConfiguredMapping>& mappings();

  /// Adds a mapping to the configuration.
  void addMapping (
    const PtrMapping&    mapping,
    const mesh::PtrMesh& fromMesh,
    const mesh::PtrMesh& toMesh,
    Direction            direction,
    Timing               timing );

  void resetMappings() { _mappings.clear(); }

private:

  mutable logging::Logger _log{"config:MappingConfiguration"};

  const std::string TAG = "mapping";

  const std::string ATTR_DIRECTION = "direction";
  const std::string ATTR_FROM = "from";
  const std::string ATTR_TO = "to";
  const std::string ATTR_TIMING = "timing";
  const std::string ATTR_TYPE = "type";
  const std::string ATTR_CONSTRAINT = "constraint";
  const std::string ATTR_SHAPE_PARAM = "shape-parameter";
  const std::string ATTR_SUPPORT_RADIUS = "support-radius";
  const std::string ATTR_SOLVER_RTOL = "solver-rtol";
  const std::string ATTR_X_DEAD = "x-dead";
  const std::string ATTR_Y_DEAD = "y-dead";
  const std::string ATTR_Z_DEAD = "z-dead";

  const std::string VALUE_WRITE = "write";
  const std::string VALUE_READ = "read";
  const std::string VALUE_CONSISTENT = "consistent";
  const std::string VALUE_CONSERVATIVE = "conservative";
  const std::string VALUE_NEAREST_NEIGHBOR;
  const std::string VALUE_NEAREST_PROJECTION;
  const std::string VALUE_RBF_TPS;
  const std::string VALUE_RBF_MULTIQUADRICS;
  const std::string VALUE_RBF_INV_MULTIQUADRICS;
  const std::string VALUE_RBF_VOLUME_SPLINES;
  const std::string VALUE_RBF_GAUSSIAN;
  const std::string VALUE_RBF_CTPS_C2;
  const std::string VALUE_RBF_CPOLYNOMIAL_C0;
  const std::string VALUE_RBF_CPOLYNOMIAL_C6;

  const std::string VALUE_PETRBF_TPS;
  const std::string VALUE_PETRBF_MULTIQUADRICS;
  const std::string VALUE_PETRBF_INV_MULTIQUADRICS;
  const std::string VALUE_PETRBF_VOLUME_SPLINES;
  const std::string VALUE_PETRBF_GAUSSIAN;
  const std::string VALUE_PETRBF_CTPS_C2;
  const std::string VALUE_PETRBF_CPOLYNOMIAL_C0;
  const std::string VALUE_PETRBF_CPOLYNOMIAL_C6;
  
  const std::string VALUE_TIMING_INITIAL = "initial";
  const std::string VALUE_TIMING_ON_ADVANCE = "onadvance";
  const std::string VALUE_TIMING_ON_DEMAND = "ondemand";

  mesh::PtrMeshConfiguration _meshConfig;

  std::vector<ConfiguredMapping> _mappings;

  ConfiguredMapping createMapping (
    const std::string& direction,
    const std::string& type,
    const std::string& constraint,
    const std::string& fromMeshName,
    const std::string& toMeshName,
    Timing             timing,
    double             shapeParameter,
    double             supportRadius,
    double             solverRtol,
    bool               xDead,
    bool               yDead,
    bool               zDead,
    Polynomial         polynomial,
    Preallocation      preallocation) const;

  void checkDuplicates ( const ConfiguredMapping& mapping );

  Timing getTiming(const std::string& timing) const;
};

}} // namespace mapping, config
