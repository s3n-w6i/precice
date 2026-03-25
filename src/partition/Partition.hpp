#pragma once

#include <string>
#include <vector>
#include "com/SerializedConnectionInfo.hpp"
#include "logging/Logger.hpp"
#include "m2n/M2N.hpp"
#include "m2n/SharedPointer.hpp"
#include "m2n/config/M2NConfiguration.hpp"
#include "mapping/SharedPointer.hpp"
#include "mesh/SharedPointer.hpp"

// ----------------------------------------------------------- CLASS DEFINITION

namespace precice::partition {

/**
 * @brief Abstract base class for partitions.
 *
 * A Partition describes how a mesh is decomposed among multiple ranks and
 * is associated to a "provide-mesh" or "receive-mesh" a participant holds. This class holds the
 * structures that describe the decomposition (if not the mesh) and compute
 * them.
 *
 * A Partition can come in two flavors: Either defined by a participant
 * (provide-mesh) or received from another participant (receive-mesh).
 *
 * Access to the associated mesh, to both mappings (from and to this mesh),
 * and to an m2n communication to another participant is necessary.
 */
class Partition {
public:
  /// Constructor.
  Partition(mesh::PtrMesh mesh);

  Partition &operator=(Partition &&) = delete;

  virtual ~Partition() = default;

  /// Intersections between bounding boxes around each rank are computed
  virtual void compareBoundingBoxes(std::string participantName, std::map<std::string, com::serialize::SerializedConnectionInfoMap::ConnectionInfoMap>* connectionInfos) = 0;

  /// The mesh is communicated between both primary ranks (if required)
  virtual void communicate() = 0;

  /// The partition is computed, i.e. the mesh re-partitioned if required and all data structures are set up.
  virtual void compute() = 0;

  void addFromMapping(mapping::PtrMapping fromMapping)
  {
    _fromMappings.push_back(std::move(fromMapping));
  }

  void addToMapping(mapping::PtrMapping toMapping)
  {
    _toMappings.push_back(std::move(toMapping));
  }

  void addM2N(m2n::M2NConfiguration::ConfiguredM2N m2n)
  {
    _m2ns.push_back(m2n);
  }

  bool usesTwoLevelInitialization()
  {
    //@todo treatment of multiple m2ns
    return _m2ns.size() == 1 && _m2ns[0].m2n->usesTwoLevelInitialization();
  }

protected:
  mesh::PtrMesh _mesh;

  std::vector<mapping::PtrMapping> _fromMappings;

  std::vector<mapping::PtrMapping> _toMappings;

  /// m2n connection to each connected participant
  std::vector<m2n::M2NConfiguration::ConfiguredM2N> _m2ns;

private:
  logging::Logger _log{"partition::Partition"};
};

} // namespace precice::partition
