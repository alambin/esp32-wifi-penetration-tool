#ifndef SERIAL_COMMAND_DISPATCHER_H_
#define SERIAL_COMMAND_DISPATCHER_H_

#include <functional>
#include <map>
#include <string>

class SerialCommandDispatcher {
 public:
  enum class CommandType : uint8_t {
    kReset,
    kStartLogs,
    kStopLogs,
    kLimitLogs,
    kSetLogLevel,
    kLedOn,
    kLedOff,
    kHelp,
    kGetAttackStatus,
    kBtTerminalConnected  // Fake command, which is not received via BT Serial Port, but generated by
                          // BluetoothSerial when BT terminal is connected or disconnected
  };
  using CommandHandlerType = std::function<void(const std::string& param)>;

  SerialCommandDispatcher();
  void setCommandHandler(CommandType command, CommandHandlerType handler);
  void onNewSymbols(std::string symbols);
  std::string getSupportedCommands() const;

 private:
  void process(const std::string& command);

  std::map<CommandType, CommandHandlerType> mHandlers;
  std::string mCurrentlyReceivedSymbols;
};

#endif  // SERIAL_COMMAND_DISPATCHER_H_
