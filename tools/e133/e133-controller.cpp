/*
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * e133-controller.cpp
 * Copyright (C) 2011 Simon Newton
 */

#include "plugins/e131/e131/E131Includes.h"  //  NOLINT, this has to be first
#include <errno.h>
#include <getopt.h>
#include <sysexits.h>

#include <ola/BaseTypes.h>
#include <ola/Callback.h>
#include <ola/Logging.h>
#include <ola/network/IPV4Address.h>
#include <ola/network/NetworkUtils.h>
#include <ola/rdm/RDMCommand.h>
#include <ola/rdm/RDMEnums.h>
#include <ola/rdm/RDMHelper.h>
#include <ola/rdm/UID.h>

#include <iostream>
#include <string>
#include <vector>

#include "plugins/e131/e131/ACNPort.h"

#include "E133Node.h"
#include "E133UniverseController.h"

using std::string;
using ola::network::IPV4Address;
using ola::rdm::RDMRequest;
using ola::rdm::UID;

typedef struct {
  bool help;
  ola::log_level log_level;
  unsigned int universe;
  string ip_address;
  string target_address;
  UID *uid;
} options;


static unsigned int responses_to_go;
static unsigned int transaction_number = 0;



/*
 * Parse our command line options
 */
void ParseOptions(int argc, char *argv[], options *opts) {
  int uid_set = 0;
  static struct option long_options[] = {
      {"help", no_argument, 0, 'h'},
      {"ip", required_argument, 0, 'i'},
      {"log-level", required_argument, 0, 'l'},
      {"target", required_argument, 0, 't'},
      {"universe", required_argument, 0, 'u'},
      {"uid", required_argument, &uid_set, 1},
      {0, 0, 0, 0}
    };

  int option_index = 0;

  while (1) {
    int c = getopt_long(argc, argv, "hi:l:t:u:", long_options, &option_index);

    if (c == -1)
      break;

    switch (c) {
      case 0:
        if (uid_set)
          opts->uid = UID::FromString(optarg);
        break;
      case 'h':
        opts->help = true;
        break;
      case 'i':
        opts->ip_address = optarg;
        break;
      case 'l':
        switch (atoi(optarg)) {
          case 0:
            // nothing is written at this level
            // so this turns logging off
            opts->log_level = ola::OLA_LOG_NONE;
            break;
          case 1:
            opts->log_level = ola::OLA_LOG_FATAL;
            break;
          case 2:
            opts->log_level = ola::OLA_LOG_WARN;
            break;
          case 3:
            opts->log_level = ola::OLA_LOG_INFO;
            break;
          case 4:
            opts->log_level = ola::OLA_LOG_DEBUG;
            break;
          default :
            break;
        }
        break;
      case 't':
        opts->target_address = optarg;
        break;
      case 'u':
        opts->universe = atoi(optarg);
        break;
      case '?':
        break;
      default:
       break;
    }
  }
  return;
}


/*
 * Display the help message
 */
void DisplayHelpAndExit(char *argv[]) {
  std::cout << "Usage: " << argv[0] << " [options]\n"
  "\n"
  "Send a  E1.33 Message.\n"
  "\n"
  "  -h, --help         Display this help message and exit.\n"
  "  -t, --target       IP to send the message to\n"
  "  -i, --ip           The IP address to listen on.\n"
  "  -l, --log-level <level>   Set the loggging level 0 .. 4.\n"
  "  -u, --universe <universe> The universe to respond on (> 0).\n"
  "  --uid <uid>               The UID of the device to control.\n"
  << std::endl;
  exit(0);
}


