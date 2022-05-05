# forward_cpp
A TCP forward tools.

##
The code uses select to poll both size in one fdset.  
Work in both Unix/Windows system.  
If you have boost lib,consider using boost/main.cpp instead,
it is more C++ style and mostly faster than select.

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


