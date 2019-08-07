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
#include "device/serial/serialclass.h"
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

#include <bsoncxx/document/value.hpp>
#include <bsoncxx/document/view.hpp>
#include <bsoncxx/array/element.hpp>
#include <bsoncxx/string/to_string.hpp>
#include <bsoncxx/builder/stream/document.hpp>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/concatenate.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/exception/exception.hpp>
#include <bsoncxx/exception/error_code.hpp>
#include <bsoncxx/types/value.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>
#include <mongocxx/stdx.hpp>
#include <mongocxx/uri.hpp>
#include <mongocxx/collection.hpp>
#include <mongocxx/bulk_write.hpp>
#include <mongocxx/exception/bulk_write_exception.hpp>
#include <mongocxx/exception/query_exception.hpp>
#include <mongocxx/exception/logic_error.hpp>
#include <mongocxx/cursor.hpp>
#include <mongocxx/options/find.hpp>

#include <server_ws.hpp>
#include <client_ws.hpp>

using WsServer = SimpleWeb::SocketServer<SimpleWeb::WS>;
using WsClient = SimpleWeb::SocketClient<SimpleWeb::WS>;
using namespace bsoncxx;
using bsoncxx::builder::basic::kvp;
using namespace bsoncxx::builder::stream;
namespace fs = std::experimental::filesystem;

static Agent *agent;

static mongocxx::instance instance
{};

// Connect to a MongoDB URI and establish connection
static mongocxx::client connection_ring
{
    mongocxx::uri {
        "mongodb://server:27017/"
    }
};

static mongocxx::client connection_file
{
    mongocxx::uri {
        "mongodb://server:27017/"
    }
};

//! Options available to specify when querying a Mongo database
enum class MongoFindOption
{
    //! If some shards are unavailable, it returns partial results if true.
    ALLOW_PARTIAL_RESULTS,
    //! The number of documents to return in the first batch.
    BATCH_SIZE,
    //! Specify language specific rules for string comparison.
    COLLATION,
    //! Comment to attach to query to assist in debugging
    COMMENT,
    //! The cursor type
    CURSOR_TYPE,
    //! Specify index name
    HINT,
    //! The limit of how many documents you retrieve.
    LIMIT,
    //! Get the upper bound for an index.
    MAX,
    //! Max time for the server to wait on new documents to satisfy cursor query
    MAX_AWAIT_TIME,
    //! Deprecated
    MAX_SCAN,
    //! Max time for the oepration to run in milliseconds on the server
    MAX_TIME,
    //! Inclusive lower bound for index
    MIN,
    //! Prevent cursor from timing out server side due to activity.
    NO_CURSOR_TIMEOUT,
    //! Projection which limits the returned fields for the matching documents
    PROJECTION,
    //! Read preference
    READ_PREFERENCE,
    //! Deprecated
    MODIFIERS,
    //! Deprecated
    RETURN_KEY,
    //! Whether to include record identifier in results
    SHOW_RECORD_ID,
    //! Specify the number of documents to skip when querying
    SKIP,
    //! Deprecated
    SNAPSHOT,
    //! Order to return the matching documents.
    SORT,
    INVALID
};

std::string execute(std::string cmd);
void get_packets();
void file_walk(std::string &database, std::vector<std::string> &included_nodes, std::vector<std::string> &excluded_nodes);
map<std::string, std::string> get_keys(const std::string &request, const std::string variable_delimiter, const std::string value_delimiter);
void str_to_lowercase(std::string &input);
MongoFindOption option_table(std::string input);
void set_mongo_options(mongocxx::options::find &options, std::string request);
void maintain_agent_list(std::vector<std::string> &included_nodes, std::vector<std::string> &excluded_nodes);

static thread get_packets_thread;

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

