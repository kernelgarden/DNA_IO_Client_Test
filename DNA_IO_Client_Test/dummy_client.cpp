#include <iostream>
#include <chrono>
#include <deque>

#include <boost/asio.hpp>
#include <boost/bind.hpp>
#include <boost/thread.hpp>
#include <boost/asio/steady_timer.hpp>

#include "protocol.pb.h"
#include "PB_handler.h"
#include "Settings.h"
#include "User_info.h"

class DummyClient
{
public:
	DummyClient(boost::asio::io_service& io_service)
		: m_Io_service(io_service), m_Socket(io_service),
		m_bIslogin(false), m_nPacketBufferMark(0), handler(this), 
		sync_timer(io_service)
	{
		InitializeCriticalSectionAndSpinCount(&m_Lock, 4000);
	}

	~DummyClient()
	{
		EnterCriticalSection(&m_Lock);

		while (!send_queue.empty())
		{
			delete[] send_queue.front();
			send_queue.pop_front();
		}

		LeaveCriticalSection(&m_Lock);

		DeleteCriticalSection(&m_Lock);
	}

	bool Is_connect() const { return m_Socket.is_open(); }

	void Set_login() { m_bIslogin = true; }
	bool Is_login() const { return m_bIslogin; }

	void Set_Id(std::string id) { m_id = id; }
	std::string Get_Id() const { return m_id; }

	void Set_Passwd(std::string passwd) { m_passwd = passwd; }
	std::string Get_Passwd() const { return m_passwd; }

	void Set_RequestLogin() { m_bIsRequestLogin = true; }
	bool Get_RequestLogin() const { return m_bIsRequestLogin; }

	void Set_Channel(int channel_num) { m_channel_num = channel_num; }
	int Get_Channel() const { return m_channel_num; }

	void Connect(boost::asio::ip::tcp::endpoint endpoint)
	{
		m_nPacketBufferMark = 0;
		m_bIsRequestLogin = false;

		m_Socket.async_connect(endpoint,
			boost::bind(&DummyClient::handle_connect, this,
				boost::asio::placeholders::error)
		);

		boost::asio::ip::tcp::no_delay option(true);
		m_Socket.set_option(option);
	}

	void Close()
	{
		if (m_Socket.is_open())
			m_Socket.close();
	}

	void Send(const bool b_Immediately, unsigned char *packet, size_t size)
	{ 
		unsigned char *SendData = nullptr;

		EnterCriticalSection(&m_Lock);

		if (b_Immediately == false)
		{
			SendData = new unsigned char[size];
			memcpy(SendData, packet, size);
			delete[] packet;

			send_queue.push_back(SendData);
			send_queue_size.push_back(size);
		}
		else
		{
			SendData = packet;
			size = send_queue_size.front();
		}

		if (b_Immediately || send_queue.size() < 2)
		{
			boost::asio::async_write(m_Socket, boost::asio::buffer(SendData, size),
				boost::bind(&DummyClient::handle_write, this,
					boost::asio::placeholders::error, 
					boost::asio::placeholders::bytes_transferred)
			);
		}

		LeaveCriticalSection(&m_Lock);
	}

	/* 유저 정보를 세팅합니다. */
	void Set_UserInfo(User_info *info)
	{
		user_info.user_name = info->user_name;
		user_info.xpos = info->xpos;
		user_info.ypos = info->ypos;
		user_info.vec = info->vec;
		user_info.type = info->type;
		user_info.A_type_pow = info->A_type_pow;
		user_info.B_type_pow = info->B_type_pow;
		user_info.C_type_pow = info->C_type_pow;
	}

	/* 서버로의 동기화를 시작합니다. */
	void Start_Sync()
	{
		sync_timer.expires_from_now(std::chrono::milliseconds(300));

		sync_timer.async_wait(boost::bind(&DummyClient::Sync, this,
			boost::asio::placeholders::error));
	}

private:

