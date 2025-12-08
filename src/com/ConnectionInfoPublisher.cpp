#include <algorithm>
#include <boost/algorithm/string/trim.hpp>
#include <boost/uuid/name_generator.hpp>
#include <boost/uuid/string_generator.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <libmemcached/memcached.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <thread>

#include "com/ConnectionInfoPublisher.hpp"
#include "logging/LogMacros.hpp"
#include "precice/impl/Types.hpp"
#include "utils/Hash.hpp"
#include "utils/assertion.hpp"

#include <profiling/Event.hpp>

using precice::profiling::Event;

namespace fs = std::filesystem;
namespace precice::com {

std::string impl::hashedFilePath(std::string_view acceptorName, std::string_view requesterName, std::string_view tag, Rank rank)
{
  constexpr int     firstLevelLen = 2;
  std::string const s             = std::string(acceptorName).append(tag).append(requesterName).append(std::to_string(rank));
  std::string       hash          = utils::preciceHash(s);

  auto p = fs::path(hash.substr(0, firstLevelLen)) / hash.substr(firstLevelLen);

  return p.string();
}

std::string impl::localDirectory(std::string_view acceptorName, std::string_view requesterName, std::string_view addressDirectory)
{
  std::string directional = std::string(acceptorName).append("-").append(requesterName);

  auto p = fs::path(addressDirectory.begin(), addressDirectory.end()) / "precice-run" / directional;

  return p.string();
}

std::string ConnectionInfoPublisher::getLocalDirectory() const
{
  return impl::localDirectory(acceptorName, requesterName, addressDirectory);
}

std::string ConnectionInfoPublisher::getFilename() const
{
  auto local  = getLocalDirectory();
  auto hashed = impl::hashedFilePath(acceptorName, requesterName, tag, rank);
  auto p      = fs::path(getLocalDirectory()) / hashed;

  return p.string();
}

std::string ConnectionInfoReader::read() const
{
  Event e("ConnectionInfoReader.read");
  Event e1("ConnectionInfoReader.read.createMemcachedClient");

  auto addr = "127.0.0.1";
  auto port = 11211;

  auto          config = fmt::format("--SERVER={}:{}", addr, port);
  memcached_st *memc   = memcached(config.data(), config.length());

  e1.stop();
  Event e2("ConnectionInfoReader.read.getValue");

  auto key = fmt::format("{}-{}-{}-{}", acceptorName, requesterName, tag, rank);

  // Retrieve the value
  size_t           value_length;
  uint32_t         flags;
  memcached_return rc;
  char *           retrieved_value;

  do {
    retrieved_value = memcached_get(memc, &key.front(), key.length(), &value_length, &flags, &rc);
  } while (rc == MEMCACHED_NOTFOUND);

  PRECICE_CHECK(rc == MEMCACHED_SUCCESS, "Failed to read key {} from memcached server {}:{}. {}", key, addr, port, memcached_strerror(memc, rc));

  e2.stop();
  Event e3("ConnectionInfoReader.read.free");

  std::string value(retrieved_value, value_length);
  PRECICE_WARN("S SUCCESS {}", retrieved_value);
  free(retrieved_value);
  memcached_free(memc);

  e3.stop();

  return value;
}

ConnectionInfoWriter::~ConnectionInfoWriter()
{
  Event e("ConnectionInfoWriter.init");
  Event e1("ConnectionInfoWriter.init.createMemcachedClient");

  auto addr = "127.0.0.1";
  auto port = 11211;

  auto          config = fmt::format("--SERVER={}:{}", addr, port);
  memcached_st *memc   = memcached(config.data(), config.length());

  e1.stop();
  Event e2("ConnectionInfoWriter.init.deletePreExistingValue");

  auto key = fmt::format("{}-{}-{}-{}", acceptorName, requesterName, tag, rank);

  auto rc = memcached_delete(memc, &key.front(), key.length(), (time_t) 0);
  PRECICE_WARN_IF(rc != MEMCACHED_SUCCESS, "Failed to delete key {} from memcached server {}:{}. {}", key, addr, port, memcached_strerror(memc, rc));

  e2.stop();
  Event e3("ConnectionInfoWriter.init.freeMemcachedClient");

  memcached_free(memc);

  e3.stop();
}

void ConnectionInfoWriter::write(std::string_view info) const
{
  Event e("ConnectionInfoWriter.write");
  Event e1("ConnectionInfoWriter.write.createMemcachedClient");

  auto addr = "127.0.0.1";
  auto port = 11211;

  auto          config = fmt::format("--SERVER={}:{}", addr, port);
  memcached_st *memc   = memcached(config.data(), config.length());

  e1.stop();
  Event e2("ConnectionInfoWriter.write.addValue");

  auto key = fmt::format("{}-{}-{}-{}", acceptorName, requesterName, tag, rank);
  auto rc  = memcached_add(memc, &key.front(), key.length(), &info.front(), info.length(), (time_t) 0, (uint32_t) 0);
  PRECICE_CHECK(rc == MEMCACHED_SUCCESS, "Failed to add key {} to memcached server {}:{}. {}", key, addr, port, memcached_strerror(memc, rc));

  e2.stop();
  Event e3("ConnectionInfoWriter.write.freeMemcachedClient");

  memcached_free(memc);

  e3.stop();
}

} // namespace precice::com
