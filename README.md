## 🚀 TCP/UDP Message Broker and Subscriber Application

This application implements a **Client-Server message platform** utilizing both **TCP** and **UDP** protocols. The server acts as a central **message broker**, distributing messages from UDP clients (publishers) to connected TCP clients (subscribers) based on topic subscriptions.

---

## 1. ✨ Core Components and Functionality

| Component | Protocol | Key Responsibilities |
| :---: | :---: | :--- |
| **Server** (`server.c`) | TCP & UDP | [cite_start]Listens for **TCP connections** and **UDP messages**[cite: 4]. [cite_start]Acts as the **broker**[cite: 3]. [cite_start]Manages client subscriptions and unsubscriptions[cite: 4]. [cite_start]Distributes messages from UDP clients to relevant TCP subscribers[cite: 5]. |
| **Subscriber** (`subscriber.c`) | TCP | [cite_start]Connects to the server with a **unique ID**[cite: 6]. [cite_start]Sends `subscribe`, `unsubscribe`, and `exit` commands[cite: 6]. [cite_start]Receives and displays messages from the server on the subscribed topics[cite: 7]. |
| **Publisher** | UDP | [cite_start]Sends messages to the server on specific topics[cite: 8]. (Publishers are not implemented in this project; provided external clients are used) [cite_start][cite: 8]. |

### Networking and Efficiency

* [cite_start]**Dual Protocol Support**: The server handles **bidirectional TCP communication** with subscribers [cite: 9] [cite_start]and **receives messages via UDP** from publishers[cite: 10].
* [cite_start]**Low Latency**: The **Nagle algorithm is disabled** (`TCP_NODELAY`) on the TCP socket to ensure low latency communication[cite: 11].
* **I/O Multiplexing**: The server utilizes the **`poll()`** mechanism to efficiently handle concurrent connections, monitoring the TCP listener, UDP listener, and `STDIN_FILENO`.

---

## 2. ⚙️ Key Technical Implementations

### 2.1. Subscription and Topic Management

* **Wildcard Support**: The application supports subscriptions using **wildcard characters** (`*` and `+`) through the `cmp_topics` function.
* **Dynamic Topics**: The server creates and manages topics dynamically (`struct topic_entry`) upon first subscription.
* **`execute_command`**: Handles all subscription and unsubscription logic, managing dynamic memory (`realloc`) for the list of subscribed clients (`subscribed_clients`).

### 2.2. Reliable Data Transfer and Serialization

* **Length Prefixation (TCP)**: All TCP messages are sent and received using **length prefixation** (`send_buf`, `recv_all`). The message length is prepended to the payload, ensuring the receiver obtains the entire message.
* **Data Type Parsing**: Subscribers use functions like `parse_int`, `parse_short_real`, and `parse_float` to correctly interpret the different data types received in the UDP packets, handling **network byte order** (`ntohl`, `ntohs`) and sign/decimal components.
* **Packet Integrity (`send_info`/`recv_info`)**: These functions ensure the full UDP packet structure (IP, port, topic, type, content) is transferred reliably over TCP, using an array of lengths for prefixing multiple data fields.

---

## 3. 🔨 Build and Run

### Prerequisites

* GCC (GNU Compiler Collection)

### Running the Server

* The server requires a port number as a command-line argument: ./server <PORT>

### Running the Subscriber

*The subscriber requires a unique ID, the server's IP, and the server's port:./subscriber <CLIENT_ID> <SERVER_IP> <SERVER_PORT>

### Compilation

Use the provided `Makefile` to compile the server and the subscriber executables:

```bash
make all
