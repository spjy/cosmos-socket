/********************************************************************
* Copyright (C) 2015 by Interstel Technologies, Inc.
*   and Hawaii Space Flight Laboratory.
*
* This file is part of the COSMOS/core that is the central
* module for COSMOS. For more information on COSMOS go to
* <http://cosmos-project.com>
*
* The COSMOS/core software is licenced under the
* GNU Lesser General Public License (LGPL) version 3 licence.
*
* You should have received a copy of the
* GNU Lesser General Public License
* If not, go to <http://www.gnu.org/licenses/>
*
* COSMOS/core is free software: you can redistribute it and/or
* modify it under the terms of the GNU Lesser General Public License
* as published by the Free Software Foundation, either version 3 of
* the License, or (at your option) any later version.
*
* COSMOS/core is distributed in the hope that it will be useful, but
* WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
* Lesser General Public License for more details.
*
* Refer to the "licences" folder for further information on the
* condititons and terms to use this software.
********************************************************************/

// TODO: change include paths so that we can make reference to cosmos using a full path
// example
// #include <cosmos/core/agent/agentclass.h>

#include "agent/agentclass.h"
#include "support/configCosmos.h"
#include "agent/agentclass.h"
#include "support/socketlib.h"
#include "support/stringlib.h"
#include "support/jsondef.h"
#include "support/jsonlib.h"
#include "support/datalib.h"

#include <string.h>
#include <cstring>
#include <iterator>
#include <iostream>
#include <fstream>
#include <thread>
#include <string>
#include <vector>
#include <map>
#include <locale>
#include <memory>
#include <cstdio>
#include <stdexcept>
#include <array>
#include <experimental/filesystem>
#include <set>

#include <server_ws.hpp>
#include <client_ws.hpp>

using WsServer = SimpleWeb::SocketServer<SimpleWeb::WS>;
using WsClient = SimpleWeb::SocketClient<SimpleWeb::WS>;

static Agent *agent;

//! Options available to specify when querying a Mongo database
//int32_t request_change_socket_address(char *request, char *respsonse, Agent *);
//int32_t request_change_socket_port(char *request, char *respsonse, Agent *);


void collect_data_loop(std::vector<std::string> &included_nodes, std::vector<std::string> &excluded_nodes);
std::string execute(std::string cmd);
void get_packets();
map<std::string, std::string> get_keys(const std::string &request, const std::string variable_delimiter, const std::string value_delimiter);
void str_to_lowercase(std::string &input);

static thread get_packets_thread;
static thread collect_data_loop_thread;

/*! Run a command line script and get the output of it.
 * \brief execute Use popen to run a command line script and get the output of the command.
 * \param cmd the command to run
 * \return the output from the command that was run
 */
std::string execute(std::string cmd)
{
    try {
        std::string data;
        FILE * stream;
        const int max_buffer = 256;
        char buffer[max_buffer];
        cmd.insert(0, "~/cosmos/bin/agent ");
        cmd.append(" 2>&1");

        stream = popen(cmd.c_str(), "r");
        if (stream)
        {
            while (!feof(stream))
            {
                if (fgets(buffer, max_buffer, stream) != NULL) data.append(buffer);
            }
            pclose(stream);
        }
        return data;
    } catch (...) {
        return std::string();
    }

}

/*! Check whether a vector contains a certain value.
 * \brief vector_contains loop through the vector and check that a certain value is identical to one inside the vector
 * \param input_vector
 * \param value
 * \return boolean: true if it was found inthe vector, false if not.
 */
bool vector_contains(vector<std::string> &input_vector, std::string value)
{
    for (vector<std::string>::iterator it = input_vector.begin(); it != input_vector.end(); ++it)
    {
        if (*it == value)
        {
            return true;
        }
    }
    return false;
}


/*! Check whether to save data from a node from command line/file specification
 * \brief whitelisted_node loop through the included/excluded vectors and check if the node is contained in either one.
 * \param included_nodes vector of included nodes
 * \param excluded_nodes vector of excluded nodes
 * \param node the node to check against vectors
 * \return whether the node is whitelisted; true if it is, false if it is not.
 */
bool whitelisted_node(vector<std::string> &included_nodes, vector<std::string> &excluded_nodes, std::string &node)
{
    bool whitelisted = false;

    // Check if the node is on the included list, if so return true

    // if not, continue and check if included list contains the wildcard

    // if it contains the wildcard, check if the node is on the excluded list

    if (vector_contains(included_nodes, node))
    {
        whitelisted = true;
    }
    else
    {
        if (vector_contains(excluded_nodes, node))
        {
            whitelisted = false;
        }
        else if (vector_contains(included_nodes, "*"))
        {
            whitelisted = true;
        }
    }

    return whitelisted;
}

//! Retrieve a request consisting of a list of key values and assign them to a map.
 /*! \brief Split up the list of key values by a specified delimiter, then split up the key values by a specified delimiter and assign the key to a map with its corresponding value.
 \param request The request to assign to a map
 \param variable_delimiter The delimiter that separates the list of key values
 \param value_delimiter The delimiter that separates the key from the value
 \return map<std::string, std::string>
*/

