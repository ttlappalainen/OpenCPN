
#include "config.h"

#include <algorithm>
#include <iostream>
#include <fstream>
#include <cstdio>

#include "wx/event.h"
#include "wx/app.h"

#include <gtest/gtest.h>

#include "BasePlatform.h"
#include "comm_appmsg_bus.h"
#include "comm_bridge.h"
#include "comm_drv_file.h"
#include "comm_drv_registry.h"
#include "observable_navmsg.h"
#include "observable_confvar.h"
#include "ocpn_types.h"
#include "AIS_Decoder.h"

BasePlatform* g_BasePlatform = 0;
bool g_bportable = false;
wxString g_winPluginDir;
wxConfigBase* pBaseConfig = 0;
void* g_pi_manager = reinterpret_cast<void*>(1L);
wxString g_compatOS = PKG_TARGET;
wxString g_compatOsVersion = PKG_TARGET_VERSION;

namespace safe_mode {
bool get_mode() { return false; }
}  // namespace safe_mode

wxString g_catalog_custom_url;
wxString g_catalog_channel;
wxLog* g_logger;

/* comm_bridge context. */

double gCog;
double gHdm;
double gHdt;
double gLat;
double gLon;
double gSog;
double gVar;
double g_UserVar;
int gps_watchdog_timeout_ticks;
int g_nNMEADebug;
int g_NMEAAPBPrecision;
bool g_bVAR_Rx;
int g_SatsInView;
bool g_bSatValid;
bool g_bHDT_Rx;
int g_priSats;
int sat_watchdog_timeout_ticks = 12;

wxString g_TalkerIdText;

wxString gRmcTime;
wxString gRmcDate;

// navutil_base context

int g_iDistanceFormat = 0;
int g_iSDMMFormat = 0;
int g_iSpeedFormat = 0;

wxDEFINE_EVENT(EVT_FOO, ObservedEvt);
wxDEFINE_EVENT(EVT_BAR, ObservedEvt);

std::string s_result;
std::string s_result2;
std::string s_result3;

NavAddr::Bus s_bus;
AppMsg::Type s_apptype;

auto shared_navaddr_none = std::make_shared<NavAddr>();

class MsgCliApp : public wxAppConsole {
public:
  class Sink : public wxEvtHandler {
  private:
    ObservedVarListener listener;

  public:
    Sink() {
      ObservableMsg observable("1234");
      listener = observable.GetListener(this, EVT_BAR);
      Bind(EVT_BAR, [&](ObservedEvt ev) {
        auto msg = ev.GetSharedPtr();
        auto n2000_msg = std::static_pointer_cast<const Nmea2000Msg>(msg);
        std::string s(n2000_msg->payload.begin(), n2000_msg->payload.end());
        s_result = s;
        s_bus = n2000_msg->bus;
      });
    }
  };

  class Source {
  public:
    Source() {
      std::string s("payload data");
      auto payload = std::vector<unsigned char>(s.begin(), s.end());
      auto id = static_cast<uint64_t>(1234);
      auto n2k_msg =
          std::make_shared<const Nmea2000Msg>(id, payload, shared_navaddr_none);
      ObservableMsg observable("1234");
      observable.notify(n2k_msg);
    }
  };

  MsgCliApp() : wxAppConsole() {
    Sink sink;
    Source source;
    ProcessPendingEvents();
  }
};

class TransportCliApp : public wxAppConsole {
public:
  class Source {
  public:
    Source() {
      std::string s("payload data");
      auto payload = std::vector<unsigned char>(s.begin(), s.end());
      auto id = static_cast<uint64_t>(1234);
      auto msg =
          std::make_unique<Nmea2000Msg>(id, payload, shared_navaddr_none);
      NavMsgBus::GetInstance().Notify(std::move(msg));
    }
  };

