#include "Network/Sockets/TcpSocket.h"
#include "Network/NetworkTypes.h"
#include "Shared/Common/Types.h"

#include <iostream>
#include <thread>
#include <chrono>

using namespace Helianthus::Network;
using namespace Helianthus::Network::Sockets;

int main()
{
    std::cout << "Starting TcpSocket test..." << std::endl;
    
    // Create server socket
    TcpSocket ServerSocket;
    NetworkAddress ServerAddr("127.0.0.1", 8080);
    
    // Bind and listen
    NetworkError BindResult = ServerSocket.Bind(ServerAddr);
    if (BindResult != NetworkError::NONE)
    {
        std::cout << "Server bind failed: " << static_cast<int>(BindResult) << std::endl;
        return 1;
    }
    
    NetworkError ListenResult = ServerSocket.Listen(5);
    if (ListenResult != NetworkError::NONE)
    {
        std::cout << "Server listen failed: " << static_cast<int>(ListenResult) << std::endl;
        return 1;
    }
    
    std::cout << "Server listening on " << ServerAddr.ToString() << std::endl;
    
    // Create client socket in a separate thread
    std::thread ClientThread([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        TcpSocket ClientSocket;
        NetworkError ConnectResult = ClientSocket.Connect(ServerAddr);
        if (ConnectResult != NetworkError::NONE)
        {
            std::cout << "Client connect failed: " << static_cast<int>(ConnectResult) << std::endl;
            return;
        }
        
        std::cout << "Client connected successfully" << std::endl;
        
        // Send a test message
        std::string TestMessage = "Hello, Server!";
        size_t BytesSent = 0;
        NetworkError SendResult = ClientSocket.Send(
            reinterpret_cast<const char*>(TestMessage.data()),
            TestMessage.size(), 
            BytesSent
        );
        
        if (SendResult == NetworkError::NONE)
        {
            std::cout << "Client sent " << BytesSent << " bytes" << std::endl;
        }
        else
        {
            std::cout << "Client send failed: " << static_cast<int>(SendResult) << std::endl;
        }
        
        // Wait a bit then disconnect
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        ClientSocket.Disconnect();
    });
    
    // Accept connection on server
    NetworkError AcceptResult = ServerSocket.Accept();
    if (AcceptResult != NetworkError::NONE)
    {
        std::cout << "Server accept failed: " << static_cast<int>(AcceptResult) << std::endl;
        ClientThread.join();
        return 1;
    }
    
    std::cout << "Server accepted connection" << std::endl;
    
    // Wait for client to finish
    ClientThread.join();
    
    // Cleanup
    ServerSocket.Disconnect();
    
    std::cout << "TcpSocket test completed successfully!" << std::endl;
    return 0;
}
