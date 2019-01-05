#include "cplscheme/ParallelCouplingScheme.hpp"
#include "cplscheme/config/CouplingSchemeConfiguration.hpp"
#include "cplscheme/config/PostProcessingConfiguration.hpp"
#include "cplscheme/impl/ConvergenceMeasure.hpp"
#include "cplscheme/impl/AbsoluteConvergenceMeasure.hpp"
#include "cplscheme/impl/MinIterationConvergenceMeasure.hpp"
#include "cplscheme/impl/IQNILSPostProcessing.hpp"
#include "cplscheme/impl/MVQNPostProcessing.hpp"
#include "cplscheme/impl/BaseQNPostProcessing.hpp"
#include "cplscheme/impl/ConstantPreconditioner.hpp"
#include "cplscheme/SharedPointer.hpp"
#include "cplscheme/impl/SharedPointer.hpp"
#include "cplscheme/Constants.hpp"
#include "mesh/Mesh.hpp"
#include "mesh/SharedPointer.hpp"
#include "mesh/Vertex.hpp"
#include "mesh/config/DataConfiguration.hpp"
#include "mesh/config/MeshConfiguration.hpp"
#include "com/MPIDirectCommunication.hpp"
#include "m2n/GatherScatterCommunication.hpp"
#include "m2n/config/M2NConfiguration.hpp"
#include "m2n/M2N.hpp"
#include "xml/XMLTag.hpp"
#include <Eigen/Core>
#include <string>
#include "utils/EigenHelperFunctions.hpp"

#include "testing/Testing.hpp"
#include "testing/Fixtures.hpp"

using namespace precice;
using namespace precice::cplscheme;

BOOST_AUTO_TEST_SUITE(CplSchemeTests)

struct ParallelImplicitCouplingSchemeFixture
{
  using DataMap = std::map<int,PtrCouplingData>;

  std::string _pathToTests;

  ParallelImplicitCouplingSchemeFixture(){
    _pathToTests = testing::getPathToSources() + "/cplscheme/tests/";
  }
};

BOOST_FIXTURE_TEST_SUITE(ParallelImplicitCouplingSchemeTests, ParallelImplicitCouplingSchemeFixture)

#ifndef PRECICE_NO_MPI

BOOST_AUTO_TEST_CASE(testParseConfigurationWithRelaxation)
{
  using namespace mesh;

  std::string path(_pathToTests + "parallel-implicit-cplscheme-relax-const-config.xml");

  xml::XMLTag root = xml::getRootTag();
  PtrDataConfiguration dataConfig(new DataConfiguration(root));
  dataConfig->setDimensions(3);
  PtrMeshConfiguration meshConfig(new MeshConfiguration(root, dataConfig));
  meshConfig->setDimensions(3);
  m2n::M2NConfiguration::SharedPointer m2nConfig(
      new m2n::M2NConfiguration(root));
  CouplingSchemeConfiguration cplSchemeConfig(root, meshConfig, m2nConfig);

  xml::configure(root, path);
  BOOST_CHECK(cplSchemeConfig._postProcConfig->getPostProcessing().get());
  meshConfig->setMeshSubIDs();
}

