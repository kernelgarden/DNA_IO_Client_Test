#pragma once

#include <string>
#include <boost/asio.hpp>
/*
* ���� ������ �����ϴ� ��ü
*/
struct User_info
{
	//int user_id; // ������ ���� id
	std::string user_name;
	int xpos;
	int ypos;
	int vec;
	int type;
	int A_type_pow;
	int B_type_pow;
	int C_type_pow;
};