  class Sink : public wxEvtHandler {
  public:
    Sink() {
      auto& t = NavMsgBus::GetInstance();
      Nmea2000Msg n2k_msg(static_cast<uint64_t>(1234));
      listener = t.GetListener(EVT_FOO, this, n2k_msg);

      Bind(EVT_FOO, [&](ObservedEvt ev) {
        auto ptr = ev.GetSharedPtr();
        auto n2k_msg = std::static_pointer_cast<const Nmea2000Msg>(ptr);
        std::string s(n2k_msg->payload.begin(), n2k_msg->payload.end());
        s_result = s;
        s_bus = n2k_msg->bus;
      });
    }
    ObservedVarListener listener;
  };

  TransportCliApp() : wxAppConsole() {
    Sink sink;
    Source source;
    ProcessPendingEvents();
  }
};

class All0183App : public wxAppConsole {
public:
  class Source {
  public:
    Source() {
      std::string payload("payload data");
      std::string id("GPGGA");
      auto msg1 =
          std::make_shared<Nmea0183Msg>(id, payload, shared_navaddr_none);
      auto msg_all = std::make_shared<const Nmea0183Msg>(*msg1, "ALL");
      NavMsgBus::GetInstance().Notify(std::move(msg_all));
    }
  };

  class Sink : public wxEvtHandler {
  public:
    Sink() {
      auto& t = NavMsgBus::GetInstance();
      listener = t.GetListener(EVT_FOO, this, Nmea0183Msg::MessageKey("ALL"));

      Bind(EVT_FOO, [&](ObservedEvt ev) {
        auto ptr = ev.GetSharedPtr();
        auto msg = std::static_pointer_cast<const Nmea0183Msg>(ptr);
        s_result = msg->payload;
        s_bus = msg->bus;
      });
    }
    ObservedVarListener listener;
  };

  All0183App() : wxAppConsole() {
    Sink sink;
    Source source;
    ProcessPendingEvents();
  }
};

class ListenerCliApp : public wxAppConsole {
public:
  class Source {
  public:
    Source() {
      std::string s("payload data");
      auto payload = std::vector<unsigned char>(s.begin(), s.end());
      auto id = static_cast<uint64_t>(1234);
      auto msg =
          std::make_unique<Nmea2000Msg>(id, payload, shared_navaddr_none);
      NavMsgBus::GetInstance().Notify(std::move(msg));
    }
  };

  class Sink : public wxEvtHandler {
  public:
    Sink() {
      auto& t = NavMsgBus::GetInstance();
      Nmea2000Msg n2k_msg(static_cast<uint64_t>(1234));
      listeners.push_back(t.GetListener(EVT_FOO, this, n2k_msg));
      Bind(EVT_FOO, [&](ObservedEvt ev) {
        auto ptr = ev.GetSharedPtr();
        auto n2k_msg = std::static_pointer_cast<const Nmea2000Msg>(ptr);
        std::string s(n2k_msg->payload.begin(), n2k_msg->payload.end());
        s_result = s;
        s_bus = n2k_msg->bus;
      });
    }
    std::vector<ObservedVarListener> listeners;
  };

  ListenerCliApp() : wxAppConsole() {
    Sink sink;
    Source source;
    ProcessPendingEvents();
  }
};

class AppmsgCliApp : public wxAppConsole {
public:
  class Source {
  public:
    Source() {
      Position pos(65.2211, 21.4433, Position::Type::NE);
      auto fix = std::make_shared<GnssFix>(pos, 1659345030);
      AppMsgBus::GetInstance().Notify(std::move(fix));
    }
  };

  class Sink : public wxEvtHandler {
  public:
    Sink() {
      auto& a = AppMsgBus::GetInstance();
      listener = a.GetListener(EVT_FOO, this, AppMsg::Type::GnssFix);

      Bind(EVT_FOO, [&](ObservedEvt ev) {
        auto ptr = ev.GetSharedPtr();
        auto msg = std::static_pointer_cast<const AppMsg>(ptr);
        std::cout << msg->TypeToString(msg->type) << "\n";
        auto fix = std::static_pointer_cast<const GnssFix>(msg);
        if (fix == 0) {
          std::cerr << "Cannot cast pointer\n" << std::flush;
        } else {
          s_result = fix->pos.to_string();
          s_apptype = fix->type;
        }
      });
    }
    ObservedVarListener listener;
  };

