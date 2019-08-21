# Socket

An alternative agent to agent_mongo if you do not need the database portion.

## Requirements

* C++17
* GCC 7+
* Crypto
* OpenSSL

## Installing

```bash
# Clone repository
git clone https://github.com/spjy/cosmos-socket.git
mkdir build
cmake ../source
make
```

## Running

```
./agent_socket
```

## Options

### Websocket

The websocket query listening port is 8080. These are socket requests are used to get data from the agent.
The websocket live listening port is 8081. These socket requests are used to get data as it is coming in.

#### /command/
Initialize an agent request, e.g. `agent list`. It runs the command in the command line and returns the output. Omit the `agent `  prefix and only include the command after that (e.g. if I wanted `agent list` I would include only `list` in the body.

**Body**: command

**Options**: -

**Return**: The output of the agent request.

#### /live/
This is a listen-only endpoint. As data is flowing in from any node/process, it will be sent out on this endpoint.

**Body**: None

**Options**: None

**Return**: Data from every node/process in JSON format.

### Command Line Options

* --include
  * A comma delimited list of nodes as strings to not save data to the database or can contain a wildcard to include all nodes.
* --exclude
  * A comma delimited list of nodes as strings to save data to the database.
* --database
  * The database to save agent data to.

**Examples**:

Including and excluding certain nodes:
```bash
--include "cubesat1,hsflpc-03,neutron1" --exclude "node1,node-arduino" --database "agent_dump"
```

Including all and excluding certain nodes:
```bash
--include "*" --exclude "node1,node-arduino"
```
