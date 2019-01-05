#include "Triangle.hpp"
#include "mesh/Edge.hpp"
#include "mesh/Vertex.hpp"
#include <boost/range/concepts.hpp>

namespace precice
{
namespace mesh
{

BOOST_CONCEPT_ASSERT((boost::RandomAccessIteratorConcept<Triangle::iterator>));
BOOST_CONCEPT_ASSERT((boost::RandomAccessIteratorConcept<Triangle::const_iterator>));
BOOST_CONCEPT_ASSERT((boost::RandomAccessRangeConcept<Triangle>));
BOOST_CONCEPT_ASSERT((boost::RandomAccessRangeConcept<const Triangle>));

Triangle::Triangle(
    Edge &edgeOne,
    Edge &edgeTwo,
    Edge &edgeThree,
    int   id)
    : PropertyContainer(),
      _edges({&edgeOne, &edgeTwo, &edgeThree}),
      _id(id),
      _normal(Eigen::VectorXd::Zero(edgeOne.getDimensions()))
{
  assertion(edgeOne.getDimensions() == edgeTwo.getDimensions(),
            edgeOne.getDimensions(), edgeTwo.getDimensions());
  assertion(edgeTwo.getDimensions() == edgeThree.getDimensions(),
            edgeTwo.getDimensions(), edgeThree.getDimensions());
  assertion(getDimensions() == 3, getDimensions());

  // Determine vertex map
  Vertex &v0 = edge(0).vertex(0);
  Vertex &v1 = edge(0).vertex(1);

  if (&edge(1).vertex(0) == &v0) {
    _vertexMap[0] = 1;
    _vertexMap[1] = 0;
  } else if (&edge(1).vertex(1) == &v0) {
    _vertexMap[0] = 1;
    _vertexMap[1] = 1;
  } else if (&edge(1).vertex(0) == &v1) {
    _vertexMap[0] = 0;
    _vertexMap[1] = 0;
  } else {
    assertion(&edge(1).vertex(1) == &v1);
    _vertexMap[0] = 0;
    _vertexMap[1] = 1;
  }

  if (_vertexMap[1] == 0) {
    if (&edge(2).vertex(0) == &edge(1).vertex(1)) {
      _vertexMap[2] = 0;
    } else {
      assertion(&edge(2).vertex(1) == &edge(1).vertex(1));
      _vertexMap[2] = 1;
    }
  } else if (_vertexMap[1] == 1) {
    if (&edge(2).vertex(0) == &edge(1).vertex(0)) {
      _vertexMap[2] = 0;
    } else {
      assertion(&edge(2).vertex(1) == &edge(1).vertex(0));
      _vertexMap[2] = 1;
    }
  }
  assertion(&edge(0).vertex(_vertexMap[0]) != &edge(1).vertex(_vertexMap[1]));
  assertion(&edge(0).vertex(_vertexMap[0]) != &edge(2).vertex(_vertexMap[2]));
  assertion(&edge(1).vertex(_vertexMap[1]) != &edge(2).vertex(_vertexMap[2]));
  assertion((_vertexMap[0] == 0) || (_vertexMap[0] == 1), _vertexMap[0]);
  assertion((_vertexMap[1] == 0) || (_vertexMap[1] == 1), _vertexMap[0]);
  assertion((_vertexMap[2] == 0) || (_vertexMap[2] == 1), _vertexMap[0]);
}

int Triangle::getDimensions() const
{
  return _edges[0]->getDimensions();
}

const Eigen::VectorXd &Triangle::getNormal() const
{
  return _normal;
}

const Eigen::VectorXd Triangle::getCenter() const
{
  return (_edges[0]->getCenter() +_edges[1]->getCenter() + _edges[2]->getCenter()) / 3.0;
}

double Triangle::getEnclosingRadius() const
{
  auto center = getCenter();
  return std::max({(center - vertex(0).getCoords()).norm(),
                   (center - vertex(1).getCoords()).norm(),
                   (center - vertex(2).getCoords()).norm()});
}

bool Triangle::operator==(const Triangle& other) const
{
    return math::equals(_normal, other._normal) &&
        std::is_permutation(_edges.begin(), _edges.end(), other._edges.begin(),
                [](const Edge* e1, const Edge* e2){return *e1 == *e2;});
}

bool Triangle::operator!=(const Triangle& other) const
{
    return !(*this == other);
}

std::ostream& operator<<(std::ostream& os, const Triangle& t)
{
    os << "POLYGON ((";
    for (int i = 0; i < 3; i++){
        os << t.vertex(i).getCoords().transpose();
        if (i < 2)
            os << ", ";
    }
    return os <<", " << t.vertex(0).getCoords().transpose() << "))";
}

} // namespace mesh
} // namespace precice
