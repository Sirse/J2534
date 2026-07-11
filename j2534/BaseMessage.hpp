#pragma once

#include "J2534_v0404.h"

#include <cstdint>
#include <vector>

namespace j2534 {

class BaseMessage {
public:
  explicit BaseMessage(uint32_t canId) : _canId{canId} {}
  virtual ~BaseMessage() = default;

protected:
  BaseMessage(const BaseMessage &) = default;
  BaseMessage(BaseMessage &&) = default;
  BaseMessage &operator=(const BaseMessage &rhs) {
    _canId = rhs._canId;
    return *this;
  }

  BaseMessage &operator=(BaseMessage &&) = default;

public:
  virtual std::vector<PASSTHRU_MSG>
  toPassThruMsgs(unsigned long ProtocolID, unsigned long Flags) const = 0;

  uint32_t getCanId() const { return _canId; }

private:
  uint32_t _canId;
};

} // namespace j2534
