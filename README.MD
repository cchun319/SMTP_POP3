# Email Server
- [Brief](#project-brief)  
- [Use](#use)

## Project Brief
The project is a multithreaded C++ email server compatible with Thundermail in linux<br/>
The system supports<br/>
:key:user sign-up/log-in<br/>
:email:mail write/send<br/>

## Architect
• USER, which tells the server which user is logging in<br/>
• PASS, which specifies the user's password<br/>
• STAT, which returns the number of messages and the size of the mailbox<br/>
• UIDL, which shows a list of messages, along with a unique ID for each message<br/>
• RETR, which retrieves a particular message<br/>
• DELE, which deletes a message<br/>
• QUIT, which terminates the connection<br/>
• LIST, which shows the size of a particular message, or all the messages<br/>
• RSET, which undeletes all the messages that have been deleted with DELE; and<br/>
• NOOP, which does nothing<br/>

## Use
1. Go to the directories: ./smtp or ./pop3 and do: 
```bash 
make
```
2. 
```bash
./pop3 localhost
```

## Technologies
C++