map<std::string, std::string> get_keys(const std::string &request, const std::string variable_delimiter, const std::string value_delimiter)
{
    // Delimit by key value pairs
    vector<std::string> input = string_split(request, variable_delimiter);
    map<std::string, std::string> keys;

    // Delimit
    for (vector<std::string>::iterator it = input.begin(); it != input.end(); ++it)
    {
        vector<std::string> kv = string_split(*it, value_delimiter);

        keys[kv[0]] = kv[1]; // Set the variable to the map key and assign the corresponding value
    }

    return keys;
}

//! Convert the characters in a given string to lowercase
/*!
 \param input The string to convert to lowercase
 \return void
*/
void str_to_lowercase(std::string &input)
{
    std::locale locale;
    for (std::string::size_type i = 0; i < input.length(); ++i)
    {
        tolower(input[i], locale);
    }
}

int main(int argc, char** argv)
{
    cout << "Agent Socket" << endl;
    agent = new Agent("", "socket", 1, AGENTMAXBUFFER, false, 20302, NetworkType::UDP, 1);

//    agent->add_request("change_socket_address", request_change_socket_address, "", "Change the address of the socket.");
//    agent->add_request("change_socket_port", request_change_socket_port, "", "Change the port of the socket.");

    if (agent->cinfo == nullptr)
    {
        cout << "Unable to start agent_socket" << endl;
        exit(1);
    }

    std::vector<std::string> included_nodes;
    std::vector<std::string> excluded_nodes;
    std::string nodes_path;
    std::string database = "db";

    // Get command line arguments for including/excluding certain nodes
    // If include nodes by file, include path to file through --whitelist_file_path
    for (int i = 1; i < argc; i++)
    {
        // Look for flags and see if the value exists
        if (argv[i][0] == '-' && argv[i][1] == '-' && argv[i + 1] != nullptr)
        {
            if (strncmp(argv[i], "--include", sizeof(argv[i]) / sizeof(argv[i][0])) == 0)
            {
                included_nodes = string_split(argv[i + 1], ",");
            }
            else if (strncmp(argv[i], "--exclude", sizeof(argv[i]) / sizeof(argv[i][0])) == 0)
            {
                excluded_nodes = string_split(argv[i + 1], ",");
            }
        }
    }

    if (included_nodes.empty() && excluded_nodes.empty() && nodes_path.empty())
    {
        included_nodes.push_back("*");
    }

    cout << "Including nodes: ";

    for (std::string s : included_nodes)
    {
        cout << s + " ";
    }

    cout << endl;

    cout << "Excluding nodes: ";

    for (std::string s : excluded_nodes)
    {
        cout << s + " " << endl;
    }

    WsServer ws_live;
    ws_live.config.port = 8081;

    WsServer ws_query;
    ws_query.config.port = 8080;

    // Endpoints for querying the database. Goes to /query/
    // Example query message: database=db?collection=test?multiple=true?query={"cost": { "$lt": 11 }}?options={"limit": 5}
    // database: the database name
    // collection: the collection name
    // multiple: whether to return in array format/multiple
    // query: JSON querying the mongo database. See MongoDB docs for more complex queries
    // options: JSON options
    // For live requests, to broadcast to all clients. Goes to /live/node_name/
    auto &echo_all = ws_live.endpoint["^/live/(.+)/?$"];

    echo_all.on_message = [&echo_all](std::shared_ptr<WsServer::Connection> connection, std::shared_ptr<WsServer::InMessage> in_message)
    {
      auto out_message = in_message->string();

      // echo_all.get_connections() can also be used to solely receive connections on this endpoint
      for(auto &endpoint_connections : echo_all.get_connections())
      {
          if (connection->path == endpoint_connections->path || endpoint_connections->path == "/live/all" || endpoint_connections->path == "/live/all/")
          {
              endpoint_connections->send(out_message);
          }
      }
    };

    auto &command = ws_query.endpoint["^/command/?$"];

    command.on_message = [](std::shared_ptr<WsServer::Connection> ws_connection, std::shared_ptr<WsServer::InMessage> ws_message)
    {
        std::string message = ws_message->string();

        std::string result = execute(message);

        ws_connection->send(result, [](const SimpleWeb::error_code &ec)
        {
            if (ec) {
                cout << "WS Command: Error sending message. " << ec.message() << endl;
            }
        });
    };

    thread ws_thread([&ws_live]()
    {
      // Start WS-server

      ws_live.start();
    });

    thread query_thread([&ws_query]()
    {
      // Start WS-server

      ws_query.start();
    });

    get_packets_thread = thread(get_packets);
    collect_data_loop_thread = thread(collect_data_loop, std::ref(included_nodes), std::ref(excluded_nodes));

    while(agent->running())
    {
        // Sleep for 1 sec
        COSMOS_SLEEP(0.1);
    }

    agent->shutdown();
    ws_thread.join();
    get_packets_thread.join();
    collect_data_loop_thread.join();

    return 0;
}

