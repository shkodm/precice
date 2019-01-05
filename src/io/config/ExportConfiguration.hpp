#pragma once

#include "io/ExportContext.hpp"
#include "xml/XMLTag.hpp"
#include "logging/Logger.hpp"
#include <string>
#include <list>

namespace precice {
namespace io {

/**
 * @brief Configuration class for exports.
 */
class ExportConfiguration : public xml::XMLTag::Listener
{
public:

  ExportConfiguration ( xml::XMLTag& parent );

  /**
   * @brief Returns the configured export context
   */
  std::list<ExportContext>& exportContexts() { return _contexts; }

  virtual void xmlTagCallback ( xml::XMLTag& callingTag );

  /// Callback from automatic configuration. Not utilitzed here.
  virtual void xmlEndTagCallback ( xml::XMLTag& callingTag ) {}

  void resetExports() { _contexts.clear(); }

private:
  logging::Logger _log{"io::ExportConfiguration"};

  const std::string TAG = "export";

  const std::string ATTR_LOCATION = "directory";
  const std::string ATTR_TYPE = "type";
  const std::string ATTR_AUTO = "auto";
  const std::string VALUE_VTK = "vtk";

  const std::string ATTR_TIMESTEP_INTERVAL = "timestep-interval";
  const std::string ATTR_NEIGHBORS = "neighbors";
  const std::string ATTR_TRIGGER_SOLVER = "trigger-solver";
  const std::string ATTR_NORMALS = "normals";
  const std::string ATTR_EVERY_ITERATION = "every-iteration";

  std::list<ExportContext> _contexts;
};

}} // namespace precice, io
