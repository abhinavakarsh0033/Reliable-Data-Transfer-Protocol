This assignment implements a custom message-oriented transport layer protocol (KTP - KGP Transport Protocol) built on top of UDP, providing reliable, in-order message delivery using window-based flow control.

A set of KTP socket functions for flow control, retransmissions, and buffer management is implemented. The protocol supports sending and receiving messages, handling timeouts, and managing a sliding window for flow control.

Two sample applications are provided to demonstrate the use of the KTP protocol: One for sending a file and another for receiving it.

The information about data structures and functions used in the KTP protocol is provided in the `documentation.txt` file. This file also conatins the performance analysis of the protocol for different drop probabilities.

## Compilation Instructions
To compile the KTP protocol and the sample applications:
```bash
make all
```
To clean up the compiled files:
```bash
make clean
```

## To Run the Applications
First start the KTP Socket Daemon (init process):
```bash
./init
```
To run the sender application:
```bash
./user1 <source port> <destination port>
```
To run the receiver application:
```bash
./user2 <source port> <destination port>
```
You can run the above applications in multiple terminals to simulate multiple senders and receivers.