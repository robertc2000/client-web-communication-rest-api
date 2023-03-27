In this project, I implemented a client in C capable of interacting with a REST API. The goals aimed to achieve were:

• Understanding the mechanisms of communication through HTTP

• Interacting with a REST API

• Understanding commonly used web concepts such as JSON, session, JWT

• Using external libraries for manipulating JSON objects in REST API. For that, i used the parson library, represented by the 2 parson files (.c and .h)

Although the de facto technology for developing web clients is the trio of HTML, CSS, and JavaScript, I chose to use C/C++ in order to get as close as possible to the protocol and to solidify my understanding of the concepts.

• The server exposes a REST (Representational State Transfer) API (Application Programmable Interface). You can think of it as a black box that exposes a series of inputs, represented by HTTP routes. Upon receiving HTTP requests, the server performs an action. In the context of the project, the server simulates an online library and is already fully implemented.

• The client is a program written in C that accepts keyboard commands and sends requests to the server based on the command. Its purpose is to act as a user interface (UI) for the virtual library.

The client interprets keyboard commands in order to interact with the server. Upon receiving a command, the client forms the JSON object (if applicable), executes the request to the server, and displays its response (whether successful or in error). The process is repeated until the command "exit" is entered.
