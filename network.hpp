#ifndef __NETWORK_H__
#define __NETWORK_H__

#ifdef _WIN32

#include <winsock2.h>
#pragma comment(lib, "WS2_32.Lib")

#else

#include <netinet/in.h>
#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>

#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
using SOCKET = int;

#endif

#include <map>
#include <iostream>

namespace network
{
	using socket_fd = SOCKET;
	class Socket
	{
	public:
		Socket();
		Socket(int aftype, int socktype, const char *addr = "0.0.0.0", int port = 0, int fd = 0);
		Socket(int socktype, const sockaddr_in *paddr, int fd = 0);
		Socket(const Socket &rhs);
		Socket(Socket &&rhs);
		virtual ~Socket() {}

		Socket &operator=(const Socket &rhs);
		Socket &operator=(Socket &&rhs);

		socket_fd GetFd();
		int GetPort();
		void GetAddr(char *dst);
		const sockaddr_in *GetSockAddr();
		bool Close();
		int Errno();
		int Send(const char *buf, int size);
		int Recv(char *buf, int size);

		socket_fd GetFd() const;
		int GetPort() const;
		void GetAddr(char *dst) const;
		const sockaddr_in *GetSockAddr() const;
		int Send(const char *buf, int size) const;
		// int Recv(char *buf, int size) const;

	protected:
		sockaddr_in addr;
		int socktype;
		socket_fd fd;

		bool CreateSocket();
	};

	namespace tcp
	{
		class Client : public Socket
		{
		public:
			Client();
			Client(const char *addr, int port);
			// ~Client();

			bool Connect();
			bool Connect(const char *addr, int port);
		};

		class Server : public Socket
		{
		public:
			Server(const char *addr, int port, int buffersize = 1024);
			Server(Server &&server);
			~Server();

			void SetOnNewConnection(bool (*onNewConnection)(const Socket &socket));
			void SetOnNewData(bool (*onNewData)(const Socket &socket, char *data, int recvsize));
			void SetOnConnectionClose(void (*onConnectionClose)(const Socket &socket));
			void SetOnError(void (*onError)(const char *message));
			bool Listen();
			void Begin();

		protected:
			using SocketMap = std::map<socket_fd, Socket>;

			// return false if you want this connection to be closed after this callback;
			bool (*onNewConnection)(const Socket &socket);
			// return false if you want this connection to be closed after this callback;
			bool (*onNewData)(const Socket &socket, char *data, int recvsize);
			void (*onConnectionClose)(const Socket &socket);
			void (*onError)(const char *);

			static bool DefaultOnNewConnection(const Socket &socket);
			static bool DefaultOnNewData(const Socket &socket, char *data, int recvsize);
			static void DefaultOnConnectionClose(const Socket &socket);
			static void DefaultOnError(const char *msg);

			Server &operator=(const Server &rhs) = delete;
			Server &operator=(Server &&rhs);

			void ParseCallback();

			Socket &GetSocket(socket_fd cfd);
			void AddSocket(socket_fd cfd, const sockaddr_in &sockaddr);
			void RemoveSocket(socket_fd cfd);

			SocketMap socketmap;
			fd_set socketset;
			char *buffer;
			int buffersize;
		};
	}
}

namespace network
{
	constexpr int SOCKADDR_IN_SIZE = sizeof(sockaddr_in);

#ifdef _WIN32

	struct WSAManager
	{
		WSAManager()
		{
			WSADATA wsadata;
			WSAStartup(MAKEWORD(2, 2), &wsadata);
		}
		~WSAManager() { WSACleanup(); }
	};

	WSAManager &WSAInit()
	{
		static WSAManager wsamanager;
		return wsamanager;
	}

	void (*Init)() = []() -> void
	{ WSAInit(); };
	inline int GetErrno() { return WSAGetLastError(); }
	using socklen_t = int;

#else
	inline int ioctlsocket(socket_fd fd, unsigned long int arg, u_long *val)
	{
		return ioctl(fd, arg, val);
	}

	void (*Init)() = []() -> void {};
	inline void itoa(int value, char *buf, int unuse) { sprintf(buf, "%d", value); }
	int (*closesocket)(int) = close;
	inline int GetErrno() { return errno; }

#endif

