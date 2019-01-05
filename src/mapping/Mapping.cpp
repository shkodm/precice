#include "Mapping.hpp"

namespace precice {
namespace mapping {

Mapping:: Mapping
(
  Constraint      constraint,
  int             dimensions)
:
  _constraint(constraint),
  _inputRequirement(UNDEFINED),
  _outputRequirement(UNDEFINED),
  _input(),
  _output(),
  _dimensions(dimensions)
{}

void Mapping:: setMeshes
(
  const mesh::PtrMesh& input,
  const mesh::PtrMesh& output )
{
  _input = input;
  _output = output;
}

const mesh::PtrMesh& Mapping:: getInputMesh() const {
  return _input;
}

const mesh::PtrMesh& Mapping:: getOutputMesh() const {
  return _output;
}

Mapping::Constraint Mapping:: getConstraint() const
{
  return _constraint;
}

Mapping::MeshRequirement Mapping:: getInputRequirement() const
{
  return _inputRequirement;
}

Mapping::MeshRequirement Mapping:: getOutputRequirement() const
{
  return _outputRequirement;
}

mesh::PtrMesh Mapping:: input() const
{
  return _input;
}

mesh::PtrMesh Mapping:: output() const
{
  return _output;
}

void Mapping:: setInputRequirement
(
  MeshRequirement requirement )
{
  _inputRequirement = requirement;
}

void Mapping:: setOutputRequirement
(
  MeshRequirement requirement )
{
  _outputRequirement = requirement;
}

int Mapping:: getDimensions() const
{
  return _dimensions;
}

bool operator<(Mapping::MeshRequirement lhs, Mapping::MeshRequirement rhs) {
    switch(lhs) {
        case(Mapping::MeshRequirement::UNDEFINED):
            return rhs != Mapping::MeshRequirement::UNDEFINED;
        case(Mapping::MeshRequirement::VERTEX):
            return rhs == Mapping::MeshRequirement::FULL;
        case(Mapping::MeshRequirement::FULL):
                return false;
    };
}

}} // namespace precice, mapping