BOOST_AUTO_TEST_CASE(testMVQNPP)
{
  //use two vectors and see if underrelaxation works
  double initialRelaxation = 0.01;
  int    maxIterationsUsed = 50;
  int    timestepsReused = 6;
  int    reusedTimestepsAtRestart = 0;
  int    chunkSize = 0;
  int filter = cplscheme::impl::PostProcessing::QR1FILTER;
  int restartType = cplscheme::impl::MVQNPostProcessing::NO_RESTART;
  double singularityLimit = 1e-10;
  double svdTruncationEps = 0.0;
  bool enforceInitialRelaxation = false;
  bool alwaysBuildJacobian = false;
  std::vector<int> dataIDs;
  dataIDs.push_back(0);
  dataIDs.push_back(1);
  std::vector<double> factors;
  factors.resize(2,1.0);
  cplscheme::impl::PtrPreconditioner prec(new cplscheme::impl::ConstantPreconditioner(factors));
  mesh::PtrMesh dummyMesh ( new mesh::Mesh("DummyMesh", 3, false) );


  cplscheme::impl::MVQNPostProcessing pp(initialRelaxation, enforceInitialRelaxation, maxIterationsUsed,
      timestepsReused, filter, singularityLimit, dataIDs, prec, alwaysBuildJacobian,
      restartType, chunkSize, reusedTimestepsAtRestart, svdTruncationEps);

  Eigen::VectorXd dvalues;
  Eigen::VectorXd dcol1;
  Eigen::VectorXd fvalues;
  Eigen::VectorXd fcol1;

  //init displacements
  utils::append(dvalues, 1.0);
  utils::append(dvalues, 2.0);
  utils::append(dvalues, 3.0);
  utils::append(dvalues, 4.0);

  utils::append(dcol1, 1.0);
  utils::append(dcol1, 1.0);
  utils::append(dcol1, 1.0);
  utils::append(dcol1, 1.0);

  PtrCouplingData dpcd(new CouplingData(&dvalues,dummyMesh,false,1));

  //init forces
  utils::append(fvalues, 0.1);
  utils::append(fvalues, 0.1);
  utils::append(fvalues, 0.1);
  utils::append(fvalues, 0.1);

  utils::append(fcol1, 0.2);
  utils::append(fcol1, 0.2);
  utils::append(fcol1, 0.2);
  utils::append(fcol1, 0.2);

  PtrCouplingData fpcd(new CouplingData(&fvalues,dummyMesh,false,1));

  DataMap data;
  data.insert(std::pair<int,PtrCouplingData>(0,dpcd));
  data.insert(std::pair<int,PtrCouplingData>(1,fpcd));

  pp.initialize(data);

  dpcd->oldValues.col(0) = dcol1;
  fpcd->oldValues.col(0) = fcol1;

  pp.performPostProcessing(data);

  BOOST_TEST(testing::equals((*data.at(0)->values)(0), 1.00000000000000000000));
  BOOST_TEST(testing::equals((*data.at(0)->values)(1), 1.01000000000000000888));
  BOOST_TEST(testing::equals((*data.at(0)->values)(2), 1.02000000000000001776));
  BOOST_TEST(testing::equals((*data.at(0)->values)(3), 1.03000000000000002665));
  BOOST_TEST(testing::equals((*data.at(1)->values)(0), 0.199000000000000010214));
  BOOST_TEST(testing::equals((*data.at(1)->values)(1), 0.199000000000000010214));
  BOOST_TEST(testing::equals((*data.at(1)->values)(2), 0.199000000000000010214));
  BOOST_TEST(testing::equals((*data.at(1)->values)(3), 0.199000000000000010214));

  Eigen::VectorXd newdvalues;
  utils::append(newdvalues, 10.0);
  utils::append(newdvalues, 10.0);
  utils::append(newdvalues, 10.0);
  utils::append(newdvalues, 10.0);

  data.begin()->second->values = &newdvalues;

  pp.performPostProcessing(data);

  BOOST_TEST(testing::equals((*data.at(0)->values)(0), -5.63401340929695848558e-01));
  BOOST_TEST(testing::equals((*data.at(0)->values)(1), 6.10309919173602111186e-01));
  BOOST_TEST(testing::equals((*data.at(0)->values)(2), 1.78402117927690184729e+00));
  BOOST_TEST(testing::equals((*data.at(0)->values)(3), 2.95773243938020247157e+00));
  BOOST_TEST(testing::equals((*data.at(1)->values)(0), 8.28025852497733250157e-02));
  BOOST_TEST(testing::equals((*data.at(1)->values)(1), 8.28025852497733250157e-02));
  BOOST_TEST(testing::equals((*data.at(1)->values)(2), 8.28025852497733250157e-02));
  BOOST_TEST(testing::equals((*data.at(1)->values)(3), 8.28025852497733250157e-02));
}

