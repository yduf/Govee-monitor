#include <iostream>
#include <string>

#include "signal.hh"
#include "hci_extended_cmd.h"

using namespace std;

auto verbose = false;

// Error helper
struct Error : public string { 
    Error( const std::string& msg) :
        string( msg) {
            perror( msg.c_str());
        }
};

// Expected event from ble scan
struct buffer_t {
    union {
        uint8_t raw[HCI_MAX_EVENT_SIZE];                    // needed to get all information from socket
                                                            // otherwise result are intellegently truncated => len_data is set to 0 in reports
        
        struct {
            // HCI Event: LE Meta Event  (0x3e) plen 53
            uint8_t       pad0[1];      // ogf, ocf
            hci_event_hdr hdr;          // bt_hci_evt_hdr

            uint8_t       subevent;

            // LE Extended Advertising Report (0x0d)
            struct {
                uint8_t       num_reports;
                bt_hci_le_ext_adv_report report[0];
            }__attribute__ ((packed));
        } __attribute__ ((packed));
    };
};

// 
uint64_t bl_addr2int( const uint8_t buf[6]) {
    union {
        uint64_t i;
        uint8_t  b2[8];
    } addr = {};

    for( int i = 0; i < 6; i++) {
        addr.b2[i] = buf[i];
    }

    return addr.i;
}



void show_data( int data_len, const uint8_t* data, bool hex=true, const string& end = "\n") {
    for( int i = 0; i < data_len; ++i) {
        if( hex)
            cout <<  std::hex <<  (int)data[i] << " ";
        else
            cout << data[i];
    }   

    cout << end;
}

#include <poll.h>
#include <array>

// Helper class for BLE Scan
class BTdevice {
public:   
    static constexpr int timeout_ms = 1'000;

 	int device_id;
	int socket;
    array<pollfd,1> fds;

    buffer_t buf;

    BTdevice( const string& controler = "") {
        if( controler.empty())
            device_id = hci_get_route(NULL);
        else
            device_id = hci_devid( controler.c_str());

        if (device_id < 0)
            throw Error("Bluetooth device not found");
    }

    ~BTdevice() {
        this->scan( false);
        hci_close_dev( socket);
    }

    void open() {
        socket = hci_open_dev(device_id);
        if(socket < 0) 
            throw Error("Could not open device");

        // Make sure scan is disabled first
        scan( false);

        if( set_random(  socket, timeout_ms) < 0)
            throw Error( "set_random");

        if( hci_le_set_ext_scan_parameters( socket, 0x01, htobs(0x0012), htobs(0x0012), LE_RANDOM_ADDRESS, 0x00, timeout_ms) < 0)
            throw Error( "hci_le_set_ext_scan_parameters");

        // prepare polling
        // http://www.ulduzsoft.com/2014/01/select-poll-epoll-practical-difference-for-system-architects/#Polling_with_poll
        fds[0].fd = socket;
        fds[0].events = POLLIN;     // Monitor socket for input
    }

    void scan( bool enable = true) {
        if( enable) {
            if( verbose)
                cerr << "*** start scan\n";

            // Save the current HCI filter (Host Controller Interface)
            struct hci_filter original_filter;
            socklen_t olen = sizeof(original_filter);
            if( getsockopt( socket, SOL_HCI, HCI_FILTER, &original_filter, &olen) < 0)
                throw Error( "getsockopt");

            // Create and set the new filter
            struct hci_filter new_filter;
            hci_filter_clear(&new_filter);
            hci_filter_set_ptype(HCI_EVENT_PKT, &new_filter);
            hci_filter_set_event(EVT_LE_META_EVENT, &new_filter);
            if (setsockopt(socket, SOL_HCI, HCI_FILTER, &new_filter, sizeof(new_filter)) < 0)
                throw Error( "setsockopt");
        }
        else {
            if( verbose)
                cerr << "*** stop scan\n";
        }

        constexpr auto filter_duplicate = 0x00;
		if( hci_le_set_ext_scan_enable( socket, enable, filter_duplicate, timeout_ms) < 0)
            throw Error( "hci_le_set_ext_scan_enable");
    }

