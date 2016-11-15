#include "PB_handler.h"
#include "dummy_client.cpp"
#include "User_info.h"

int Process_packet(protobuf::io::CodedInputStream& input_stream, PacketHandler& handler)
{
	int remain_size, nRead = 0;
	PacketHeader packet_header;

	// 스트림으로부터 헤더를 읽어옵니다.
	while (input_stream.ReadRaw(&packet_header, sizeof(PacketHeader)))
	{
		// 스트림에 직접 엑세스 할 수 있는 버퍼 포인터와 남은 길이를 알아옵니다.
		const void *payload_ptr = NULL;
		remain_size = 0;
		input_stream.GetDirectBufferPointer(&payload_ptr, &remain_size);
		if (remain_size < (signed)packet_header.size)
			break;

		nRead += sizeof(PacketHeader) + packet_header.size;

		// 패킷을 읽어내기 위한 스트림을 생성합니다.
		protobuf::io::ArrayInputStream payload_array_stream(payload_ptr, packet_header.size);
		protobuf::io::CodedInputStream payload_input_stream(&payload_array_stream);

		// 패킷 본체만큼 포인터를 이동합니다.
		input_stream.Skip(packet_header.size);

		// 패킷을 분석해서 메시지 종류에 따라 처리를 합니다.
		switch (packet_header.type)
		{
		/*
		case dna_info::LOGIN_REQ:
		{
			dna_info::LoginRequest message;
			if (message.ParseFromCodedStream(&payload_input_stream) == false)
			{
				std::cerr << "[Error] parse error" << std::endl;
				break;
			}
			handler.Handle(message);
		}
		break;
		*/
		case dna_info::LOGIN_RES:
		{
			dna_info::LoginResponse message;
			if (message.ParseFromCodedStream(&payload_input_stream) == false)
			{
				std::cerr << "[Error] parse error" << std::endl;
				break;
			}
			handler.Handle(message);
		}
		break;
		case dna_info::USER_INFO:
		{
			dna_info::UserInfo message;
			if (message.ParseFromCodedStream(&payload_input_stream) == false)
			{
				std::cerr << "[Error] parse error" << std::endl;
				break;
			}
			handler.Handle(message);
		}
		break;
		case dna_info::SYNC_INFO_C:
		{
			dna_info::SyncInfo_C message;
			if (message.ParseFromCodedStream(&payload_input_stream) == false)
			{
				std::cerr << "[Error] parse error" << std::endl;
				break;
			}
			handler.Handle(message);
		}
		break;
		case dna_info::SYNC_INFO_S:
		{
			dna_info::SyncInfo_S message;
			if (message.ParseFromCodedStream(&payload_input_stream) == false)
			{
				std::cerr << "[Error] parse error" << std::endl;
				break;
			}
			handler.Handle(message);
		}
		break;
		/*
		case dna_info::CHAT_REQ:
		{
		}
		break;
		*/
		case dna_info::CHAT_RES:
		{
		}
		break;
		}
	}

	return nRead;
}

void WriteMessageToStream(
	const protobuf::Message& message,
	dna_info::packet_type message_type,
	protobuf::io::CodedOutputStream& stream)
{
	PacketHeader header;
	header.size = message.ByteSize();
	header.type = message_type;
	stream.WriteRaw(&header, sizeof(PacketHeader));
	message.SerializeToCodedStream(&stream);
}

void PacketHandler::Handle(const dna_info::LoginResponse& message)
{
	PrintMessage(message);

	m_client->Set_login();
}

void PacketHandler::Handle(const dna_info::UserInfo& message)
{
	PrintMessage(message);

	std::cout << "[User Info]" << std::endl;
	std::cout << message.identify_id() << std::endl;
	std::cout << message.channel_num() << std::endl;
	std::cout << message.session_num() << std::endl;

	/* 클라이언트에 할당 받은 채널을 설정합니다. */
	m_client->Set_Channel(message.channel_num());

	/* 초기 유저 정보를 설정합니다. */
	User_info user_info;

	user_info.user_name = message.identify_id();
	user_info.xpos = 0;
	user_info.ypos = 0;
	user_info.A_type_pow = 0; 
	user_info.B_type_pow = 0;
	user_info.C_type_pow = 0;
	user_info.type = MALE;
	user_info.vec = 0;
	
	m_client->Set_UserInfo(&user_info);

	/* 서버와의 동기화를 시작합니다. */
	m_client->Start_Sync();
}