#include "Arduino.h"
#include "utils.h"


float mapfloat(long x, long in_min, long in_max, long out_min, long out_max) {
  return (float)(x - in_min) * (out_max - out_min) / (float)(in_max - in_min) + out_min;
}


String str(const char *fmtStr, ...) {
  static char buf[201] = {'\0'};
  va_list arg_ptr;
  va_start(arg_ptr, fmtStr);
  vsnprintf(buf, 200, fmtStr, arg_ptr);
  va_end(arg_ptr);
  return String(buf);
}
String str(std::string s) {
  return String(s.c_str());
}
String str(bool v) {
  return v ? " WIN" : " FAIL";
}

// float lifepo4_soc[] = {13.4, 13.3, 13.28, 13.};



PowerSupply::PowerSupply(Stream &port) : _port(&port), debug_(false) { }

bool PowerSupply::begin() {
  flush();
  return doUpdate();
}

bool PowerSupply::doUpdate() {
  bool res = readVoltage() &&
         readCurrent() &&
         getOutputEnabled();
  if (res && !limitVolt_) {
    handleReply(cmdReply("arc")); //read current limit
    handleReply(cmdReply("arv")); //read voltage limit
  }
  return res;
}

bool PowerSupply::readVoltage() { return handleReply(cmdReply("aru")); }
bool PowerSupply::readCurrent() { return handleReply(cmdReply("ari")); }
bool PowerSupply::getOutputEnabled() { return handleReply(cmdReply("aro")); }

template<typename T>void setCheck(T &save, float in, float max) { if (in < max) save = in; }

bool PowerSupply::handleReply(String msg) {
  if (!msg.length()) return false;
  String hdr = msg.substring(0, 3);
  String body = msg.substring(3);
  if      (hdr == "#ro") setCheck(outEn_, (body.toInt() == 1), 2);
  else if (hdr == "#ru") setCheck(outVolt_, body.toFloat() / 100.0, 80);
  else if (hdr == "#ri") setCheck(outCurr_, body.toFloat() / 100.0, 15);
  else if (hdr == "#rv") setCheck(limitVolt_, body.toFloat() / 100.0, 80);
  else if (hdr == "#ra") setCheck(limitCurr_, body.toFloat() / 100.0, 15);
  else {
    Serial.println("got unknown msg > '" + hdr + "' / '" + body + "'");
    return false;
  }
  return true;
}

void PowerSupply::flush() {
  _port->flush();
}

String PowerSupply::cmdReply(String cmd) {
  _port->print(cmd + "\r\n");
  if (debug_) Serial.println("> " + cmd + "CRLF");
  String reply;
  uint32_t start = millis();
  char c;
  while ((millis() - start) < 1000 && !reply.endsWith("\n"))
    if (_port->readBytes(&c, 1))
      reply.concat(c);
  if (debug_ && reply.length()) {
    String debug = reply;
    debug.replace("\r", "CR");
    debug.replace("\n", "NL");
    Serial.println("< " + debug);
  }
  if (!reply.length() && debug_ && _port->available())
    Serial.printf("nothing read.. stuff available!? %d", _port->available());
  reply.trim();
  return reply;
}

bool PowerSupply::enableOutput(bool status) {
  String r = cmdReply(str("awo%d\r\n", status ? 1 : 0));
  if (r == "#wook") {
    outEn_ = status;
    return true;
  } else return false;
}

String PowerSupply::fourCharStr(uint16_t input) {
  char buf[] = "    ";
  // Iterate through units, tens, hundreds etc
  for (int digit = 0; digit < 4; digit++)
    buf[3 - digit] = ((input / ((int) pow(10, digit))) % 10) + '0';
  return String(buf);
}

bool PowerSupply::setVoltage(float v) {
  limitVolt_ = v;
  String r = cmdReply("awu" + fourCharStr(v * 100.0));
  return (r == "#wuok");
}

bool PowerSupply::setCurrent(float v) {
  limitCurr_ = v;
  String r = cmdReply("awi" + fourCharStr(v * 100.0));
  return (r == "#wiok");
}
