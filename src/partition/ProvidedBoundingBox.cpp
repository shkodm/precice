#include "partition/ProvidedBoundingBox.hpp"
#include "com/CommunicateBoundingBox.hpp"
#include "com/Communication.hpp"
#include "utils/MasterSlave.hpp"
#include "m2n/M2N.hpp"
#include "utils/EventTimings.hpp"
#include "utils/Parallel.hpp"
#include "mesh/Mesh.hpp"
#include "mapping/Mapping.hpp"
#include "mesh/Mesh.hpp"
#include "mesh/Vertex.hpp"
#include "mesh/Edge.hpp"
#include "mesh/Triangle.hpp"


using precice::utils::Event;

namespace precice {
namespace partition {

logging::Logger ProvidedBoundingBox:: _log ( "precice::partition::ProvidedBoundingBox" );

ProvidedBoundingBox::ProvidedBoundingBox
(mesh::PtrMesh mesh,
 bool hasToSend,
 double safetyFactor,
 mesh::Mesh::BoundingBoxMap globalBB)
:
    Partition (mesh),
    _hasToSend(hasToSend),
    /*_bb(mesh->getDimensions(),std::make_pair(std::numeric_limits<double>::max(), std::numeric_limits<double>::lowest())),*/
     _bb(mesh->getBoundingBox()),
    _dimensions(mesh->getDimensions()),
    _safetyFactor(safetyFactor),
    _globalBB(globalBB)
{}

void ProvidedBoundingBox::communicate()
{

  if (_hasToSend) {

 
  
    Event e1("creat and gather bounding box");

    if (utils::MasterSlave::_slaveMode) {//slave
      com::CommunicateBoundingBox(utils::MasterSlave::_communication).sendBoundingBox(_bb, 0); 
    }
    else{ // Master

      // assertion(utils::MasterSlave::_rank==0);
      //assertion(utils::MasterSlave::_size>1);

       _globalBB[0] = _bb;
       
      if (utils::MasterSlave::_size>1) {  

      for (int rankSlave = 1; rankSlave < utils::MasterSlave::_size; rankSlave++) {
        com::CommunicateBoundingBox(utils::MasterSlave::_communication).receiveBoundingBox(_bb, rankSlave);
        
        DEBUG("From slave " << rankSlave << ", bounding mesh: " << _bb[0].first
                     << ", " << _bb[0].second << " and " << _bb[1].first << ", " << _bb[1].second);
        
        _globalBB[rankSlave] = _bb;
        
      }

      // Now also creat Bounding Box around master vertices      
     
    }
    }
    e1.stop();

    Event e2("send global Bounding Box");
    if (not utils::MasterSlave::_slaveMode) {     
      com::CommunicateBoundingBox(_m2n->getMasterCommunication()).sendBoundingBoxMap(_globalBB,0); 
    }
    e2.stop();
  }

   
}
void ProvidedBoundingBox::compute()
{

    if (utils::MasterSlave::_masterMode) {//Master

      com::CommunicateBoundingBox(_m2n->getMasterCommunication()).receiveFeedbackMap(received_feedbackMap, 0 ); // @Amin: create this method!
      com::CommunicateBoundingBox(utils::MasterSlave::_communication).broadcastSendFeedbackMap(received_feedbackMap);     

      for (auto &other_rank : received_feedbackMap) {
        for (auto &included_ranks: other_rank.second) {
          if (utils::Parallel::getProcessRank() == included_ranks) {
            connected_ranks.push_back(other_rank.first);             
          }
        }
      }
    }
    else{ // Slave

      com::CommunicateBoundingBox(utils::MasterSlave::_communication).broadcastReceiveFeedbackMap(received_feedbackMap);

      for (auto &other_rank : received_feedbackMap) {
        for (auto &included_ranks: other_rank.second) {
          if (utils::Parallel::getProcessRank() == included_ranks) {
            connected_ranks.push_back(other_rank.first);             
          }
        }
      }
    }
}


void ProvidedBoundingBox::createOwnerInformation()
{}

}}

