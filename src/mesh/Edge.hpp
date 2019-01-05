#pragma once

#include <Eigen/Core>
#include <iostream>

#include "mesh/PropertyContainer.hpp"
#include "mesh/Vertex.hpp"
#include "boost/noncopyable.hpp"
#include "math/differences.hpp"

namespace precice {
namespace mesh {

struct ConstEdgeIteratorTypes;
struct EdgeIteratorTypes;
template<typename Types> class EdgeIterator;

/// Linear edge of a mesh, defined by two Vertex objects.
class Edge : public PropertyContainer, private boost::noncopyable
{
public:

  /**
   * @brief Constructor.
   *
   * @param[in] vertexOne First Vertex object defining the edge.
   * @param[in] vertexTwo Second Vertex object defining the edge.
   * @param[in] id Unique (among edges in one mesh) ID.
   */
  Edge (
    Vertex& vertexOne,
    Vertex& vertexTwo,
    int     id );

  /// Destructor, empty.
  virtual ~Edge () {}

  /// Returns number of spatial dimensions (2 or 3) the edge is embedded to.
  int getDimensions() const;

  /// Returns the edge's vertex with index 0 or 1.
  Vertex& vertex ( int i );

  /// Returns the edge's vertex as const object with index 0 or 1.
  const Vertex& vertex ( int i ) const;

  /// Sets the normal of the edge.
  template<typename VECTOR_T>
  void setNormal ( const VECTOR_T& normal );

  /// Returns the (among edges) unique ID of the edge.
  int getID () const;

  /// Returns the normal of the edge.
  const Eigen::VectorXd& getNormal () const;

  /// Returns the center of the edge.
  const Eigen::VectorXd getCenter () const;

  /// Returns the radius of the enclosing circle of the edge.
  double getEnclosingRadius () const;

  /**
   * @brief Compares two Edges for equality
   *
   * Two Edges are equal if their normal vector is equal AND
   * if the two vertices are equal, whereas the order of vertices is NOT important.
   */
  bool operator==(const Edge& other) const;

  /// Not equal, implemented in terms of equal.
  bool operator!=(const Edge& other) const;

private:

  /// Pointers to Vertex objects defining the edge.
  std::array<Vertex*,2> _vertices;

  /// Unique (among edges) ID of the edge.
  int _id;

  /// Normal of the edge.
  Eigen::VectorXd _normal;
};

// ------------------------------------------------------ HEADER IMPLEMENTATION

inline Vertex& Edge:: vertex
(
  int i )
{
  assertion ( (i == 0) || (i == 1), i );
  return *_vertices[i];
}

inline const Vertex& Edge:: vertex
(
  int i ) const
{
  assertion ( (i==0) || (i==1), i );
  return *_vertices[i];
}

inline int Edge:: getDimensions() const
{
  return _vertices[0]->getDimensions();
}

template<typename VECTOR_T>
void Edge:: setNormal
(
  const VECTOR_T& normal )
{
  assertion ( normal.size() == _vertices[0]->getDimensions(), normal,
               _vertices[0]->getDimensions() );
  _normal = normal;
}

inline const Eigen::VectorXd& Edge::getNormal () const
{
  return _normal;
}

std::ostream& operator<<(std::ostream& stream, const Edge& edge);

}} // namespace precice, mesh
