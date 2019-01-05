#pragma once
#ifndef PRECICE_NO_SOCKETS

#include "Request.hpp"

#include <condition_variable>
#include <mutex>

namespace precice
{
namespace com
{
class SocketRequest : public Request
{
public:
  SocketRequest();

  void complete();

  bool test() override;

  void wait() override;

private:
  bool _complete;

  std::condition_variable _completeCondition;
  std::mutex              _completeMutex;
};
} // namespace com
} // namespace precice

#endif // not PRECICE_NO_SOCKETS
