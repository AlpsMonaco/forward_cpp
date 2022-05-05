#include <boost/asio.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <string>

using namespace boost::system;
using namespace boost::asio;
using namespace boost::asio::ip;
using namespace std;

void PrintHelp()
{
    std::cout << R"(usage:
./forward <src_port> <dst_ip> <dst_port>

example:
./forward 66022 192.168.1.12 22

this will proxy all connection from port 66022
of host machine to 192.168.1.12:22.
)";
}

#define HandleError(ec)                                       \
    do                                                        \
    {                                                         \
        std::cerr << __FILE__ << ":" << __LINE__ << std::endl \
                  << ec << std::endl                          \
                  << ec.value() << std::endl                  \
                  << ec.message() << std::endl;               \
    } while (0)

#define DebugInfo(info)                                       \
    do                                                        \
    {                                                         \
        std::cout << __FILE__ << ":" << __LINE__ << std::endl \
                  << info << std::endl;                       \
    } while (0)

inline bool CheckPort(int port)
{
    return (port >= 1 && port <= 65535);
}

template <size_t bufferSize>
class Buffer
{
public:
    Buffer() { memset(buffer, 0, bufferSize); }
    inline ~Buffer() { DebugInfo("buffer destructor"); }

    char *Get() { return buffer; }
    const size_t GetSize() { return bufferSize; }

protected:
    char buffer[bufferSize];
};

void Forward(boost::shared_ptr<tcp::socket> pSrc,
             boost::shared_ptr<tcp::socket> pDst,
             boost::shared_ptr<Buffer<1024>> pBuffer)
{
    pSrc->async_read_some(buffer(pBuffer->Get(), pBuffer->GetSize()),
                          [pSrc,
                           pDst,
                           pBuffer](const boost::system::error_code &ec,
                                    size_t length) -> void
                          {
                              if (ec)
                              {
                                  HandleError(ec);
                                  return;
                              }
                              pDst->async_write_some(
                                  buffer(pBuffer->Get(), length),
                                  [pSrc,
                                   pDst,
                                   pBuffer](const boost::system::error_code &ec,
                                            size_t length) -> void
                                  {
                                      if (ec)
                                      {
                                          HandleError(ec);
                                          return;
                                      }
                                      Forward(pSrc, pDst, pBuffer);
                                  });
                          });
}

void BeginForward(io_service &ios,
                  boost::shared_ptr<tcp::socket> client,
                  int dstPort,
                  const std::string &dstAddr)
{
    boost::shared_ptr<tcp::socket> target = boost::make_shared<tcp::socket>(ios);
    target->async_connect(tcp::endpoint(address::from_string(dstAddr), dstPort),
                          [client, target](const boost::system::error_code &ec) -> void
                          {
                              if (ec)
                              {
                                  HandleError(ec);
                                  return;
                              }
                              Forward(client, target, boost::make_shared<Buffer<1024>>());
                              Forward(target, client, boost::make_shared<Buffer<1024>>());
                          });
}

void BeginAccept(io_service &ios,
                 tcp::acceptor &acceptor,
                 int dstPort,
                 const std::string &dstAddr)
{
    boost::shared_ptr<tcp::socket> pSocket = boost::make_shared<tcp::socket>(ios);
    acceptor.async_accept(*pSocket,
                          [pSocket,
                           &acceptor,
                           &ios,
                           dstPort,
                           &dstAddr](const boost::system::error_code &ec) -> void
                          {
                              if (ec)
                              {
                                  HandleError(ec);
                                  return;
                              }
                              BeginForward(ios, pSocket, dstPort, dstAddr);
                              BeginAccept(ios, acceptor, dstPort, dstAddr);
                          });
}

void Begin(int port, int dstPort, const std::string &dstAddr)
{
    io_service ios;
    tcp::acceptor acceptor(ios, tcp::endpoint(tcp::v4(), port));
    BeginAccept(ios, acceptor, dstPort, dstAddr);
    ios.run();
}

int main(int argc, char **argv)
{
    if (argc < 4)
    {
        std::cerr << "wrong usage" << std::endl;
        PrintHelp();
        return 1;
    }

    int port = atoi(argv[1]);
    if (!CheckPort(port))
    {
        std::cerr << "invalid port " << argv[1];
        return 1;
    }
    int dstPort = atoi(argv[3]);
    if (!CheckPort(dstPort))
    {
        std::cerr << "invalid port " << argv[3];
        return 1;
    }

    std::string dstAddr(argv[2]);
    Begin(port, dstPort, dstAddr);
}