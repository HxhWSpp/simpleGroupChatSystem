# simpleGroupChatSystem

simpleGroupChatSystem is a group messaging system

## Installation

run build.sh
```bash
    ./build.sh
```
run the server and client
```bash
    ./server
    ./client
```

## Usage

Client
```
    !CREATE group_name     // create a new group
    !REMOVE group_id       // remove a group
    !JOIN group_id         // join a group
    !LEAVE group_id        // leave a group
    !CHAT group_id         // enter the group's chat
    !CHATL                 // leave the current chat
    !GROUPS                // list all groups [group_name|id]
    !JOINED                // list groups you've joined
    !OWNED                 // list groups you own
    !INFO group_id         // show detailed info about a group
    !EXIT                  // exit
```