  AppmsgCliApp() : wxAppConsole() {
    Sink sink;
    Source source;
    ProcessPendingEvents();
  };
};

using namespace std;

#ifdef _MSC_VER
const static string kSEP("\\");
#else
const static string kSEP("/");
#endif

class GuernseyApp : public wxAppConsole {
public:
  GuernseyApp(vector<string>& log) : wxAppConsole() {
    auto& msgbus = NavMsgBus::GetInstance();
    string path("..");
    path += kSEP + ".." + kSEP + "test" + kSEP + "testdata" + kSEP +
            "Guernesey-1659560590623.input.txt";
    auto driver = make_shared<FileCommDriver>("test-output.txt", path, msgbus);
    auto listener = msgbus.GetListener(EVT_FOO, this, Nmea0183Msg("GPGLL"));
    Bind(EVT_FOO, [&log](ObservedEvt ev) {
      auto ptr = ev.GetSharedPtr();
      auto n0183_msg = static_pointer_cast<const Nmea0183Msg>(ptr);
      log.push_back(n0183_msg->to_string());
    });
    driver->Activate();
    ProcessPendingEvents();
  }
};

class PriorityApp : public wxAppConsole {
public:
  PriorityApp(string inputfile) : wxAppConsole() {
    auto& msgbus = NavMsgBus::GetInstance();
    string path("..");
    path += kSEP + ".." + kSEP + "test" + kSEP + "testdata" + kSEP + inputfile;
    auto driver = make_shared<FileCommDriver>(inputfile + ".log", path, msgbus);
    CommBridge comm_bridge;
    comm_bridge.Initialize();
    driver->Activate();
    ProcessPendingEvents();
  }
};

const char* const GPGGA_1 =
    "$GPGGA,092212,5759.097,N,01144.345,E,1,06,1.9,3.5,M,39.4,M,,*4C";
const char* const GPGGA_2 =
    "$GPGGA,092212,5755.043,N,01344.585,E,1,06,1.9,3.5,M,39.4,M,,*4C";
class PriorityApp2 : public wxAppConsole {
public:
  PriorityApp2() : wxAppConsole() {
    auto& msgbus = NavMsgBus::GetInstance();
    CommBridge comm_bridge;
    comm_bridge.Initialize();

    auto addr1 = std::make_shared<NavAddr>(NavAddr0183("interface1"));
    auto m1 = std::make_shared<const Nmea0183Msg>(
        Nmea0183Msg("GPGGA", GPGGA_1, addr1));
    auto addr2 = std::make_shared<NavAddr>(NavAddr0183("interface2"));
    auto m2 = std::make_shared<const Nmea0183Msg>(
        Nmea0183Msg("GPGGA", GPGGA_2, addr2));
    msgbus.Notify(m1);
    msgbus.Notify(m2);
    ProcessPendingEvents();

    Position p = Position::ParseGGA("5759.097,N,01144.345,E");
    EXPECT_NEAR(gLat, p.lat, 0.0001);
    EXPECT_NEAR(gLon, p.lon, 0.0001);
  }
};

class SillyDriver : public AbstractCommDriver {
public:
  SillyDriver() : AbstractCommDriver(NavAddr::Bus::TestBus, "silly") {}
  SillyDriver(const string& s) : AbstractCommDriver(NavAddr::Bus::TestBus, s) {}

  virtual void SendMessage(const NavMsg& msg, const NavAddr& addr) {}

  virtual void SetListener(DriverListener& listener) {}

  virtual void Activate(){};
};