BOOST_AUTO_TEST_CASE(testVIQNPP)
{
  //use two vectors and see if underrelaxation works

  double initialRelaxation = 0.01;
  int    maxIterationsUsed = 50;
  int    timestepsReused = 6;
  int filter = cplscheme::impl::BaseQNPostProcessing::QR1FILTER;
  double singularityLimit = 1e-10;
  bool enforceInitialRelaxation = false;
  std::vector<int> dataIDs;
  dataIDs.push_back(0);
  dataIDs.push_back(1);
  std::vector<double> factors;
  factors.resize(2,1.0);
  cplscheme::impl::PtrPreconditioner prec(new cplscheme::impl::ConstantPreconditioner(factors));

  std::map<int, double> scalings;
  scalings.insert(std::make_pair(0,1.0));
  scalings.insert(std::make_pair(1,1.0));
  mesh::PtrMesh dummyMesh ( new mesh::Mesh("DummyMesh", 3, false) );

  cplscheme::impl::IQNILSPostProcessing pp(initialRelaxation, enforceInitialRelaxation, maxIterationsUsed,
      timestepsReused, filter, singularityLimit, dataIDs, prec);


  Eigen::VectorXd dvalues;
  Eigen::VectorXd dcol1;
  Eigen::VectorXd fvalues;
  Eigen::VectorXd fcol1;

  //init displacements
  utils::append(dvalues, 1.0);
  utils::append(dvalues, 2.0);
  utils::append(dvalues, 3.0);
  utils::append(dvalues, 4.0);

  utils::append(dcol1, 1.0);
  utils::append(dcol1, 1.0);
  utils::append(dcol1, 1.0);
  utils::append(dcol1, 1.0);

  PtrCouplingData dpcd(new CouplingData(&dvalues,dummyMesh,false,1));

  //init forces
  utils::append(fvalues, 0.1);
  utils::append(fvalues, 0.1);
  utils::append(fvalues, 0.1);
  utils::append(fvalues, 0.1);

  utils::append(fcol1, 0.2);
  utils::append(fcol1, 0.2);
  utils::append(fcol1, 0.2);
  utils::append(fcol1, 0.2);

  PtrCouplingData fpcd(new CouplingData(&fvalues,dummyMesh,false,1));

  DataMap data;
  data.insert(std::pair<int,PtrCouplingData>(0,dpcd));
  data.insert(std::pair<int,PtrCouplingData>(1,fpcd));

  pp.initialize(data);

  dpcd->oldValues.col(0) = dcol1;
  fpcd->oldValues.col(0) = fcol1;

  pp.performPostProcessing(data);

  BOOST_TEST(testing::equals((*data.at(0)->values)(0), 1.00));
  BOOST_TEST(testing::equals((*data.at(0)->values)(1), 1.01));
  BOOST_TEST(testing::equals((*data.at(0)->values)(2), 1.02));
  BOOST_TEST(testing::equals((*data.at(0)->values)(3), 1.03));
  BOOST_TEST(testing::equals((*data.at(1)->values)(0), 0.199));
  BOOST_TEST(testing::equals((*data.at(1)->values)(1), 0.199));
  BOOST_TEST(testing::equals((*data.at(1)->values)(2), 0.199));
  BOOST_TEST(testing::equals((*data.at(1)->values)(3), 0.199));

  Eigen::VectorXd newdvalues;
  utils::append(newdvalues, 10.0);
  utils::append(newdvalues, 10.0);
  utils::append(newdvalues, 10.0);
  utils::append(newdvalues, 10.0);
  data.begin()->second->values = &newdvalues;

  pp.performPostProcessing(data);

  BOOST_TEST(testing::equals((*data.at(0)->values)(0), -5.63401340929692295845e-01));
  BOOST_TEST(testing::equals((*data.at(0)->values)(1), 6.10309919173607440257e-01));
  BOOST_TEST(testing::equals((*data.at(0)->values)(2), 1.78402117927690717636e+00));
  BOOST_TEST(testing::equals((*data.at(0)->values)(3), 2.95773243938020513610e+00));
  BOOST_TEST(testing::equals((*data.at(1)->values)(0), 8.28025852497733944046e-02));
  BOOST_TEST(testing::equals((*data.at(1)->values)(1), 8.28025852497733944046e-02));
  BOOST_TEST(testing::equals((*data.at(1)->values)(2), 8.28025852497733944046e-02));
  BOOST_TEST(testing::equals((*data.at(1)->values)(3), 8.28025852497733944046e-02));
}