    // return next advertising event
    auto next_adv() -> bt_hci_le_ext_adv_report* {

        // Wait for inputs
        for(;;) {
            constexpr auto poll_timeout_ms = 1'000;
            int ret = poll( fds.begin(), fds.size(),poll_timeout_ms);

            // Check if poll actually succeed
            if( ret == -1 ) {
                if (errno == EINTR) {
                    cerr << "*** poll interrupted\n";
                    SignalHandler::check();
                }

                throw Error( "poll failed");                    // report error and abort
            }
            
            if( ret == 0) {
                if( verbose)
                    cerr << "*** poll timeout\n";

                continue;
            }

            // else
            SignalHandler::check();                         // what if interrupted in read ?


            // got an event: we don't care about the event, since we monitor only one socket
            // Now read the data following the chunk & let the buffer block as long it's not ready
            if( verbose)    
                cerr << "*** read socket\n";

            ssize_t bufDataLen = read( socket, &buf, sizeof(buf));

            if( bufDataLen <= 0)                    // expect a full message
                throw Error("read error");

            if( verbose)
                cout << "evt:" << std::hex << (int)  buf.hdr.evt << " "
                    << "plen:" << std::dec << (int)  buf.hdr.plen << " "
                    ; 

            switch( buf.hdr.evt) {

            case BT_HCI_EVT_LE_META_EVENT: {

                if( verbose) {
                    cout << "\nevt:" << std::hex << (int)  buf.hdr.evt << " "
                            << "plen:" << std::dec << (int)  buf.hdr.plen << " "
                            ; 

                    cout <<  "subevt:" << std::hex << (int)  buf.subevent << " "
                        << "numreport:" << std::dec << (int) buf.num_reports << " "
                    ;
                }

                // anyway we only process&return first report / would need a continuation otherwise
                for( int i = 0; i < buf.num_reports; ++i) {
                    if( verbose)   
                        cerr << "*** read report " << i << "\n";

                    auto& report = buf.report[i];

                    if( verbose) {
                        uint64_t addr = bl_addr2int(report.addr);
                        cout <<  "\naddr  :" << std::hex << addr  << "\n"
                            << ", RSSI: " << std::dec << (int) report.rssi
                            << "\n";

                        cerr << "- event type : " << std::hex << (int) report.event_type << "\n";

                        cout << "data length: " << std::hex << (int) report.data_len << " "
                            << "\ndata: ";
                        show_data( report.data_len, report.data); 
                    }
 
                    return &report;
                } 
            }

            default:
                cerr << "unknown event " << buf.hdr.evt << "\n";
            }
        }
    }
};


//

struct BTDevice {
    int isGovee;            // -1 false, 0 don't know, 1 true
    int found;              // got a signal
};

struct Govee : public BTDevice {
    string room;
    string name;
    
    float temp;
    float hum;
    float battery;
    int   rssi;
};

// private struct for msg from govee
struct GVH5075_msg {
    uint16_t uuid;
    uint32_t iTemp;
    uint8_t  battery;
}__attribute__ ((packed));

// GVH5075_xxxx
// from https://github.com/Thrilleratplay/GoveeWatcher
void parseMSG( Govee& govee, int data_len, const uint8_t* data) {
    // Data: 88 ec 0 3 31 9e 41 0 
    // temp: 20.9C°, humidity: 31%, battery: 41%
    auto msg = *reinterpret_cast<const GVH5075_msg*>( data);
    auto htemp = htonl( msg.iTemp);
    bool bNegative = htemp & 0x80'00'00;	// check sign bit
    auto iTemp = htemp & 0x7'ff'ff;		// mask off sign bit
    auto temp_C = float(iTemp / 1000) / 10.0;     
    if (bNegative)						// apply sign bit
        temp_C *= -1.0;
    auto Humidity = float(iTemp % 1000) / 10.0;
    auto Battery = int( msg.battery);

    if( verbose)    
        cerr << "temp: " << temp_C << "C°, humidity: " << Humidity << "%, battery: " << Battery << "%\n";   
    govee.temp = temp_C;
    govee.hum  = Humidity;
    govee.battery = Battery;
}


void parse(  Govee& govee, int chunk_len, const uint8_t* data) {
    switch( data[0]) {
        default:
        case 0x07: // 128b UUID
        case 0x16: //  16b UUID
            if( verbose) {
                cerr << "unknown:";
                show_data( chunk_len, data);
            }
            break;

        case 0x01: // flags
            if( verbose) {
                cerr << "flags: ";
                show_data( chunk_len -1, data +1);
            }
            break;

        case 0x03: { //  16b Service UUID Complete
            if( verbose) {
                cerr << "16b Service UUID (complete): ";
                show_data( chunk_len -1, data +1);
            }

            uint16_t uuid = *reinterpret_cast<const uint16_t*>( data +1);
            govee.isGovee = uuid == 0xec88;
            break;
        }

        case 0x09: // complete local name
            if( verbose) {
                cerr << "Name (complete): '";
                show_data( chunk_len -1, data +1, false, "'\n");
            }
            govee.name = string{ (char*) data +1, chunk_len -1};
            break;

        case 0xFF: // Manufacturer specific data
            if( verbose) {
                cerr << "Data: ";
                show_data( chunk_len-1, data+1);
            }
            parseMSG( govee, chunk_len-1, data+1);
            break;
    }
}

