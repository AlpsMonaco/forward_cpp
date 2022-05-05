# forward_cpp
A TCP forward tools.

##
This is a tcp port forward tool.  
If you have boost lib,it is highly recommand using  
forward-boost.cpp,which implemented with boost::asio  
and faster than select.  
forward.cpp implemented with select,works in most of  
circumstance.  

## compile
This code could be compiled both in Unix/Windows os.  

### In unix:  
`g++ -o forward forward.cpp`

### In Windows:  
`cl /EHsc /Ox forward.cpp`

## usage
forward localport remoteaddr remoteport  
`./forward 65444 192.168.1.2 22`  
In this case,the program will forward all data to 192.168.1.2:22  