	Socket &Socket::operator=(const Socket &rhs)
	{
		this->addr = rhs.addr;
		this->socktype = rhs.socktype;
		this->fd = rhs.fd;
		return *this;
	}

	Socket &Socket::operator=(Socket &&rhs)
	{
		*this = static_cast<const Socket &>(rhs);
		rhs.addr.sin_port = 0;
		rhs.addr.sin_family = 0;
		rhs.addr.sin_zero[0] = 0;
		rhs.addr.sin_addr.s_addr = 0;
		rhs.fd = 0;
		return *this;
	}

	Socket::Socket(const Socket &rhs) : addr(rhs.addr), socktype(rhs.socktype), fd(rhs.fd) {}
	Socket::Socket(Socket &&rhs) : addr(rhs.addr), socktype(rhs.socktype), fd(rhs.fd)
	{
		rhs.socktype = 0;
		rhs.fd = 0;
		rhs.addr.sin_port = 0;
		rhs.addr.sin_family = 0;
		rhs.addr.sin_zero[0] = 0;
		rhs.addr.sin_addr.s_addr = 0;
		rhs.fd = 0;
	}

	Socket::Socket() : addr{0, 0, 0, {0}}, socktype(0), fd(0) { Init(); }
	Socket::Socket(int sockType, const sockaddr_in *paddr, int fd) : socktype(sockType), fd(fd), addr(*paddr) { Init(); }
	Socket::Socket(int afType, int socktype, const char *addr, int port, int fd) : socktype(socktype),
																				   fd(fd), addr{0, 0, 0, {0}}
	{
		this->addr.sin_family = afType;
		this->addr.sin_addr.s_addr = inet_addr(addr);
		this->addr.sin_port = htons(port);
		Init();
	}

	void Socket::GetAddr(char *dst) const
	{
		for (int i = 0; i < 4; i++)
		{
			itoa((this->addr.sin_addr.s_addr & 0xFF << (8 * i)) >> (8 * i), dst, 10);
			size_t size = strlen(dst);
			if (i != 3)
			{
				dst[size] = '.';
				dst += size + 1;
				continue;
			}
			dst[size] = '\0';
			break;
		}
	}

	int Socket::GetPort() const { return ntohs(this->addr.sin_port); }
	socket_fd Socket::GetFd() const { return this->fd; }
	socket_fd Socket::GetFd() { return const_cast<const Socket &>(*this).GetFd(); }

	bool Socket::CreateSocket()
	{
		this->fd = socket(this->addr.sin_family, this->socktype, 0);
		return this->fd != INVALID_SOCKET;
	}

	bool Socket::Close()
	{
		int status = closesocket(this->fd);
		this->fd = 0;
		return status != SOCKET_ERROR;
	}

	int Socket::Send(const char *buf, int bufsize) const { return send(this->fd, buf, bufsize, 0); }
	int Socket::Recv(char *buf, int bufsize) { return recv(fd, buf, bufsize, 0); }
	int Socket::Errno() { return GetErrno(); }
	const sockaddr_in *Socket::GetSockAddr() const { return const_cast<const sockaddr_in *>(&this->addr); }

	// tcp::Client::~Client() { this->Close(); }
	tcp::Client::Client(const char *addr, int port) : Socket(AF_INET, SOCK_STREAM, addr, port) { this->CreateSocket(); }
	tcp::Client::Client() : Socket(AF_INET, SOCK_STREAM) { this->CreateSocket(); }

	bool tcp::Client::Connect()
	{
		if (this->fd == INVALID_SOCKET)
		{
			if (!this->CreateSocket())
				return false;
		}
		return connect(this->fd, (sockaddr *)this->GetSockAddr(), SOCKADDR_IN_SIZE) != SOCKET_ERROR;
	}

	bool tcp::Client::Connect(const char *addr, int port)
	{
		this->addr.sin_port = htons(port);
		this->addr.sin_addr.s_addr = inet_addr(addr);
		return this->Connect();
	}

	tcp::Server::Server(const char *addr, int port, int buffersize) : Socket(AF_INET, SOCK_STREAM, addr, port),
																	  onNewConnection(nullptr),
																	  onConnectionClose(nullptr),
																	  onNewData(nullptr),
																	  onError(nullptr),
																	  socketmap(),
																	  socketset(),
																	  buffer(nullptr),
																	  buffersize(0)
	{
		FD_ZERO(&this->socketset);
		this->buffersize = buffersize;
		this->buffer = (char *)malloc(this->buffersize);
	}