	void Receive()
	{
		m_Socket.async_read_some(
			boost::asio::buffer(ReceiveBuf),
			boost::bind(&DummyClient::handle_receive, this,
				boost::asio::placeholders::error,
				boost::asio::placeholders::bytes_transferred)
		);
	}

	void handle_connect(const boost::system::error_code& error)
	{
		if (!error)
		{
			std::cout << "[Dummy Client] 서버와 연결에 성공했습니다." << std::endl;
			std::cout << "이름과 패스워드를 입력해주세요." << std::endl;
	
			Receive();
		}
		else
		{
			std::cout << "[Dummy Client] 서버와 연결에 실패했습니다." << std::endl;
			std::cout << "[Dummy Client] Error No: " << error.value()
				<< ", Message: " << error.message() << std::endl;
		}
	}
	
	void handle_write(const boost::system::error_code& error,
		size_t bytes_transferred)
	{
		EnterCriticalSection(&m_Lock);

		delete[] send_queue.front();
		send_queue.pop_front();

		int size = send_queue_size.front();
		send_queue_size.pop_front();

		unsigned char *SendData = nullptr;

		if (!send_queue.empty())
		{
			SendData = send_queue.front();
			size = send_queue_size.front();
		}

		LeaveCriticalSection(&m_Lock);

		if (SendData != nullptr)
		{
			Send(true, SendData, size);
		}
	}

	void handle_receive(const boost::system::error_code& error,
		size_t bytes_transferred)
	{
		if (error)
		{
			if (error == boost::asio::error::eof)
			{
				std::cout << "[Dummy Client] 서버와 연결이 끊어졌습니다." << std::endl;
			}
			else
			{
				std::cout << "[Dummy CLient] Error No: " << error.value() << ", Message: "
					<< error.message() << std::endl;
			}

			Close();
		}
		else
		{
			memcpy(&m_PacketBuf[m_nPacketBufferMark], ReceiveBuf.data(), bytes_transferred);

			int nPacketData = m_nPacketBufferMark + bytes_transferred;
			int nReadn = 0;

			protobuf::io::ArrayInputStream input_array_stream(&m_PacketBuf, nPacketData);
			protobuf::io::CodedInputStream input_coded_stream(&input_array_stream);

			/*
			m_nPacketBufferMark = Process_packet(input_coded_stream, handler);
			nReadn = nPacketData - m_nPacketBufferMark;
			*/

			nReadn = Process_packet(input_coded_stream, handler); // 세션의 패킷 버퍼에 쌓인 패킷들을 처리하고 읽어들인 패킷 데이터의 양을 저장합니다.
			m_nPacketBufferMark = nPacketData - nReadn; // 남은 데이터의 양

			if (m_nPacketBufferMark > 0)
			{
				protobuf::uint8 TempBuffer[MAX_RECEIVE_BUF_LEN] = { 0, };
				memcpy(&TempBuffer[0], &m_PacketBuf[nReadn], m_nPacketBufferMark);
				memcpy(&m_PacketBuf[0], &TempBuffer[0], m_nPacketBufferMark);
			}

			Receive();
		}
	}

	/* 서버와의 동기화 코드 */
	void Sync(const boost::system::error_code& error)
	{
		if (error)
		{
			std::cout << "[Dummy Client] 서버와의 동기화에 실패했습니다." << std::endl << 
				"Error No: " << error.value() << "Error MSG: " << 
				error.message() << std::endl;
		}
		else
		{
			int buf_size = 0;
			dna_info::SyncInfo_C sync_data;

			sync_data.set_user_id(user_info.user_name);
			sync_data.set_x_pos(user_info.xpos);
			sync_data.set_y_pos(user_info.ypos);
			sync_data.set_vec(user_info.vec);
			sync_data.set_type(user_info.type);
			sync_data.set_a_type_pow(user_info.A_type_pow);
			sync_data.set_b_type_pow(user_info.B_type_pow);
			sync_data.set_c_type_pow(user_info.C_type_pow);

			buf_size += sizeof(PacketHeader) + sync_data.ByteSize();
			protobuf::uint8 *outputBuf = new protobuf::uint8[buf_size];

			protobuf::io::ArrayOutputStream output_array_stream(outputBuf, buf_size);
			protobuf::io::CodedOutputStream output_coded_stream(&output_array_stream);

			WriteMessageToStream(sync_data, dna_info::SYNC_INFO_C, output_coded_stream);

			Send(false, outputBuf, buf_size);

			Start_Sync();

			std::cout << "[Dummy Client] 서버와 동기를 수행했습니다. " << std::endl;
		}
	}

