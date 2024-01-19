# Project - Part 2: Setup Guide
## Table of contents
1. [What changed?](#what-changed)
2. [Commands](#commands)
3. [Server](#server)
4. [Client](#client)

## What changed? <a name="what-changed"></a>
In Part 2 we were meant to design a version of the program where it can actually benefit from using multiprocessing techniques. <br>
Now the event manager works on a server-client basis. The server supports multiple connections with clients and there is only one instance of the EMS database. Communication is handled via named pipes (FIFOs). <br>
Each client processes a single `*.jobs` file sequentally.

For a more detailed explanation make sure to read the [Requirements](./Projeto%20SO%20-%20Parte%202.pdf)

Jump to [Server](#server) to learn how to setup and run the server or [Client](#client) to learn how to send job commands to the server.

## Commands <a name="commands"></a>
| Command + Syntax | Description |
| --- | --- |
| ```CREATE <event_id> <num_rows> <num_cols>``` | Creates a new event with id `event_id` of size `num_rows` * `num_cols` |
| ```RESERVE <event_id> [(<x1>, <y1>) (<x2>, <y2>) ...]``` | Reserves a list of seats to corresponding event |
| ```SHOW <event_id>``` | Shows the corresponding event seat layout |
| ```LIST``` | Lists all events and their id |
| ```WAIT <delay_ms>``` | The client waits for `delay_ms` before sending the next command |
| ```HELP``` | Displays command syntax |
| ```#``` | Indicates a comment (ignored) |
> [!IMPORTANT]
> More information about the command arguments here
<details>
<summary>Argument info</summary>

| Argument | Type |
| --- | --- |
| ```event_id``` | `uint` |
| ```num_rows``` | `uint` |
| ```num_cols``` | `uint` |
| ```[(<x1>, <y1>) (<x2>, <y2>)...]``` | `unsigned_int` - $(x\\_i, y\\_i) \in \\{1..num\\_cols\\} \times \\{1..num\\_rows\\}$ |
| ```delay_ms``` | `ulong` |

> The number of reservations per command is limited by [MAX_RESERVATION_SIZE](./src/common/constants.h)

</details>

## Server <a name="server"></a>
### How to run
This is the syntax of the server process:
```bash
./ems <server_pipe_path> [access_delay]
```
- `server_pipe_path` -> Path for the client registration named pipe
- `access_delay` -> **OPTIONAL:** Adds delay when accessing data

> The server creates the registration pipe.
### Signals
It was required to demonstrate signals by displaying some information about the current status of the server with `SIGUSR1`. <br>
You can send that signal to see what happens.

### How to close
The server runs an infinite loop. To close the server we must send a `SIGINT` signal like this:
```bash
pkill ems -SIGINT
```
> [!NOTE]
> This feature was not required.

## Client <a name="client"></a>
### How to run
This is the syntax of the client process:
```bash
./client <request_pipe_path> <response_pipe_path> <server_pipe_path> <.jobs_file_path>
```
- `request_pipe_path` -> Path for the request pipe (Client send commands through here)
- `response_pipe_path` -> Path for the response pipe (Client receives response from server through here)
- `server_pipe_path` -> Path for the client registration named pipe
- `.jobs_file_path` -> A single command file

> The client creates the request and response pipes.

> [!WARNING]
> Make sure `server_pipe_path` is the same as the server's registration pipe. <br>
> The request and response pipes should be unique for each client.

> [!TIP]
> To test the server with multiple clients you can open multiple terminal windows and launch the clients. <br>
> Or create a simple bash script that launches multiple clients in the background using `&`.