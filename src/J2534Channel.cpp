#include "j2534/J2534Channel.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <thread>

namespace j2534 {

namespace {

std::string j2534StatusToString(unsigned long status) {
  switch (status) {
  case STATUS_NOERROR:
    return "STATUS_NOERROR";
  case ERR_NOT_SUPPORTED:
    return "ERR_NOT_SUPPORTED";
  case ERR_INVALID_CHANNEL_ID:
    return "ERR_INVALID_CHANNEL_ID";
  case ERR_INVALID_PROTOCOL_ID:
    return "ERR_INVALID_PROTOCOL_ID";
  case ERR_NULL_PARAMETER:
    return "ERR_NULL_PARAMETER";
  case ERR_INVALID_IOCTL_VALUE:
    return "ERR_INVALID_IOCTL_VALUE";
  case ERR_INVALID_FLAGS:
    return "ERR_INVALID_FLAGS";
  case ERR_FAILED:
    return "ERR_FAILED";
  case ERR_DEVICE_NOT_CONNECTED:
    return "ERR_DEVICE_NOT_CONNECTED";
  case ERR_TIMEOUT:
    return "ERR_TIMEOUT";
  case ERR_INVALID_MSG:
    return "ERR_INVALID_MSG";
  case ERR_INVALID_TIME_INTERVAL:
    return "ERR_INVALID_TIME_INTERVAL";
  case ERR_EXCEEDED_LIMIT:
    return "ERR_EXCEEDED_LIMIT";
  case ERR_INVALID_MSG_ID:
    return "ERR_INVALID_MSG_ID";
  case ERR_DEVICE_IN_USE:
    return "ERR_DEVICE_IN_USE";
  case ERR_INVALID_IOCTL_ID:
    return "ERR_INVALID_IOCTL_ID";
  case ERR_BUFFER_EMPTY:
    return "ERR_BUFFER_EMPTY";
  case ERR_BUFFER_FULL:
    return "ERR_BUFFER_FULL";
  case ERR_BUFFER_OVERFLOW:
    return "ERR_BUFFER_OVERFLOW";
  case ERR_PIN_INVALID:
    return "ERR_PIN_INVALID";
  case ERR_CHANNEL_IN_USE:
    return "ERR_CHANNEL_IN_USE";
  case ERR_MSG_PROTOCOL_ID:
    return "ERR_MSG_PROTOCOL_ID";
  case ERR_INVALID_FILTER_ID:
    return "ERR_INVALID_FILTER_ID";
  case ERR_NO_FLOW_CONTROL:
    return "ERR_NO_FLOW_CONTROL";
  case ERR_NOT_UNIQUE:
    return "ERR_NOT_UNIQUE";
  case ERR_INVALID_BAUDRATE:
    return "ERR_INVALID_BAUDRATE";
  case ERR_INVALID_DEVICE_ID:
    return "ERR_INVALID_DEVICE_ID";
  default:
    std::stringstream ss;
    ss << "J2534_STATUS_0x" << std::uppercase << std::hex << std::setw(2)
       << std::setfill('0') << status;
    return ss.str();
  }
}

std::vector<PASSTHRU_MSG> toPassThruMsgs(const std::vector<uint8_t> &data,
                                         unsigned long protocolId,
                                         unsigned long txFlags) {
  PASSTHRU_MSG msg;
  if (data.size() > sizeof(msg.Data)) {
    throw std::length_error("PASSTHRU_MSG data exceeds 4128 bytes");
  }
  memset(&msg, 0, sizeof(msg));
  msg.ProtocolID = protocolId;
  msg.TxFlags = txFlags;
  msg.DataSize = static_cast<unsigned long>(data.size());
  memcpy(msg.Data, data.data(), data.size());
  return {msg};
}

} // namespace

J2534Channel::J2534Channel(J2534 &j2534, unsigned long ProtocolID,
                           unsigned long Flags, unsigned long Baudrate,
                           unsigned long TxFlags)
    : _j2534{j2534}, _protocolId(ProtocolID), _txFlags(TxFlags),
      _baudrate(Baudrate) {
  const auto result =
      _j2534.PassThruConnect(ProtocolID, Flags, Baudrate, _channelID);
  if (result != STATUS_NOERROR) {
    std::string err;
    _j2534.PassThruGetLastError(err);
    throw std::runtime_error("Can't open channel: " + err);
  }
}

J2534Channel::~J2534Channel() { _j2534.PassThruDisconnect(_channelID); }

J2534_ERROR_CODE J2534Channel::readMsgs(std::vector<PASSTHRU_MSG> &msgs,
                                        unsigned long Timeout) const {
  return _j2534.PassThruReadMsgs(_channelID, msgs, Timeout);
}

J2534_ERROR_CODE J2534Channel::writeMsgs(const std::vector<PASSTHRU_MSG> &msgs,
                                         unsigned long &numMsgs,
                                         unsigned long Timeout) const {
  return _j2534.PassThruWriteMsgs(_channelID, msgs, numMsgs, Timeout);
}

J2534_ERROR_CODE J2534Channel::writeMsgs(const std::vector<BaseMessage *> &msgs,
                                         unsigned long &numMsgs,
                                         unsigned long Timeout) const {
  std::vector<PASSTHRU_MSG> messages;
  for (const auto &msg : msgs) {
    for (auto &passMsg : msg->toPassThruMsgs(_protocolId, _txFlags))
      messages.emplace_back(std::move(passMsg));
  }
  return _j2534.PassThruWriteMsgs(_channelID, messages, numMsgs, Timeout);
}

J2534_ERROR_CODE
J2534Channel::writeMsgs(const BaseMessage &msg, unsigned long &numMsgs,
                        unsigned long Timeout,
                        unsigned long DelayBetweenMessages) const {
  unsigned long numMsg = 0;
  numMsgs = 0;
  for (const auto &passMsg : msg.toPassThruMsgs(_protocolId, _txFlags)) {
    J2534_ERROR_CODE errorCode =
        _j2534.PassThruWriteMsgs(_channelID, {passMsg}, numMsg, Timeout);
    if (errorCode != STATUS_NOERROR) {
      return errorCode;
    }
    numMsgs += numMsg;
    if (DelayBetweenMessages > 0) {
      std::this_thread::sleep_for(
          std::chrono::milliseconds(DelayBetweenMessages));
    }
  }
  return STATUS_NOERROR;
}

J2534_ERROR_CODE J2534Channel::writeMsg(const std::vector<uint8_t> &data,
                                        unsigned long Timeout) const {
  unsigned long numMsgs = 1;
  return _j2534.PassThruWriteMsgs(_channelID,
                                  toPassThruMsgs(data, _protocolId, _txFlags),
                                  numMsgs, Timeout);
}

J2534_ERROR_CODE
J2534Channel::startPeriodicMsg(const PASSTHRU_MSG &msg, unsigned long &msgID,
                               unsigned long TimeInterval) const {
  if (TimeInterval < 5 || TimeInterval > 65535) {
    throw std::invalid_argument(
        "Periodic message interval must be 5..65535 ms");
  }
  return _j2534.PassThruStartPeriodicMsg(_channelID, msg, msgID, TimeInterval);
}

std::vector<unsigned long>
J2534Channel::startPeriodicMsgs(const BaseMessage &msg,
                                unsigned long TimeInterval) const {
  if (TimeInterval < 5 || TimeInterval > 65535) {
    throw std::invalid_argument(
        "Periodic message interval must be 5..65535 ms");
  }
  std::vector<unsigned long> result;
  // Preserve the existing API contract: failures are omitted, so callers
  // may receive a partial list of successfully started message IDs.
  for (const auto &message : msg.toPassThruMsgs(_protocolId, _txFlags)) {
    unsigned long msgID;
    if (_j2534.PassThruStartPeriodicMsg(_channelID, message, msgID,
                                        TimeInterval) == STATUS_NOERROR) {
      result.push_back(msgID);
    }
  }
  return result;
}

J2534_ERROR_CODE J2534Channel::stopPeriodicMsg(unsigned long MsgID) const {
  return _j2534.PassThruStopPeriodicMsg(_channelID, MsgID);
}

void J2534Channel::stopPeriodicMsg(
    const std::vector<unsigned long> &ids) const {
  for (const auto &messageId : ids) {
    _j2534.PassThruStopPeriodicMsg(_channelID, messageId);
  }
}

J2534_ERROR_CODE J2534Channel::startMsgFilter(unsigned long FilterType,
                                              PASSTHRU_MSG *maskMsg,
                                              PASSTHRU_MSG *patternMsg,
                                              PASSTHRU_MSG *flowControlMsg,
                                              unsigned long &msgID) const {
  return _j2534.PassThruStartMsgFilter(_channelID, FilterType, maskMsg,
                                       patternMsg, flowControlMsg, msgID);
}

J2534_ERROR_CODE J2534Channel::stopMsgFilter(unsigned long MsgID) const {
  return _j2534.PassThruStopMsgFilter(_channelID, MsgID);
}

J2534_ERROR_CODE J2534Channel::passThruIoctl(unsigned long IoctlID,
                                             const void *input,
                                             void *output) const {
  return _j2534.PassThruIoctl(_channelID, IoctlID, input, output);
}

J2534_ERROR_CODE J2534Channel::clearRx() const {
  return passThruIoctl(CLEAR_RX_BUFFER, nullptr, nullptr);
}

J2534_ERROR_CODE J2534Channel::clearTx() const {
  return passThruIoctl(CLEAR_TX_BUFFER, nullptr, nullptr);
}

J2534_ERROR_CODE
J2534Channel::setConfig(const std::vector<SCONFIG> &config) const {
  if (config.size() > std::numeric_limits<uint32_t>::max()) {
    throw std::length_error("Too many J2534 configuration parameters");
  }
  SCONFIG_LIST configList;
  configList.NumOfParams = static_cast<uint32_t>(config.size());
  configList.ConfigPtr = const_cast<SCONFIG *>(config.data());
  return passThruIoctl(SET_CONFIG, &configList);
}

unsigned long J2534Channel::getProtocolId() const { return _protocolId; }

unsigned long J2534Channel::getTxFlags() const { return _txFlags; }

unsigned long J2534Channel::getBaudrate() const { return _baudrate; }

void J2534Channel::readMsgs(
    const std::function<bool(const uint8_t *data, size_t length)> &func,
    unsigned long timeout) const {
  // Keep timeout bounded across ignored frames, e.g. loopback or unrelated CAN
  // traffic.
  const auto deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
  while (true) {
    const auto now = std::chrono::steady_clock::now();
    if (now >= deadline) {
      throw std::runtime_error("Timed out waiting for data from CAN channel");
    }
    const auto remaining =
        static_cast<unsigned long>(std::max<std::chrono::milliseconds::rep>(
            1, std::chrono::duration_cast<std::chrono::milliseconds>(deadline -
                                                                     now)
                   .count()));
    std::vector<PASSTHRU_MSG> msgs(1);
    const auto readResult = readMsgs(msgs, remaining);
    if (readResult != STATUS_NOERROR && readResult != ERR_TIMEOUT &&
        readResult != ERR_BUFFER_EMPTY) {
      throw std::runtime_error(
          "PassThruReadMsgs failed while reading data from CAN channel: " +
          j2534StatusToString(readResult));
    }
    if (readResult == ERR_TIMEOUT || readResult == ERR_BUFFER_EMPTY ||
        msgs.empty()) {
      continue;
    }
    if (!func(msgs[0].Data, msgs[0].DataSize)) {
      break;
    }
  }
}

} // namespace j2534
