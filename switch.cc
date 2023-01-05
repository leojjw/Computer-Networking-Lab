#include "switch.h"
#include <iostream>

using namespace std;

SwitchBase* CreateSwitchObject() {
  // TODO : Your code.
  return new EthernetSwitch;
}

void EthernetSwitch::InitSwitch(int numPorts) {
  port_num = numPorts;
  return;
}

int EthernetSwitch::ProcessFrame(int inPort, char* framePtr) {
  ether_header_t * header = (ether_header_t *)framePtr;

  if (header->ether_type == ETHER_CONTROL_TYPE){
    map<array<uint8_t,6>, int>::iterator i;
    for(i = mac_ctr.begin(); i != mac_ctr.end(); i++){
      if (i->second > 0){
        i->second = i->second - 1;
        if (i->second == 0){
          mac_table.erase(i->first);
        }
      }
    }
    return -1;
  }

  array<uint8_t,6> src;
  memcpy(&src, &(header->ether_src), 6);
  mac_table[src] = inPort;
  mac_ctr[src] = 10;

  int out_port;
  array<uint8_t,6> dest;
  memcpy(&dest, &(header->ether_dest), 6);
  try {  
    out_port = mac_table.at(dest);
  }
  catch(out_of_range e) {
    return 0;
  }

  if (out_port == inPort){
    return -1;
  }

  return out_port;
}