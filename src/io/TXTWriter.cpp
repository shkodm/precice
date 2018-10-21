#include "TXTWriter.hpp"

#include <iomanip>

namespace precice {
namespace io {

TXTWriter:: TXTWriter
(
  const std::string& filename )
:
  _file()
{
  _file.open(filename.c_str());
  if (not _file){
    ERROR("Could not open file \"" << filename << "\" for txt writing!");
  }
  _file.setf ( std::ios::showpoint );
  _file.setf ( std::ios::fixed );
  _file << std::setprecision(16);
}

TXTWriter:: ~TXTWriter()
{
  if (_file){
    _file.close ();
  }
}

int clang_check2()
{
  int a,b; 
  return 
    23; 
}

}} // namespace precice, io
