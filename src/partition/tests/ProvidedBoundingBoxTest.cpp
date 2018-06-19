//#ifndef PRECICE_NO_MPI
#include "testing/Testing.hpp"
#include "testing/Fixtures.hpp"

#include "partition/ProvidedBoundingBox.hpp"
#include "partition/ReceivedBoundingBox.hpp"

#include "utils/Parallel.hpp"
#include "com/MPIDirectCommunication.hpp"
#include "m2n/M2N.hpp"
#include "utils/MasterSlave.hpp"
#include "m2n/GatherScatterComFactory.hpp"

using namespace precice;
using namespace partition;

BOOST_AUTO_TEST_SUITE(PartitionTests)
BOOST_AUTO_TEST_SUITE(ProvidedBoundingBoxTests)

void setupParallelEnvironment(m2n::PtrM2N m2n){
  assertion(utils::Parallel::getCommunicatorSize() == 4);

  com::PtrCommunication masterSlaveCom =
        com::PtrCommunication(new com::MPIDirectCommunication());
  utils::MasterSlave::_communication = masterSlaveCom;

  utils::Parallel::synchronizeProcesses();

  if (utils::Parallel::getProcessRank() == 0){ //NASTIN
    utils::Parallel::splitCommunicator( "Fluid" );
    m2n->acceptMasterConnection ( "Fluid", "SolidMaster");
    utils::MasterSlave::_slaveMode = false;
    utils::MasterSlave::_masterMode = false;
  }
  else if(utils::Parallel::getProcessRank() == 1){//Master
    utils::Parallel::splitCommunicator( "SolidMaster" );
    m2n->requestMasterConnection ( "Fluid", "SolidMaster" );
    utils::MasterSlave::_rank = 0;
    utils::MasterSlave::_size = 3;
    utils::MasterSlave::_slaveMode = false;
    utils::MasterSlave::_masterMode = true;
  }
  else if(utils::Parallel::getProcessRank() == 2){//Slave1
    utils::Parallel::splitCommunicator( "SolidSlaves");
    utils::MasterSlave::_rank = 1;
    utils::MasterSlave::_size = 3;
    utils::MasterSlave::_slaveMode = true;
    utils::MasterSlave::_masterMode = false;
  }
  else if(utils::Parallel::getProcessRank() == 3){//Slave2
    utils::Parallel::splitCommunicator( "SolidSlaves");
    utils::MasterSlave::_rank = 2;
    utils::MasterSlave::_size = 3;
    utils::MasterSlave::_slaveMode = true;
    utils::MasterSlave::_masterMode = false;
  }

  if(utils::Parallel::getProcessRank() == 1){//Master
    masterSlaveCom->acceptConnection ( "SolidMaster", "SolidSlaves");
    masterSlaveCom->setRankOffset(1);
  }
  else if(utils::Parallel::getProcessRank() == 2){//Slave1
    masterSlaveCom->requestConnection( "SolidMaster", "SolidSlaves", 0, 2 );
  }
  else if(utils::Parallel::getProcessRank() == 3){//Slave2
    masterSlaveCom->requestConnection( "SolidMaster", "SolidSlaves", 1, 2 );
  }
}

void tearDownParallelEnvironment(){
  utils::MasterSlave::_communication = nullptr;
  utils::MasterSlave::reset();
  utils::Parallel::synchronizeProcesses();
  utils::Parallel::clearGroups();
  mesh::Mesh::resetGeometryIDsGlobally();
  mesh::Data::resetDataCount();
  utils::Parallel::setGlobalCommunicator(utils::Parallel::getCommunicatorWorld());
}


BOOST_AUTO_TEST_CASE(TestProvidedBoundingBox, * testing::OnSize(4))
{
  com::PtrCommunication participantCom =
      com::PtrCommunication(new com::MPIDirectCommunication());
  m2n::DistributedComFactory::SharedPointer distrFactory = m2n::DistributedComFactory::SharedPointer(
      new m2n::GatherScatterComFactory(participantCom));
  m2n::PtrM2N m2n = m2n::PtrM2N(new m2n::M2N(participantCom, distrFactory));

  setupParallelEnvironment(m2n);

  int dimensions = 2;
  bool flipNormals = true;

  if (utils::Parallel::getProcessRank() == 0){ //NASTIN
    mesh::PtrMesh pSolidzMesh(new mesh::Mesh("SolidzMesh", dimensions, flipNormals));    
    double safetyFactor = 0.1;
    Eigen::VectorXd position(dimensions);
    position << 0.0, 0.0;
    mesh::Vertex& v1 = pSolidzMesh->createVertex(position);
    position << 0.0, 1.5;
    mesh::Vertex& v2 = pSolidzMesh->createVertex(position);
    pSolidzMesh->createEdge(v1,v2); 

   
    mesh::Mesh::BoundingBoxMap received_globalBB;    
    mesh::Mesh::BoundingBox localBB;

    for (int i=0; i < 3; i++) {
      for (int j=0; j < dimensions; j++) {
        localBB.push_back(std::make_pair(-1,-1));
      }
      received_globalBB[i]=localBB;
      localBB.clear();
    }
    

    ReceivedBoundingBox part(pSolidzMesh, safetyFactor, received_globalBB);
    part.setm2n(m2n);
    part.communicate();
    for (auto &rank : received_globalBB) {
     BOOST_TEST(rank.second!=localBB); 
    }    

//   BOOST_TEST(pSolidzMesh->vertices().size()==6);
//    BOOST_TEST(pSolidzMesh->edges().size()==4);

  }
  else{//SOLIDZ
    mesh::PtrMesh pSolidzMesh(new mesh::Mesh("SolidzMesh", dimensions, flipNormals));

    if(utils::Parallel::getProcessRank() == 1){//Master
      Eigen::VectorXd position(dimensions);
      position << 0.0, 0.0;
      mesh::Vertex& v1 = pSolidzMesh->createVertex(position);
      position << 0.0, 1.5;
      mesh::Vertex& v2 = pSolidzMesh->createVertex(position);
      pSolidzMesh->createEdge(v1,v2);      
    }
    else if(utils::Parallel::getProcessRank() == 2){//Slave1
      Eigen::VectorXd position(dimensions);
      position << 0.0, 3.5;
      mesh::Vertex& v3 = pSolidzMesh->createVertex(position);
      position << 0.0, 4.5;
      mesh::Vertex& v4 = pSolidzMesh->createVertex(position);
      pSolidzMesh->createEdge(v3,v4);
    }
    else if(utils::Parallel::getProcessRank() == 3){//Slave2
      Eigen::VectorXd position(dimensions);
      position << 0.0, 5.5;
      mesh::Vertex& v5 = pSolidzMesh->createVertex(position);
      position << 0.0, 7.0;
      mesh::Vertex& v6 = pSolidzMesh->createVertex(position); 
      pSolidzMesh->createEdge(v5,v6);
    }

    double safetyFactor = 0.1;
    bool hasToSend = true;
    mesh::Mesh::BoundingBoxMap send_globalBB;
    mesh::Mesh::BoundingBox localBB;

    for (int i=0; i < 3; i++) {
      for (int j=0; j < dimensions; j++) {
        localBB.push_back(std::make_pair(-1,-1));
      }
      send_globalBB[i]=localBB;
      localBB.clear();
    }
    
    ProvidedBoundingBox part(pSolidzMesh, hasToSend, safetyFactor, send_globalBB);
    part.setm2n(m2n);
    part.communicate();
    // part.compute();

    BOOST_TEST(part._globalBB.size()==3);
    
   }
  tearDownParallelEnvironment();
}

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
//#endif // PRECICE_NO_MPI
