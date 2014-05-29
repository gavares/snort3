/*
** Copyright (C) 2014 Cisco and/or its affiliates. All rights reserved.
**
** This program is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License Version 2 as
** published by the Free Software Foundation.  You may not use, modify or
** distribute this program under any other version of the GNU General
** Public License.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

// tcp_module.h author Russ Combs <rucombs@cisco.com>

#ifndef TCP_MODULE_H
#define TCP_MODULE_H

#include <string>
#include <vector>

#include "snort_types.h"
#include "framework/module.h"

#define GID_STREAM_TCP  129

#define STREAM_TCP_SYN_ON_EST                      1
#define STREAM_TCP_DATA_ON_SYN                     2
#define STREAM_TCP_DATA_ON_CLOSED                  3
#define STREAM_TCP_BAD_TIMESTAMP                   4
#define STREAM_TCP_BAD_SEGMENT                     5
#define STREAM_TCP_WINDOW_TOO_LARGE                6
#define STREAM_TCP_EXCESSIVE_TCP_OVERLAPS          7
#define STREAM_TCP_DATA_AFTER_RESET                8
#define STREAM_TCP_SESSION_HIJACKED_CLIENT         9
#define STREAM_TCP_SESSION_HIJACKED_SERVER        10
#define STREAM_TCP_DATA_WITHOUT_FLAGS             11
#define STREAM_TCP_SMALL_SEGMENT                  12
#define STREAM_TCP_4WAY_HANDSHAKE                 13
#define STREAM_TCP_NO_TIMESTAMP                   14
#define STREAM_TCP_BAD_RST                        15
#define STREAM_TCP_BAD_FIN                        16
#define STREAM_TCP_BAD_ACK                        17
#define STREAM_TCP_DATA_AFTER_RST_RCVD            18
#define STREAM_TCP_WINDOW_SLAM                    19
#define STREAM_TCP_NO_3WHS                        20

//-------------------------------------------------------------------------
// stream_tcp module
//-------------------------------------------------------------------------

#define MOD_NAME "stream_tcp"

struct SnortConfig;
struct StreamTcpConfig;

struct ServiceReassembly
{
    std::string name;
    bool c2s;
    bool s2c;

    ServiceReassembly(std::string&, bool, bool);
};

class StreamTcpModule : public Module
{
public:
    StreamTcpModule();
    bool set(const char*, Value&, SnortConfig*);
    bool begin(const char*, int, SnortConfig*);
    bool end(const char*, int, SnortConfig*);

    unsigned get_gid() const
    { return GID_STREAM_TCP; };

    StreamTcpConfig* get_data();

    void get_port(Port, bool& c2s, bool& s2c);
    const ServiceReassembly* get_proto(unsigned);

private:
    void add_protos(Value&, bool, bool);

private:
    StreamTcpConfig* config;

    PortList ports_client;
    PortList ports_server;
    PortList ports_both;

    std::vector<ServiceReassembly*> protos;
    bool client_protos_set;
};

#endif
