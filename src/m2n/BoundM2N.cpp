#include <memory>

#include <boost/optional/optional.hpp>
#include "com/Communication.hpp"
#include "com/Extra.hpp"
#include "com/SharedPointer.hpp"
#include "logging/LogMacros.hpp"
#include "m2n/BoundM2N.hpp"
#include "m2n/M2N.hpp"
#include "precice/impl/Types.hpp"
#include "profiling/Event.hpp"
#include "utils/IntraComm.hpp"
#include "utils/assertion.hpp"

using precice::profiling::Event;

namespace precice::m2n {

void BoundM2N::prepareEstablishment()
{
  if (isRequesting) {
    return;
  }

  m2n->prepareEstablishment(localName, remoteName);
}

void BoundM2N::connectPrimaryRanks(std::string_view configHash)
{
  std::string fullLocalName = localName;

  if (isRequesting) {
    m2n->requestPrimaryRankConnection(remoteName, fullLocalName, configHash);
  } else {
    m2n->acceptPrimaryRankConnection(fullLocalName, remoteName, configHash);
  }
}

void BoundM2N::connectSecondaryRanks()
{
  if (m2n->usesTwoLevelInitialization()) {
    PRECICE_DEBUG("Update secondary connections");
    m2n->completeSecondaryRanksConnection();
  } else {
    if (isRequesting) {
      PRECICE_DEBUG("Awaiting secondary connections from {}", remoteName);
      m2n->requestSecondaryRanksConnection(remoteName, localName);
      PRECICE_DEBUG("Established secondary connections from {}", remoteName);
    } else {
      PRECICE_DEBUG("Establishing secondary connections to {}", remoteName);
      m2n->acceptSecondaryRanksConnection(localName, remoteName);
      PRECICE_DEBUG("Established  secondary connections to {}", remoteName);
    }
  }
}

com::serialize::SerializedConnectionInfoMap::ConnectionInfoMap BoundM2N::preparePreConnectSecondaryRanks()
{
  PRECICE_ASSERT(m2n->usesTwoLevelInitialization());
  PRECICE_WARN("Two-level initialization is still in beta testing. Several edge cases are known to fail. Please report problems nevertheless.");
  Event e("bound-m2n.preparePreConnectSecondaryRanks");

  std::map<Rank, std::string> connectionInfoMap;

  // Accepting side (set up, gather connection info, communicate to requesting side)
  if (isRequesting) {
    // Set up accepting side
    PRECICE_DEBUG("Setting up preliminary secondary connections from {}", localName);
    std::string connectionInfo = m2n->prepareAcceptSecondaryRanksPreConnection(localName, remoteName);
    PRECICE_DEBUG("Set up preliminary secondary connections from {}. Ready for establishing connections.", localName);

    PRECICE_TRACE("Connection information is: {}", connectionInfo);

    // Gather connection info and communicate it
    if (utils::IntraComm::isSecondary()) {
      Event e1("bound-m2n.gatherSendConnectionInfo");
      com::sendConnectionInfo(*utils::IntraComm::getCommunication(), 0, connectionInfo);
      e1.stop();
    } else { // Primary
      // Gather connection info
      Event e1("bound-m2n.gatherConnectionInfoMap");

      // Store the primary rank's connection info as well
      connectionInfoMap.emplace(0, connectionInfo);

      for (Rank secondaryRank : utils::IntraComm::allSecondaryRanks()) {
        connectionInfoMap.emplace(secondaryRank, "");

        Event e2("bound-m2n.gatherReceiveConnectionInfo");
        com::receiveConnectionInfo(*utils::IntraComm::getCommunication(), secondaryRank, connectionInfoMap.at(secondaryRank));
        e2.stop();

        PRECICE_DEBUG("Updated connection information map after receiving from {}: {}", secondaryRank, connectionInfoMap);
      }

      e1.stop();
    }
  }

  return connectionInfoMap;
}

void BoundM2N::finishPreConnectSecondaryRanks(com::serialize::SerializedConnectionInfoMap::ConnectionInfoMap connectionInfoMap)
{
  PRECICE_TRACE(connectionInfoMap);
  Event e("bound-m2n.finishPreConnectSecondaryRanks");

  // Accepting side (set up, gather connection info, communicate to requesting side)
  if (isRequesting) {
    PRECICE_DEBUG("Awaiting preliminary secondary connections from {}", remoteName);
    m2n->finishAcceptSecondaryRanksPreConnection(localName, remoteName);
    PRECICE_DEBUG("Established preliminary secondary connections from {}", remoteName);
  } else {
    // Connect to the accepting side
    PRECICE_DEBUG("Establishing preliminary secondary connections to {}", remoteName);
    m2n->requestSecondaryRanksPreConnection(remoteName, localName, connectionInfoMap);
    PRECICE_DEBUG("Established preliminary secondary connections to {}", remoteName);
  }
}

void BoundM2N::cleanupEstablishment()
{
  if (isRequesting) {
    return;
  }
  waitForSecondaryRanks();
  if (!utils::IntraComm::isSecondary()) {
    m2n->cleanupEstablishment(localName, remoteName);
  }
}

void BoundM2N::waitForSecondaryRanks()
{
  if (utils::IntraComm::isPrimary()) {
    for (Rank rank : utils::IntraComm::allSecondaryRanks()) {
      int item = 0;
      utils::IntraComm::getCommunication()->receive(item, rank);
      PRECICE_ASSERT(item > 0);
    }
  }
  if (utils::IntraComm::isSecondary()) {
    int item = utils::IntraComm::getRank();
    utils::IntraComm::getCommunication()->send(item, 0);
  }
}

} // namespace precice::m2n