class SillyListener : public DriverListener {
public:
  /** Handle a received message. */
  virtual void Notify(std::shared_ptr<const NavMsg> message) {
    s_result2 = NavAddr::BusToString(message->bus);

    auto base_ptr = message.get();
    auto n2k_msg = dynamic_cast<const Nmea2000Msg*>(base_ptr);
    s_result3 = n2k_msg->name.to_string();

    stringstream ss;
    std::for_each(n2k_msg->payload.begin(), n2k_msg->payload.end(),
                  [&ss](unsigned char c) { ss << static_cast<char>(c); });
    s_result = ss.str();
  }

  /** Handle driver status change. */
  virtual void Notify(const AbstractCommDriver& driver) {}
};

TEST(Messaging, ObservableMsg) {
  s_result = "";
  s_bus = NavAddr::Bus::Undef;
  MsgCliApp app;
  EXPECT_EQ(s_result, string("payload data"));
  EXPECT_EQ(NavAddr::Bus::N2000, s_bus);
};

TEST(Messaging, NavMsg) {
  s_result = "";
  s_bus = NavAddr::Bus::Undef;
  TransportCliApp app;
  EXPECT_EQ(s_result, string("payload data"));
  EXPECT_EQ(NavAddr::Bus::N2000, s_bus);
};

TEST(Messaging, All0183) {
  s_result = "";
  s_bus = NavAddr::Bus::Undef;
  All0183App app;
  EXPECT_EQ(s_result, string("payload data"));
  EXPECT_EQ(NavAddr::Bus::N0183, s_bus);
};

#ifndef _MSC_VER
// FIXME (leamas) Fails on string representation of UTF degrees 0x00B0 on Win
TEST(Messaging, AppMsg) {
  s_result = "";
  s_bus = NavAddr::Bus::Undef;
  AppmsgCliApp app;
  EXPECT_EQ(s_result, string("65°22,11N 21°44,33E"));
  EXPECT_EQ(s_apptype, AppMsg::Type::GnssFix);
};

#endif

TEST(Drivers, Registry) {
  auto driver = std::make_shared<const SillyDriver>();
  auto& registry = CommDriverRegistry::getInstance();
  registry.Activate(std::static_pointer_cast<const AbstractCommDriver>(driver));
  auto drivers = registry.GetDrivers();
  EXPECT_EQ(registry.GetDrivers().size(), 1);
  EXPECT_EQ(registry.GetDrivers()[0]->iface, string("silly"));
  EXPECT_EQ(registry.GetDrivers()[0]->bus, NavAddr::Bus::TestBus);

  /* Add it again, should be ignored. */
  registry.Activate(std::static_pointer_cast<const AbstractCommDriver>(driver));
  EXPECT_EQ(registry.GetDrivers().size(), 1);

  /* Add another one, should be accepted */
  auto driver2 = std::make_shared<const SillyDriver>("orvar");
  registry.Activate(
      std::static_pointer_cast<const AbstractCommDriver>(driver2));
  EXPECT_EQ(registry.GetDrivers().size(), 2);

  /* Remove one, leaving one in place. */
  registry.Deactivate(
      std::static_pointer_cast<const AbstractCommDriver>(driver2));
  EXPECT_EQ(registry.GetDrivers().size(), 1);

  /* Remove it again, should be ignored. */
  registry.Deactivate(
      std::static_pointer_cast<const AbstractCommDriver>(driver2));
  EXPECT_EQ(registry.GetDrivers().size(), 1);
}

TEST(Navmsg2000, to_string) {
  std::string s("payload data");
  auto payload = std::vector<unsigned char>(s.begin(), s.end());
  auto id = static_cast<uint64_t>(1234);
  auto msg = std::make_shared<Nmea2000Msg>(id, payload, shared_navaddr_none);
  EXPECT_EQ(string("nmea2000 n2000-1234 1234 7061796c6f61642064617461"),
            msg->to_string());
}

TEST(FileDriver, Registration) {
  auto driver = std::make_shared<FileCommDriver>("test-output.txt");
  auto& registry = CommDriverRegistry::getInstance();
  int start_size = registry.GetDrivers().size();
  driver->Activate();
  auto drivers = registry.GetDrivers();
  EXPECT_EQ(registry.GetDrivers().size(), start_size + 1);
}

