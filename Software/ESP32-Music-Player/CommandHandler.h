#ifndef _COMMAND_HANDLER_H_
#define _COMMAND_HANDLER_H_

class CommandHandlerClass {
  public:
    CommandHandlerClass();

    void SerialCommand(String &r);
    void GuiCommand(uint8_t key);
    void WifiConnect(bool reconnect);
    void SetWifiPower(int pwr, bool force = false);
    void SetBtPower(int pwr, bool force = false);
  protected:
    void CheckUpdateFromCallback();
};
#endif