void get_packets()
{

    char buffer[1024];
    int listenfd, len;
    struct sockaddr_in servaddr, cliaddr;
    bzero(&servaddr, sizeof(servaddr));

    // Create a UDP Socket
    listenfd = socket(AF_INET, SOCK_DGRAM, 0);
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(5005);
    servaddr.sin_family = AF_INET;

    // bind server address to socket descriptor
    bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr));

    while (agent->running())
    {
        ssize_t iretn = -1;

        //receive the datagram
        len = sizeof(cliaddr);
        iretn = recvfrom(listenfd, buffer, sizeof(buffer), 0, reinterpret_cast<struct sockaddr*>(&cliaddr), reinterpret_cast<socklen_t *>(&len)); //receive message from server
        buffer[iretn] = '\0';
        puts(buffer);

        // If there is a message from the socket run process
        if (iretn > 0)
        {
            cout << iretn << endl << buffer << endl;

            WsClient client("localhost:8081/live/activity");

            std::string response = "{\"node_type\": \"activity\", \"activity\": \"" + static_cast<std::string>(buffer) + "\", \"utc\": " + std::to_string(currentmjd()) + " }";

            cout << response << endl;

            client.on_open = [&response](std::shared_ptr<WsClient::Connection> connection)
            {
                cout << "WS Agent Live: Broadcasted activity" << endl;

                connection->send(response);

                connection->send_close(1000);
            };

            client.on_close = [](std::shared_ptr<WsClient::Connection> /*connection*/, int status, const std::string & /*reason*/)
            {
                if (status != 1000) {
                    cout << "WS Live: Closed connection with status code " << status << endl;
                }
            };

            // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
            client.on_error = [](std::shared_ptr<WsClient::Connection> /*connection*/, const SimpleWeb::error_code &ec)
            {
                cout << "WS Live: Error: " << ec << ", error message: " << ec.message() << endl;
            };

            client.start();
        }

        COSMOS_SLEEP(1.);
    }

    close(listenfd);
}

//! The method to handle incoming telemetry data to write it to the database
/*!
 *
 * \param connection MongoDB connection instance
 */
void collect_data_loop(std::vector<std::string> &included_nodes, std::vector<std::string> &excluded_nodes)
{
    while (agent->running())
    {
        int32_t iretn;

        Agent::messstruc message;
        iretn = agent->readring(message, Agent::AgentMessage::ALL, 1., Agent::Where::TAIL);

        if (iretn > 0)
        {
            // First use reference to adata to check conditions
            std::string *padata = &message.adata;

            // If no content in adata, don't continue or write to database
            if (!padata->empty() && padata->front() == '{' && padata->back() == '}')
            {
                // Extract node from jdata
                std::string node = json_extract_namedmember(message.jdata, "agent_node");
                std::string type = json_extract_namedmember(message.jdata, "agent_proc");

                // Remove leading and trailing quotes around node
                node.erase(0, 1);
                node.pop_back();

                type.erase(0, 1);
                type.pop_back();

                std::string node_type = node + ":" + type;

                if (whitelisted_node(included_nodes, excluded_nodes, node)) {
                    std::string ip = "localhost:8081/live/" + node_type;
                    // Websocket client here to broadcast to the WS server, then the WS server broadcasts to all clients that are listening
                    WsClient client(ip);

                    std::string adata = message.adata;
                    adata.pop_back();
                    adata.insert(adata.size(), ", \"utc\": " + std::to_string(message.meta.beat.utc));
                    adata.insert(adata.size(), ", \"node_type\": \"" + node_type + "\"}");

                    client.on_open = [&adata, &node_type](std::shared_ptr<WsClient::Connection> connection)
                    {
                        cout << "WS Live: Broadcasted adata for " << node_type << endl;

                        connection->send(adata);

                        connection->send_close(1000);
                    };

                    client.on_close = [](std::shared_ptr<WsClient::Connection> /*connection*/, int status, const std::string & /*reason*/)
                    {
                        if (status != 1000) {
                        cout << "WS Live: Closed connection with status code " << status << endl;
                        }
                    };

                    // See http://www.boost.org/doc/libs/1_55_0/doc/html/boost_asio/reference.html, Error Codes for error code meanings
                    client.on_error = [](std::shared_ptr<WsClient::Connection> /*connection*/, const SimpleWeb::error_code &ec)
                    {
                        cout << "WS Live: Error: " << ec << ", error message: " << ec.message() << endl;
                    };

                    client.start();
                }
            }
        }
        COSMOS_SLEEP(.5);
    }
    return;
}

//int32_t request_change_socket_address(char *request, char *response, Agent *) {
//    char new_address[strlen(request)];

//    sscanf("change_socket_address %s", new_address);

//    inet_pton(AF_INET, new_address, &address.sin_addr);

//    std::string a = new_address;

//    cout << new_address[0] << new_address[1] << endl;
//    len = sizeof(address);

//    return 0;
//}

//int32_t request_change_socket_port(char *request, char *response, Agent *) {
//    char port[strlen(request)];

//    sscanf("change_socket_port %s", port);

//    address.sin_port = htons(atoi(port));
//    len = sizeof(address);

//    cout << atoi(port) << endl;

//    return 0;
//}
