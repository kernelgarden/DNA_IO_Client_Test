#pragma comment(lib, "libprotobufd.lib")

#include "dummy_client.cpp"

int main(int argc, char **argv)
{
	DummyClientTest TestManager;

	TestManager.Test();

	return 0;
}