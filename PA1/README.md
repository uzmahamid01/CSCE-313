[![Review Assignment Due Date](https://classroom.github.com/assets/deadline-readme-button-24ddc0f5d75046c5622901739e7c5dd533143b0c8e959d652212380cedb1ea36.svg)](https://classroom.github.com/a/XHbjTopT)
[![Open in Visual Studio Code](https://classroom.github.com/assets/open-in-vscode-718a45dd9cf7e7f842a935f5ebbe5719a5e09af4491e668f4dbf3b35d5cca122.svg)](https://classroom.github.com/online_ide?assignment_repo_id=11812362&assignment_repo_type=AssignmentRepo)
# <p align="center">PA1: Client-Server IPC using Named Pipes<p>

**Introduction**

In this assignment, you will write a client program that connects to a given server. 
The server hosts several electrocardiogram (ECG) data points of 15 patients suffering from various cardiac diseases. The client has to communicate with the server such that it can fulfill two main objectives:

1. Obtain individual data points from the server.
2. Obtain a whole raw file of any size in one or more segments from the server.

The client has to send properly-formatted messages to the server using a communication protocol that the server defines to implement this transfer functionality. 

**Tasks**

- [ ] Run server as a child of the client
- [ ] Request data points
  - [ ] a single data point
  - [ ] 1000 data points
- [ ] Request files (with and without differing buffer capacity)
  - [ ] CSV file
  - [ ] binary file (of differing sizes)
- [ ] Request a new channel
- [ ] Close channels

See the PA1 module on Canvas for further details and assistance.
