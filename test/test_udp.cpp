#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <cstring>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

extern "C" {
#include "utils/udp.h"
}

namespace {

TEST(UdpControlTest, BoundsAndTerminatesFullDatagramResponse)
{
    int server = socket(AF_INET, SOCK_DGRAM, 0);
    ASSERT_GE(server, 0);
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    ASSERT_EQ(0, bind(server, reinterpret_cast<sockaddr *>(&address), sizeof(address)));
    socklen_t address_size = sizeof(address);
    ASSERT_EQ(0, getsockname(server, reinterpret_cast<sockaddr *>(&address), &address_size));

    pid_t child = fork();
    ASSERT_NE(-1, child);
    if (child == 0) {
        char request[32];
        sockaddr_in client = {};
        socklen_t client_size = sizeof(client);
        if (recvfrom(server, request, sizeof(request), 0, reinterpret_cast<sockaddr *>(&client), &client_size) < 0)
            _exit(1);
        if (sendto(server, "VERSION 1", 9, 0, reinterpret_cast<sockaddr *>(&client), client_size) != 9)
            _exit(2);
        if (recvfrom(server, request, sizeof(request), 0, reinterpret_cast<sockaddr *>(&client), &client_size) < 0)
            _exit(3);
        if (sendto(server, "ABCDEFGH", 8, 0, reinterpret_cast<sockaddr *>(&client), client_size) != 8)
            _exit(4);
        _exit(0);
    }

    char response[8];
    memset(response, 'X', sizeof(response));
    EXPECT_EQ(0, udp_send_receive("127.0.0.1", ntohs(address.sin_port), "GET_STATUS", response, sizeof(response)));
    EXPECT_STREQ("ABCDEFG", response);
    EXPECT_EQ('\0', response[7]);

    int status = 0;
    ASSERT_EQ(child, waitpid(child, &status, 0));
    EXPECT_TRUE(WIFEXITED(status));
    EXPECT_EQ(0, WEXITSTATUS(status));
    close(server);
}

TEST(UdpControlTest, RejectsResponseBuffersWithoutTerminatorSpace)
{
    char response = 'X';
    EXPECT_EQ(-1, udp_send_receive("127.0.0.1", 55355, "GET_STATUS", nullptr, 8));
    EXPECT_EQ(-1, udp_send_receive("127.0.0.1", 55355, "GET_STATUS", &response, 1));
    EXPECT_EQ('X', response);
}

} // namespace
