#ifndef COMPNET_LAB4_SRC_SWITCH_H
#define COMPNET_LAB4_SRC_SWITCH_H

#include "types.h"
#include <map>
#include <array>

class SwitchBase {
 public:
  SwitchBase() = default;
  ~SwitchBase() = default;

  virtual void InitSwitch(int numPorts) = 0;
  virtual int ProcessFrame(int inPort, char* framePtr) = 0;
};

extern SwitchBase* CreateSwitchObject();

// TODO : Implement your switch class.
class EthernetSwitch : public SwitchBase {
  int port_num;
  std::map<std::array<uint8_t,6>, int> mac_table;
  std::map<std::array<uint8_t,6>, int> mac_ctr;
public:
  void InitSwitch(int numPorts);
  int ProcessFrame(int inPort, char* framePtr);
};

#endif  // ! COMPNET_LAB4_SRC_SWITCH_H