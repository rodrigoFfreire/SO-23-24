# Project - Part 1: Setup Guide
## Table of contents
1. [What does it do?](#what-does-it-do)
2. [Commands](#commands)
3. [Job Files](#job-files)
4. [Running the Program](#run)

## What does it do? <a name="what-does-it-do"></a>
The goal of this project was to build a program that manages events.<br>
We can interact with it by:
- Creating multiple events with different sizes (number of seats)
- Reserve one or multiple seats for an event
- Show an event's layout (displays a matrix with current status of each seat)
- List all created events
- Control thread execution with wait injection and barriers

For a more detailed explanation make sure to read the [Requirements](./Projeto%20SO%20-%20Parte%201.pdf)

This interaction occurs via command files. In [Part 1](./Part1) the program creates a subprocess for each command file.
Then each process runs a maximum number of threads that execute commands.<br>
Each process however is associated to a unique instance of the event manager (One system for each file). <br>
Moreover, in this part commands are not ran in order! This will cause unexpected behaviour. This is meant to demonstrate when not to use multiprocessing. <br>

Jump to [Job Files](#job-files) to learn how to setup these files or [Running the Program](#run) to learn how to run this program.

## Commands <a name="commands"></a>
| Command + Syntax | Description |
| --- | --- |
| ```CREATE <event_id> <num_rows> <num_cols>``` | Creates a new event with id `event_id` of size `num_rows` * `num_cols` |
| ```RESERVE <event_id> [(<x1>, <y1>) (<x2>, <y2>) ...]``` | Reserves a list of seats to corresponding event |
| ```SHOW <event_id>``` | Shows the corresponding event seat layout |
| ```LIST``` | Lists all events and their id |
| ```WAIT <delay_ms> [thread_id]``` | If `thread_id` is not specified all threads wait for `delay_ms` otherwise only the specified thread waits |
| ```BARRIER``` | Program will wait for all threads to finish processing their command and restarts a new round of processing |
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
| ```thread_id``` | `uint` - $thread\\_id \in \\{0..max\\_threads\\}$ |

> The number of reservations per command is limited by [MAX_RESERVATION_SIZE](./src/constants.h)

</details>

## Job Files <a name="job-files"></a>
First, create a directory for our job files. It can be something like `jobs`.<br>
Add a new job file to the directory. It should be named like this: `<job_name>.jobs` <br>
You can look at this [example](./example-job.jobs)
> [!TIP]
> You can change the extension name by changing `JOBS_FILE_EXTENSION` in [constants.h](./src/constants.h)

## Running the Program <a name="run"></a>
Here is the program syntax:
```bash
./ems <jobs_dir> <max_processes> <max_threads> [access_delay]
```
- `jobs_dir_path` -> Jobs directory
- `max_processes` -> Maximum number of allowed processes running
- `max_threads` -> Maximum number of threads running per process
- `access_delay` -> **OPTIONAL:** Adds delay when accessing data