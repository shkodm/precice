#include "TXTTableWriter.hpp"
#include "utils/Helpers.hpp"
#include "utils/assertion.hpp"
#include <iomanip>

namespace precice {
namespace io {

TXTTableWriter:: TXTTableWriter
(
  const std::string& filename )
:
  _data (),
  _writeIterator ( _data.end() ),
  _outputStream ()
{
  _outputStream.open ( filename.c_str() );
  if ( not _outputStream ) {
    ERROR("Could not open file \"" << filename
                   << "\" for writing txt table data!" );
  }
  _outputStream.setf ( std::ios::showpoint );
  _outputStream.setf ( std::ios::fixed );
  _outputStream << std::setprecision(16);
}

TXTTableWriter:: ~TXTTableWriter ()
{
  if ( _outputStream.is_open() ) {
    _outputStream.close ();
  }
}

void TXTTableWriter:: addData
(
  const std::string& name,
  DataType           type )
{
  Data data;
  data.name = name;
  data.type = type;
  assertion ( not utils::contained(data, _data), data.name, data.type );
  _data.push_back ( data );
  assertion ( _outputStream.is_open() );
  if ( (type == INT) || (type == DOUBLE) ) {
    _outputStream << name << "  ";
  }
  else if ( type == VECTOR2D ){
    for ( int i=0; i < 2; i++ ) {
      _outputStream << name << i << "  ";
    }
  }
  else {
    assertion ( type == VECTOR3D );
    for ( int i=0; i < 3; i++ ) {
      _outputStream << name << i << "  ";
    }
  }
  _writeIterator = _data.end();
}

void TXTTableWriter:: writeData
(
  const std::string& name,
  int                value )
{
  assertion ( not _data.empty() );
  if ( _writeIterator == _data.end() ) {
    _writeIterator = _data.begin();
    _outputStream << "\n";
  }
  assertion ( _writeIterator->name == name, _writeIterator->name, name );
  assertion ( _writeIterator->type == INT, _writeIterator->type );
  _outputStream << value << "  ";
  _writeIterator ++;
  if ( _writeIterator == _data.end() ) {
    _outputStream.flush ();
  }
}

void TXTTableWriter:: writeData
(
  const std::string& name,
  double             value )
{
  assertion ( not _data.empty() );
  if ( _writeIterator == _data.end() ) {
    _writeIterator = _data.begin();
    _outputStream << "\n";
  }
  assertion ( _writeIterator->name == name, _writeIterator->name, name );
  assertion ( _writeIterator->type == DOUBLE, _writeIterator->type );
  _outputStream << value << "  ";
  _writeIterator ++;
  if ( _writeIterator == _data.end() ) {
    _outputStream.flush ();
  }
}

void TXTTableWriter:: writeData
(
  const std::string&     name,
  const Eigen::Vector2d& value )
{
  assertion ( not _data.empty() );
  if ( _writeIterator == _data.end() ) {
    _writeIterator = _data.begin();
    _outputStream << "\n";
  }
  assertion ( _writeIterator->name == name, _writeIterator->name, name );
  assertion ( _writeIterator->type == VECTOR2D, _writeIterator->type );
  for ( int i=0; i < value.size(); i++ ) {
    _outputStream << value[i] << "  ";
  }
  _writeIterator ++;
  if ( _writeIterator == _data.end() ) {
    _outputStream.flush ();
  }
}

void TXTTableWriter:: writeData
(
  const std::string&     name,
  const Eigen::Vector3d& value )
{










































































































  assertion ( not _data.empty() );
  if ( _writeIterator == _data.end() ) {
    _writeIterator = _data.begin();
    _outputStream << "\n";
  }
  assertion ( _writeIterator->name == name, _writeIterator->name, name );
  assertion ( _writeIterator->type == VECTOR3D, _writeIterator->type );
  for ( int i=0; i < value.size(); i++ ) {
    _outputStream << value[i] << "  ";
  }
  _writeIterator ++;
  if ( _writeIterator == _data.end() ) {
    _outputStream.flush ();
  }
}

void TXTTableWriter:: close ()
{
  assertion ( _outputStream.is_open() );
  _outputStream.close ();
}

}} // namespace precice, io