void parse_data(  Govee& govee, int data_len, const uint8_t* data) {
    for( ; data_len > 0; ) {
        int chunk_len = *data++;
        data_len -= 1;

        parse( govee, chunk_len, data);
        data_len -= chunk_len;
        data += chunk_len;
    }
}

#include <map>
#include <iomanip>
#include <sys/statvfs.h>

#include "toml/toml.hpp"
#include "Argengine/argengine.cpp" // used as "single header" see https://github.com/juzzlin/Argengine

// associate room, with given device
void parse_Toml(  map<string, Govee>& bt_devices, const string& path) {
    toml::table tbl;
    try
    {
        tbl = toml::parse_file( path);
        if( verbose)
            std::cout << tbl << "\n";

        // retrieves all name from definition
        // and setup room properties accordingly
        tbl.for_each([&](const toml::key& key, auto&& val)
        {
            if( val.is_table()) {
                auto sub = *val.as_table();
                if( sub.contains("room")) {
                    auto room = sub.get("room")->as_string()->get();
                    bt_devices[key.data()].room = room; 

                if( verbose)
                    std::cout << key << ": " << val << "\n";
                }
            }
            // else => skip it
        });
    }
    catch (const toml::parse_error& err)
    {
        std::cerr << "Parsing failed:\n" << err << "\n";
    }
}


// allows to specify config by device Name
void load_config( map<string, Govee>& bt_devices ) {
    string config_path[] = { "~/.config/govee_logger", ".govee_logger" };
    bool hasConfig = false;

    for( auto& conf: config_path) {
        struct statvfs fiData;
        if(( statvfs( conf.c_str(), &fiData) == 0) ) {
            cerr << "loading config from " << conf << "\n";
            hasConfig = true;
            parse_Toml( bt_devices, conf);
        }
    }

    if( !hasConfig) {
        cerr << "no config loaded\n";
    }
}

// scan BLE for Govee device and display info
int main(int argc, char **argv)
{try {

    // read options
    juzzlin::Argengine ae(argc, argv);
    ae.addOption({"-v", "--verbose"}, [&] {
        verbose = true;
    });
    ae.parse();

    // read config
    map<string, Govee> by_name;
    load_config( by_name);

    // init BLE Stack
    auto bt = BTdevice();
    bt.open();

    SignalHandler enabled;                                  // must cleanly abort scan on signal. - otherwise bluetooth deamon will continue it.

    buffer_t buf;
    bool hasData = false;
    map<uint64_t, Govee> bt_devices;

    cout << "====>\n"; // << bufDataLen << endl;

    for(bt.scan();;) {     // start BLE Scan
        auto& report = *bt.next_adv();                  // tricky buffer :-( We are only interested in report, but have to obtain variable size results

        // check if it's a new devie
        auto addr = bl_addr2int(report.addr);
        if(bt_devices.contains(addr)) {
            if( verbose) 
                std::cout << addr << ": Known\n";
        } else {
            if( verbose) 
                std::cout << addr << ": New\n";
            bt_devices[addr] = Govee{ false};
        }

        auto& device = bt_devices[addr];

        if( report.event_type == 0x0013 ) {
            device.rssi = (int) report.rssi;
            parse_data( device, report.data_len, report.data);          // here we parse memory behind report object (allocated by bt_devices).

            // attach room
            if( !device.found && by_name.contains( device.name)) {
                 device.room = by_name[ device.name].room;
                 device.found = true;
                 by_name[ device.name].found = true;
            }
        }

        if( !verbose)
            std::cout << "\033[H\033[2J\033[3J" ;       // clear term

        // show device found
        for( auto& [k,v]: bt_devices) {   
            if( v.isGovee > 0) {
                auto batteryIcon = v.battery > 30.0 ? "🔋 " : "🪫 ";

                cout << " ⌂ " << std::setw(10) << v.room
                     << " 🌡 " <<  std::setw(5) << std::setprecision(5) << v.temp 
                     << "°C 💧" <<  std::setw(5) << std::setprecision(5) << v.hum << "%" 
                     << "  - " << batteryIcon <<  std::setw(3) <<  v.battery << "%" 
                     << " 📶" << std::dec <<  v.rssi << "db - "  
                     << v.name << " - " 
                     << std::hex << k
                     << "\n"; 
            }
        }

        // show missing from config
        for( auto& [k,v]: by_name) {
            if( !v.found)   
                cout << " x " << std::setw(10) << v.room
                     << " 🌡 " <<  std::setw(5) << "-"
                     << "°C 💧" <<  std::setw(5) << "-" << "%" 
                     << "  - " << "  " <<  std::setw(3) << "-" << "%" 
                     << " 📶" << std::dec << "   " << "db - "  
                     << v.name << " - " 
                     << "   ?   "
                     << "\n";             
        }
    }
} catch(...)    // otherwise abort() is called instead and scan is not disabled
    {}
}