	tcp::Server::Server(Server &&server) : Socket(std::forward<Server>(server)),
										   socketmap(std::move(server.socketmap)),
										   socketset(server.socketset),
										   buffer(server.buffer),
										   buffersize(server.buffersize)
	{
		memset(&server.socketset, 0, sizeof(server.socketset));
		server.buffer = nullptr;
		server.buffersize = 0;
	}

	tcp::Server &tcp::Server::operator=(Server &&rhs)
	{
		Socket::operator=(std::forward<Socket>(rhs));
		memset(&rhs.socketset, 0, sizeof(rhs.socketset));
		rhs.buffer = nullptr;
		rhs.buffersize = 0;
		return *this;
	}

	tcp::Server::~Server()
	{
		if (this->buffer != nullptr)
			free(this->buffer);
	}

	bool tcp::Server::Listen()
	{
		if (!this->CreateSocket())
			return false;
		u_long arg = 1;
		if (ioctlsocket(this->fd, FIONBIO, &arg))
			return false;
		if (bind(this->fd, (sockaddr *)&this->addr, SOCKADDR_IN_SIZE) == SOCKET_ERROR)
			return false;
		if (listen(this->fd, 10) == SOCKET_ERROR)
			return false;
		return true;
	}

#ifdef _WIN32
	void tcp::Server::Begin()
	{
		this->ParseCallback();
		FD_SET(this->fd, &this->socketset);
		fd_set readableset;
		int status, recvsize;
		unsigned int i;
		sockaddr_in clientaddr;
		socket_fd cfd;
		int addrlen = sizeof(clientaddr);
		for (;;)
		{
			readableset = this->socketset;
			status = select(NULL, &readableset, NULL, NULL, NULL);
			if (status == SOCKET_ERROR)
			{
				this->onError("socket error on I/O select");
				return;
			}
			for (i = 0; i < readableset.fd_count; i++)
			{
				if (readableset.fd_array[i] == this->fd)
				{
					cfd = accept(this->fd, (sockaddr *)&clientaddr, &addrlen);
					if (cfd == SOCKET_ERROR)
					{
						this->onError("accept socket failed");
						return;
					}
					this->AddSocket(cfd, clientaddr);
					Socket &rsocket = this->GetSocket(cfd);
					if (!this->onNewConnection(rsocket))
					{
						rsocket.Close();
						this->RemoveSocket(cfd);
					}
				}
				else
				{
					cfd = readableset.fd_array[i];
					Socket &rsocket = GetSocket(cfd);
					recvsize = rsocket.Recv(this->buffer, this->buffersize);
					if (recvsize > 0)
					{
						if (!this->onNewData(rsocket, this->buffer, recvsize))
						{
							rsocket.Close();
							this->RemoveSocket(cfd);
						}
					}
					else
					{
						this->onConnectionClose(rsocket);
						this->RemoveSocket(cfd);
					}
				}
			}
		}
	}
#else
	void tcp::Server::Begin()
	{
		this->ParseCallback();
		FD_SET(this->fd, &this->socketset);
		fd_set readableset;
		int count, recvsize;
		int maxfd = this->fd;
		unsigned int i;
		sockaddr_in clientaddr;
		socklen_t addrlen = sizeof(clientaddr);
		socket_fd cfd;
		int clientfdlist[1024];
		for (i = 0; i < 1024; i++)
			clientfdlist[i] = 0;
		for (;;)
		{
			readableset = this->socketset;
			count = select(maxfd + 1, &readableset, NULL, NULL, NULL);
			if (count == SOCKET_ERROR)
			{
				this->onError("socket error on I/O select");
				return;
			}
			if (FD_ISSET(this->fd, &readableset))
			{
				count--;
				cfd = accept(this->fd, (sockaddr *)&clientaddr, &addrlen);
				if (cfd == SOCKET_ERROR)
				{
					this->onError("accept socket failed");
					return;
				}
				if (cfd > maxfd)
					maxfd = cfd;
				this->AddSocket(cfd, clientaddr);
				Socket &rsocket = this->GetSocket(cfd);
				if (!this->onNewConnection(rsocket))
				{
					rsocket.Close();
					this->RemoveSocket(cfd);
				}
				else
				{
					for (i = 0; i < 1024; i++)
					{
						if (clientfdlist[i] == 0)
						{
							clientfdlist[i] = cfd;
							break;
						}
					}
				}
			}
			for (i = 0; i < 1024; i++)
			{
				cfd = clientfdlist[i];
				if (FD_ISSET(cfd, &readableset))
				{
					count--;
					Socket &rsocket = GetSocket(cfd);
					recvsize = rsocket.Recv(this->buffer, this->buffersize);
					if (recvsize > 0)
					{
						if (!this->onNewData(rsocket, this->buffer, recvsize))
						{
							rsocket.Close();
							this->RemoveSocket(cfd);
							clientfdlist[i] = 0;
						}
					}
					else
					{
						this->onConnectionClose(rsocket);
						this->RemoveSocket(cfd);
						clientfdlist[i] = 0;
					}
				}
				if (count == 0)
					break;
			}
		}
	}

#endif