void RequestCallback(E133Node *node,
                     ola::rdm::rdm_response_code rdm_code,
                     const ola::rdm::RDMResponse *response,
                     const std::vector<std::string> &packets) {
  OLA_INFO << "Callback executed with code: " <<
    ola::rdm::ResponseCodeToString(rdm_code);

  if (rdm_code == ola::rdm::RDM_COMPLETED_OK) {
    OLA_INFO << response->SourceUID() << " -> " << response->DestinationUID()
      << ", TN: " << (int) response->TransactionNumber() << ", Msg Count: " <<
      (int) response->MessageCount() << ", sub dev: " << response->SubDevice()
      << ", param 0x" << std::hex << response->ParamId() << ", data len: " <<
      std::dec << (int) response->ParamDataSize();
  }
  delete response;

  if (!--responses_to_go)
    node->Stop();
  (void) packets;
}


void SendGetRequest(UID &src_uid,
                    UID &dst_uid,
                    E133Node *node,
                    E133UniverseController *controller,
                    ola::rdm::rdm_pid pid) {
  // send a second one
  ola::rdm::RDMGetRequest *command = new ola::rdm::RDMGetRequest(
      src_uid,
      dst_uid,
      transaction_number++,  // transaction #
      1,  // port id
      0,  // message count
      ola::rdm::ROOT_RDM_DEVICE,  // sub device
      pid,  // param id
      NULL,  // data
      0);  // data length

  ola::rdm::RDMCallback *callback = ola::NewSingleCallback(RequestCallback,
                                                           node);
  controller->SendRDMRequest(command, callback);
  responses_to_go++;
}


void SendSetRequest(UID &src_uid,
                    UID &dst_uid,
                    E133Node *node,
                    E133UniverseController *controller,
                    ola::rdm::rdm_pid pid,
                    const uint8_t *data,
                    unsigned int data_length) {
  ola::rdm::RDMSetRequest *command = new ola::rdm::RDMSetRequest(
      src_uid,
      dst_uid,
      transaction_number++,  // transaction #
      1,  // port id
      0,  // message count
      ola::rdm::ROOT_RDM_DEVICE,  // sub device
      pid,  // param id
      data,  // data
      data_length);  // data length

  ola::rdm::RDMCallback *callback = ola::NewSingleCallback(RequestCallback,
                                                           node);
  controller->SendRDMRequest(command, callback);
  responses_to_go++;
}


/*
 * Startup a node
 */
int main(int argc, char *argv[]) {
  options opts;
  opts.log_level = ola::OLA_LOG_WARN;
  opts.universe = 1;
  opts.help = false;
  opts.uid = NULL;
  ParseOptions(argc, argv, &opts);

  if (opts.help)
    DisplayHelpAndExit(argv);

  ola::InitLogging(opts.log_level, ola::OLA_LOG_STDERR);

  IPV4Address target_ip;
  if (!IPV4Address::FromString(opts.target_address, &target_ip))
    DisplayHelpAndExit(argv);

  if (!opts.uid) {
    OLA_FATAL << "Invalid UID";
    exit(EX_USAGE);
  }
  UID dst_uid(*opts.uid);
  delete opts.uid;

  UID src_uid(OPEN_LIGHTING_ESTA_CODE, 0xabcdabcd);

  E133Node node(opts.ip_address, ola::plugin::e131::ACN_PORT + 1);
  if (!node.Init()) {
    exit(EX_UNAVAILABLE);
  }

  E133UniverseController universe_controller(opts.universe);
  universe_controller.AddUID(dst_uid, target_ip);
  node.RegisterComponent(&universe_controller);

  // send some rdm get messages
  SendGetRequest(src_uid, dst_uid, &node, &universe_controller,
                 ola::rdm::PID_DEVICE_INFO);
  SendGetRequest(src_uid, dst_uid, &node,
                 &universe_controller,
                 ola::rdm::PID_SOFTWARE_VERSION_LABEL);

  // now send a set message
  uint16_t start_address = 10;
  start_address = ola::network::HostToNetwork(start_address);
  SendSetRequest(src_uid,
                 dst_uid,
                 &node,
                 &universe_controller,
                 ola::rdm::PID_DMX_START_ADDRESS,
                 reinterpret_cast<uint8_t*>(&start_address),
                 sizeof(start_address));

  if (responses_to_go)
    node.Run();
  node.UnRegisterComponent(&universe_controller);
}