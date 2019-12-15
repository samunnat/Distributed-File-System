# Samunnat Lamichhane
## CSCI 4273

In this PA, I implemented a Distributed File System on both the server and client side.
Here are the following commands a client can use:

- put [file_Name]
The file is split into 4 pieces and 2-piece-pairs are assigned to the DFS servers based on the file contents' hash.

- list
All DFS servers are queried to get a list of all the files. Files with incomplete pieces are marked with "[incomplete]". For this, I parse the response from the servers into a linked list, each node being a fileInfo. I use this later in the get method too.

- get [file_Name]
Assembled the 4 pieces into one file inside the downloads folder. For this, I use the "list" method's fileInfo linked-list to find out what servers to query for what file-piece.

### Instructions:
Terminal 1-4:
./dfs DFS[N] 1000[N]

Terminal 5:
./dfc dfc.conf