//! Convert a given option and return the enumerated value
/*!
   \param input The option
   \return The enumerated MongoDB find option
*/
MongoFindOption option_table(std::string input)
{
    str_to_lowercase(input);

    if (input == "allow_partial_results") return MongoFindOption::ALLOW_PARTIAL_RESULTS;
    if (input == "batch_size") return MongoFindOption::BATCH_SIZE;
    if (input == "coalition") return MongoFindOption::COLLATION;
    if (input == "comment") return MongoFindOption::COMMENT;
    if (input == "cursor_type") return MongoFindOption::CURSOR_TYPE;
    if (input == "hint") return MongoFindOption::HINT;
    if (input == "limit") return MongoFindOption::LIMIT;
    if (input == "max") return MongoFindOption::MAX;
    if (input == "max_await_time") return MongoFindOption::MAX_AWAIT_TIME;
    if (input == "max_scan") return MongoFindOption::MAX_SCAN;
    if (input == "max_time") return MongoFindOption::MAX_TIME;
    if (input == "min") return MongoFindOption::MIN;
    if (input == "no_cursor_timeout") return MongoFindOption::NO_CURSOR_TIMEOUT;
    if (input == "projection") return MongoFindOption::PROJECTION;
    if (input == "read_preferences") return MongoFindOption::READ_PREFERENCE;
    if (input == "modifiers") return MongoFindOption::MODIFIERS;
    if (input == "return_key") return MongoFindOption::RETURN_KEY;
    if (input == "show_record_id") return MongoFindOption::SHOW_RECORD_ID;
    if (input == "skip") return MongoFindOption::SKIP;
    if (input == "snapshot") return MongoFindOption::SNAPSHOT;
    if (input == "sort") return MongoFindOption::SORT;

    return MongoFindOption::INVALID;
}

//! Set the MongoDB find options in the option class given a JSON object of options
/*!
  \param options The MongoDB find option class to append the options to
  \param request A JSON object of wanted options
*/
void set_mongo_options(mongocxx::options::find &options, std::string request)
{
    bsoncxx::document::value json = bsoncxx::from_json(request);
    bsoncxx::document::view opt { json.view() };

    for (auto e : opt)
    {
        std::string key = std::string(e.key());

        MongoFindOption option = option_table(key);

        switch(option)
        {
            case MongoFindOption::ALLOW_PARTIAL_RESULTS:
                if (e.type() == type::k_int32)
                {
                    options.allow_partial_results(e.get_int32().value);
                }
                else if (e.type() == type::k_int64)
                {
                    options.allow_partial_results(e.get_int64().value);
                }
                break;
            case MongoFindOption::BATCH_SIZE:
                if (e.type() == type::k_bool) {
                    options.batch_size(e.get_bool().value);
                }
                break;
//            case MongoFindOption::COLLATION:
//                options.batch_size(e.get_int32().value); // string view or value
//                break;
            case MongoFindOption::LIMIT:
                if (e.type() == type::k_int32)
                {
                    options.limit(e.get_int32().value);
                }
                else if (e.type() == type::k_int64)
                {
                    options.limit(e.get_int64().value);
                }
                break;
//            case MongoFindOption::MAX:
//                options.max(e.get_document()); // bson view or value
//            case MongoFindOption::MAX_AWAIT_TIME:
//                options.max_await_time(e.get_date()); // chronos
//            case MongoFindOption::MAX_TIME:server
//                options.max_time() // chronos
//            case MongoFindOption::MIN:
//                options.min(e.get_document()) // bson view or value
            case MongoFindOption::NO_CURSOR_TIMEOUT:
                if (e.type() == type::k_bool)
                {
                   options.no_cursor_timeout(e.get_bool().value);
                }
                break;
//            case MongoFindOption::PROJECTION:
//                options.projection() // bson view or value
            case MongoFindOption::RETURN_KEY:
                if (e.type() == type::k_bool)
                {
                    options.return_key(e.get_bool().value);
                }
                break;
            case MongoFindOption::SHOW_RECORD_ID:
                if (e.type() == type::k_bool)
                {
                    options.show_record_id(e.get_bool().value);
                }
                break;
            case MongoFindOption::SKIP:
                if (e.type() == type::k_int32)
                {
                    options.skip(e.get_int32().value);
                } else if (e.type() == type::k_int64) {
                    options.skip(e.get_int64().value);
                }
                break;
//            case MongoFindOption::SORT:
//                options.sort(e.get_document()); // bson view or value
            default:
                break;
        }
    }
}

