// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <cstdint>
#include <memory>
#include <string>
namespace port {
// Transport is deliberately independent of ROMs, SDL and the machine. Each step
// exchanges only one player's held buttons and the previous confirmed state hash.
class NetSession {
public:
    enum class Result { Waiting, Ready, Failed };
    NetSession();
    ~NetSession();
    NetSession(const NetSession&) = delete;
    NetSession& operator=(const NetSession&) = delete;
    bool Open(const std::string& endpoint, bool host, const std::string& identity,
              unsigned timeoutMs, const std::string& logPath, std::string& error);
    Result Poll(uint64_t frame, uint16_t local, uint32_t hash, uint16_t pads[2], bool finish = false);
    const std::string& Error() const;
    const std::string& Status() const;
    int Player() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};
// Same executable required; computed once, before starting the transport.
std::string NetExecutableHash(std::string& error);
}