TEST(FileDriver, output) {
  auto driver = std::make_shared<FileCommDriver>("test-output.txt");
  std::string s("payload data");
  auto payload = std::vector<unsigned char>(s.begin(), s.end());
  auto id = static_cast<uint64_t>(1234);
  Nmea2000Msg msg(id, payload, shared_navaddr_none);
  remove("test-output.txt");
  driver->SendMessage(msg, NavAddr());
  std::ifstream f("test-output.txt");
  stringstream ss;
  ss << f.rdbuf();
  EXPECT_EQ(ss.str(),
            string("nmea2000 n2000-1234 1234 7061796c6f61642064617461"));
}

TEST(FileDriver, input) {
  auto driver = std::make_shared<FileCommDriver>("test-output.txt");
  std::string s("payload data");
  auto payload = std::vector<unsigned char>(s.begin(), s.end());
  auto id = static_cast<uint64_t>(1234);
  Nmea2000Msg msg(id, payload, shared_navaddr_none);
  remove("test-output.txt");
  driver->SendMessage(msg, NavAddr());

  SillyListener listener;
  auto indriver = std::make_shared<FileCommDriver>("/tmp/foo.txt",
                                                   "test-output.txt", listener);
  indriver->Activate();
  EXPECT_EQ(s_result2, string("nmea2000"));
  EXPECT_EQ(s_result3, string("1234"));
  EXPECT_EQ(s_result, string("payload data"));
}

TEST(Listeners, vector) {
  s_result = "";
  ListenerCliApp app;
  EXPECT_EQ(s_result, string("payload data"));
  EXPECT_EQ(NavAddr::Bus::N2000, s_bus);
};

TEST(Guernsey, play_log) {
  vector<string> log;
  GuernseyApp app(log);
  EXPECT_EQ(log.size(), 14522);
}

TEST(FindDriver, lookup) {
  std::vector<DriverPtr> drivers;
  std::vector<std::string> ifaces{"foo", "bar", "foobar"};
  for (const auto& iface : ifaces) {
    drivers.push_back(std::make_shared<SillyDriver>(SillyDriver(iface)));
  }
  auto found = FindDriver(drivers, "bar");
  EXPECT_EQ(found->iface, string("bar"));
  found = FindDriver(drivers, "baz");
  EXPECT_FALSE(found);
}

TEST(Registry, persistence) {
  int start_size = 0;
  if (true) {  // a scope
    auto driver = std::make_shared<const SillyDriver>();
    auto& registry = CommDriverRegistry::getInstance();
    start_size = registry.GetDrivers().size();
    registry.Activate(
        std::static_pointer_cast<const AbstractCommDriver>(driver));
  }
  auto& registry = CommDriverRegistry::getInstance();
  auto drivers = registry.GetDrivers();
  EXPECT_EQ(registry.GetDrivers().size(), start_size + 1);
  EXPECT_EQ(registry.GetDrivers()[start_size]->iface, string("silly"));
  EXPECT_EQ(registry.GetDrivers()[start_size]->bus, NavAddr::Bus::TestBus);
}

TEST(Position, ParseGGA) {
  Position p = Position::ParseGGA("5800.602,N,01145.789,E");
  EXPECT_NEAR(p.lat, 58.010033, 0.0001);
  EXPECT_NEAR(p.lon, 11.763150, 0.0001);
}

TEST(Priority, Framework) {
  PriorityApp app("stupan.se-10112-tcp.log.input");
  EXPECT_NEAR(gLat, 57.6460, 0.001);
  EXPECT_NEAR(gLon, 11.7130, 0.001);
}

TEST(Priority, DifferentSource) {
  PriorityApp2 app;
  Position p = Position::ParseGGA("5759.097,N,01144.345,E");
  EXPECT_NEAR(p.lat, 57.98495, 0.001);
  EXPECT_NEAR(p.lon, 11.73908, 0.001);
}