/// Test that runs on 2 processors.
BOOST_FIXTURE_TEST_CASE(testInitializeData, testing::M2NFixture,
		              * testing::MinRanks(2)
                      * boost::unit_test::fixture<testing::MPICommRestrictFixture>(std::vector<int>({0, 1})))
{
  if (utils::Parallel::getCommunicatorSize() != 2) // only run test on ranks {0,1}, for other ranks return
    return;

  xml::XMLTag root = xml::getRootTag();

  // Create a data configuration, to simplify configuration of data
  mesh::PtrDataConfiguration dataConfig(new mesh::DataConfiguration(root));
  dataConfig->setDimensions(3);
  dataConfig->addData("Data0", 1);
  dataConfig->addData("Data1", 3);

  mesh::MeshConfiguration meshConfig(root, dataConfig);
  meshConfig.setDimensions(3);
  mesh::PtrMesh mesh(new mesh::Mesh("Mesh", 3, false));
  const auto dataID0 = mesh->createData("Data0", 1)->getID();
  const auto dataID1 = mesh->createData("Data1", 3)->getID();
  mesh->createVertex(Eigen::Vector3d::Zero());
  mesh->allocateDataValues();
  meshConfig.addMesh(mesh);

  // Create all parameters necessary to create a ParallelImplicitCouplingScheme object
  double maxTime = 1.0;
  int maxTimesteps = 3;
  double timestepLength = 0.1;
  std::string nameParticipant0("Participant0");
  std::string nameParticipant1("Participant1");
  std::string nameLocalParticipant("");
  int sendDataIndex = -1;
  int receiveDataIndex = -1;
  bool initData = false;
  if (utils::Parallel::getProcessRank() == 0){
    nameLocalParticipant = nameParticipant0;
    sendDataIndex = 0;
    receiveDataIndex = 1;
    initData = true;
  }
  else if (utils::Parallel::getProcessRank() == 1){
    nameLocalParticipant = nameParticipant1;
    sendDataIndex = 1;
    receiveDataIndex = 0;
    initData = true;
  }

  // Create the coupling scheme object
  ParallelCouplingScheme cplScheme(
      maxTime, maxTimesteps, timestepLength, 16, nameParticipant0, nameParticipant1,
      nameLocalParticipant, m2n, constants::FIXED_DT, BaseCouplingScheme::Implicit, 100);
  cplScheme.addDataToSend(mesh->data()[sendDataIndex], mesh, initData);
  cplScheme.addDataToReceive(mesh->data()[receiveDataIndex], mesh, initData);

  // Add convergence measures
  int minIterations = 3;
  cplscheme::impl::PtrConvergenceMeasure minIterationConvMeasure1 (
      new cplscheme::impl::MinIterationConvergenceMeasure(minIterations) );
  cplscheme::impl::PtrConvergenceMeasure minIterationConvMeasure2 (
      new cplscheme::impl::MinIterationConvergenceMeasure(minIterations) );
  cplScheme.addConvergenceMeasure (
      mesh->data()[1]->getID(), false, false, minIterationConvMeasure1 );
  cplScheme.addConvergenceMeasure (
      mesh->data()[0]->getID(), false, false, minIterationConvMeasure2 );

  std::string writeIterationCheckpoint(constants::actionWriteIterationCheckpoint());
  std::string readIterationCheckpoint(constants::actionReadIterationCheckpoint());

  cplScheme.initialize(0.0, 0);

  if (nameLocalParticipant == nameParticipant0){
    BOOST_TEST(cplScheme.isActionRequired(constants::actionWriteInitialData()));
    mesh->data(dataID0)->values() = Eigen::VectorXd::Constant(1, 4.0);
    cplScheme.performedAction(constants::actionWriteInitialData());
    cplScheme.initializeData();
    BOOST_TEST(cplScheme.hasDataBeenExchanged());
    auto& values = mesh->data(dataID1)->values();
    BOOST_TEST(testing::equals(values, Eigen::Vector3d(1.0, 2.0, 3.0)), values);

    while (cplScheme.isCouplingOngoing()){
      if (cplScheme.isActionRequired(writeIterationCheckpoint)){
        cplScheme.performedAction(writeIterationCheckpoint);
      }
      if (cplScheme.isActionRequired(readIterationCheckpoint)){
        cplScheme.performedAction(readIterationCheckpoint);
      }
      cplScheme.addComputedTime(timestepLength);
      cplScheme.advance();
    }
  }
  else {
    assertion(nameLocalParticipant == nameParticipant1);
    auto& values = mesh->data(dataID0)->values();
    BOOST_TEST(cplScheme.isActionRequired(constants::actionWriteInitialData()));
    Eigen::VectorXd v(3); v << 1.0, 2.0, 3.0;
    mesh->data(dataID1)->values() = v;
    cplScheme.performedAction(constants::actionWriteInitialData());
    BOOST_TEST(testing::equals(values(0), 0.0), values);
    cplScheme.initializeData();
    BOOST_TEST(cplScheme.hasDataBeenExchanged());
    BOOST_TEST(testing::equals(values(0), 4.0), values);

    while (cplScheme.isCouplingOngoing()){
      if (cplScheme.isActionRequired(writeIterationCheckpoint)){
        cplScheme.performedAction(writeIterationCheckpoint);
      }
      cplScheme.addComputedTime(timestepLength);
      cplScheme.advance();
      if (cplScheme.isActionRequired(readIterationCheckpoint)){
        cplScheme.performedAction(readIterationCheckpoint);
      }
    }
  }
  cplScheme.finalize();
  utils::Parallel::clearGroups();
}
# endif // not PRECICE_NO_MPI

BOOST_AUTO_TEST_SUITE_END()
BOOST_AUTO_TEST_SUITE_END()