int main(int argc, char** argv)
{
    cout << "Agent Socket" << endl;
    agent = new Agent("", "socket", 1, AGENTMAXBUFFER, false, 20302, NetworkType::UDP, 1);

    if (agent->cinfo == nullptr)
    {
        cout << "Unable to start agent_socket" << endl;
        exit(1);
    }

    WsServer ws;
    ws.config.port = 8080;

    // Endpoints for querying the database. Goes to /query/
    // Example query message: database=db?collection=test?multiple=true?query={"cost": { "$lt": 11 }}?options={"limit": 5}
    // database: the database name
    // collection: the collection name
    // multiple: whether to return in array format/multiple
    // query: JSON querying the mongo database. See MongoDB docs for more complex queries
    // options: JSON options
    auto &packets = ws.endpoint["^/packets?$"];

    packets.on_message = [&packets](std::shared_ptr<WsServer::Connection> ws_connection, std::shared_ptr<WsServer::InMessage> ws_message)
    {
        std::string message = ws_message->string();

        auto out_message = ws_message->string();

        // echo_all.get_connections() can also be used to solely receive connections on this endpoint
        for(auto &endpoint_connections : packets.get_connections())
        {
            if (ws_connection->path == endpoint_connections->path || endpoint_connections->path == "/live/all" || endpoint_connections->path == "/live/all/")
            {
                endpoint_connections->send(out_message);
            }
        }
    };

    packets.on_open = [](std::shared_ptr<WsServer::Connection> connection)
    {
      cout << "Server: Opened connection " << connection.get() << endl;
      // send token when connected
    };

    packets.on_error = [](std::shared_ptr<WsServer::Connection> connection, const SimpleWeb::error_code &ec)
    {
      cout << "WS Query: Error in connection " << connection.get() << ". "
           << "Error: " << ec << ", error message: " << ec.message() << endl;
    };

    thread ws_thread([&ws]()
    {
      // Start WS-server
      ws.start();
    });

    get_packets_thread = thread(get_packets);

    while(agent->running())
    {
        // Sleep for 1 sec
        COSMOS_SLEEP(0.1);
    }

    agent->shutdown();
    ws_thread.join();
    get_packets_thread.join();

    return 0;
}

void get_packets()
{
    while (agent->running())
    {
        char ebuffer[6]="[NOK]";
        ssize_t iretn = -1;
        int sock = socket(AF_INET, SOCK_DGRAM, 0);
        unsigned long bufferlen = 1024;

        char bufferin[bufferlen];

        char *hello = "Hi";

        struct sockaddr_in address;
        int len = sizeof(address);

        address.sin_family = AF_INET;
        address.sin_port = htons(10000);
        inet_pton(AF_INET, "localhost", &address.sin_addr);

        sendto(sock, (const char *)hello, strlen(hello), 0, (const struct sockaddr *) &address, sizeof(address));

        // Receiving socket data
        iretn = recvfrom(
            sock,
            reinterpret_cast<char *>(bufferin),
            bufferlen,
            0,
            reinterpret_cast<struct sockaddr *>(&address),
            reinterpret_cast<socklen_t *>(&len)
        );

        // If there is a message from the socket run process
        if (iretn > 0) {
            std::cout << "Receiving: " << bufferin << std::endl;

            if (iretn > 0)
            {
                WsClient client("localhost:8080/packets");

                client.on_open = [&bufferin](std::shared_ptr<WsClient::Connection> connection)
                {
                    cout << "WS Agent Live: Broadcasted updated agent list" << endl;

                    connection->send(bufferin);

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

            close(sock);

        }
        COSMOS_SLEEP(10);
    }
}
