# Distrubuted File System

A Distributed File System is a client/server-based application that allows client to store and retrieve files on multiple servers. One of the features of Distributed file system is that each file can be divided in to pieces and stored on different servers and can be retrieved even if one server is not active.

## How to Use:
1. To create the executables, enter

    ```make```

2. To run the four servers, open 4 terminals, and enter:
    
    ```./dfs DFS<1-4> <10001-10004>```
    
    Example: ./dfs DFS2 10002


2. To run the client, enter

    ```./dfc dfc.conf```

    in a separate terminal.


Here are the following commands a client can use:

- ```put [file_Name]```
The file is split into 4 pieces and 2-piece-pairs are assigned to the DFS servers (the order is based on the file contents' hash).

- ```list```
All DFS servers are queried to get a list of all the files. Files with incomplete pieces are marked with "[incomplete]". For this, I parse the response from the servers into a linked list, each node being a fileInfo. I use this later in the get method too.

- ```get [file_Name]```
Assembled the 4 pieces into one file inside the downloads folder. For this, I use the "list" method's fileInfo linked-list to find out what servers to query for what file-piece.

### Author: Samunnat Lamichhane