	std::string m_id;
	std::string m_passwd;

	boost::asio::io_service& m_Io_service;
	boost::asio::ip::tcp::socket m_Socket;

	std::array<google::protobuf::uint8, MAX_RECEIVE_BUF_LEN> ReceiveBuf;

	int m_nPacketBufferMark;
	google::protobuf::uint8 m_PacketBuf[MAX_RECEIVE_BUF_LEN * 10];

	CRITICAL_SECTION m_Lock;
	std::deque<unsigned char *> send_queue;
	std::deque<int> send_queue_size;

	PacketHandler handler;

	int m_channel_num;
	bool m_bIslogin;
	bool m_bIsRequestLogin;

	User_info user_info;

	boost::asio::steady_timer sync_timer;
};

class DummyClientTest
{
public:
	void Test()
	{
		boost::asio::io_service io_service;
		auto endpoint = boost::asio::ip::tcp::endpoint(
			boost::asio::ip::address::from_string("127.0.0.1"), PORT_NUM);

		/*
		dna_info::LoginRequest req;
		req.set_id("kernelgarden");
		req.set_passwd("sunrint123");

		PacketHeader header;
		header.size = req.ByteSize();
		header.type = dna_info::LOGIN_REQ;

		int nWrite = 0;
		unsigned char buf[MAX_RECEIVE_BUF_LEN];

		memcpy(buf, &header, sizeof(header));
		nWrite += sizeof(header);
		memcpy(&buf[nWrite], &req, header.size);
		*/

		boost::asio::strand st(io_service);

		DummyClient client(io_service);
		client.Connect(endpoint);
		

		boost::thread thread(boost::bind(&boost::asio::io_service::run, &io_service));
		
		char szMessage[MAX_RECEIVE_BUF_LEN] = { 0, };

		//while (std::cin.getline(szMessage, MAX_RECEIVE_BUF_LEN))
		while (true)
		{
			//if (strnlen_s(szMessage, MAX_RECEIVE_BUF_LEN) == 0)
			//	break;

			if (client.Is_connect() == false)
			{
				std::cout << "[Dummy Client] 서버와 연결되지 않았습니다." << std::endl;
				continue;
			}

			if (client.Is_login() == false)
			{
				int buf_size = 0;
				char id[300];
				char passwd[300];
				dna_info::LoginRequest req;

				Sleep(1000);

				std::cout << "ID: ";
				std::cin >> id;
				std::cout << "PASSWD: ";
				std::cin >> passwd;

				req.set_id(id);
				req.set_passwd(passwd);

				buf_size += sizeof(PacketHeader) + req.ByteSize();
				protobuf::uint8 *outputBuf = new protobuf::uint8[buf_size];

				protobuf::io::ArrayOutputStream output_array_stream(outputBuf, buf_size);
				protobuf::io::CodedOutputStream output_coded_stream(&output_array_stream);

				WriteMessageToStream(req, dna_info::LOGIN_REQ, output_coded_stream);

				client.Send(false, outputBuf, buf_size);
				while (true)
				{
					Sleep(1000);
					if (client.Is_login())
						break;
				}
			}
			else
			{
				std::cout << "[Dummy Client] 로그인 되었습니다." << std::endl;
				Sleep(1000);
			}
		}

		io_service.stop();
		client.Close();
		thread.join();

		std::cout << "Dummy Client Test 종료" << std::endl;
	}
};