	void tcp::Server::AddSocket(socket_fd cfd, const sockaddr_in &sockaddr)
	{
		FD_SET(cfd, &this->socketset);
		this->socketmap.emplace(cfd, Socket(SOCK_STREAM, &sockaddr, cfd));
	}

	void tcp::Server::RemoveSocket(socket_fd cfd)
	{
		FD_CLR(cfd, &this->socketset);
		this->socketmap.erase(cfd);
	}

	Socket &tcp::Server::GetSocket(socket_fd cfd) { return socketmap.at(cfd); }

	// return false if you want to close this connection.
	void tcp::Server::SetOnNewConnection(bool (*onNewConnection)(const Socket &socket)) { this->onNewConnection = onNewConnection; }
	// return false if you want to close this connection.
	void tcp::Server::SetOnNewData(bool (*onNewData)(const Socket &socket, char *data, int recvsize)) { this->onNewData = onNewData; }
	void tcp::Server::SetOnConnectionClose(void (*onConnectionClose)(const Socket &socket)) { this->onConnectionClose = onConnectionClose; }
	void tcp::Server::SetOnError(void (*onError)(const char *message)) { this->onError = onError; }

	bool tcp::Server::DefaultOnNewConnection(const Socket &socket)
	{
		static char buf[64];
		socket.GetAddr(buf);
		std::cout << "new connection: " << buf << ':' << socket.GetPort() << std::endl;
		return true;
	}

	bool tcp::Server::DefaultOnNewData(const Socket &socket, char *data, int recvsize)
	{
		static char buf[64];
		socket.GetAddr(buf);
		data[recvsize] = 0;
		std::cout << "new data received from " << buf << ':' << socket.GetPort() << std::endl
				  << data << std::endl;
		return true;
	}

	void tcp::Server::DefaultOnConnectionClose(const Socket &socket)
	{
		static char buf[64];
		socket.GetAddr(buf);
		std::cout << "client closed its connection: " << buf << ':' << socket.GetPort() << std::endl;
	}

	void tcp::Server::ParseCallback()
	{
		if (this->onNewConnection == nullptr)
			this->onNewConnection = DefaultOnNewConnection;
		if (this->onNewData == nullptr)
			this->onNewData = DefaultOnNewData;
		if (this->onConnectionClose == nullptr)
			this->onConnectionClose = DefaultOnConnectionClose;
		if (this->onError == nullptr)
			this->onError = DefaultOnError;
	}

	void tcp::Server::DefaultOnError(const char *msg) { std::cout << msg << std::endl; }

	int network::Socket::GetPort() { return const_cast<const network::Socket &>(*this).GetPort(); }
	void network::Socket::GetAddr(char *dst) { return const_cast<const network::Socket &>(*this).GetAddr(dst); }
	const sockaddr_in *network::Socket::GetSockAddr() { return const_cast<const network::Socket &>(*this).GetSockAddr(); }
	int network::Socket::Send(const char *buf, int size) { return const_cast<const network::Socket &>(*this).Send(buf, size); }
	// int network::Socket::Recv(char *buf, int size) { return const_cast<const network::Socket &>(*this).Recv(buf, size); }
}

#endif