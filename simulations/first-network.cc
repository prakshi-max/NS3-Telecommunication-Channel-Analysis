#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main()
{
    // ============================================================
    // 1. CREATE TWO NODES
    // ============================================================

    NodeContainer nodes;
    nodes.Create(2);

    // Node 0 = Sender
    // Node 1 = Receiver


    // ============================================================
    // 2. CREATE POINT-TO-POINT COMMUNICATION CHANNEL
    // ============================================================

    PointToPointHelper pointToPoint;

    pointToPoint.SetDeviceAttribute(
        "DataRate",
        StringValue("10Mbps")
    );

    pointToPoint.SetChannelAttribute(
        "Delay",
        StringValue("10ms")
    );


    // ============================================================
    // 3. INSTALL NETWORK DEVICES
    // ============================================================

    NetDeviceContainer devices;
    devices = pointToPoint.Install(nodes);


    // ============================================================
    // 4. INSTALL INTERNET PROTOCOL STACK
    // ============================================================

    InternetStackHelper stack;
    stack.Install(nodes);


    // ============================================================
    // 5. ASSIGN IP ADDRESSES
    // ============================================================

    Ipv4AddressHelper address;

    address.SetBase(
        "10.1.1.0",
        "255.255.255.0"
    );

    Ipv4InterfaceContainer interfaces;
    interfaces = address.Assign(devices);


    // ============================================================
    // 6. UDP SERVER — RECEIVER
    // ============================================================

    uint16_t port = 8080;

    UdpServerHelper server(port);

    ApplicationContainer serverApp;
    serverApp = server.Install(nodes.Get(1));

    Ptr<UdpServer> serverPtr =
        DynamicCast<UdpServer>(serverApp.Get(0));

    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(15.0));


    // ============================================================
    // 7. UDP CLIENT — SENDER
    // ============================================================

    UdpClientHelper client(
        interfaces.GetAddress(1),
        port
    );

    // Exactly 100 packets
    client.SetAttribute(
        "MaxPackets",
        UintegerValue(100)
    );

    // One packet every 100 ms
    client.SetAttribute(
        "Interval",
        TimeValue(MilliSeconds(100))
    );

    // Packet size = 1024 bytes
    client.SetAttribute(
        "PacketSize",
        UintegerValue(1024)
    );

    ApplicationContainer clientApp;
    clientApp = client.Install(nodes.Get(0));

    // Give enough time to send all 100 packets
    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(14.0));


    // ============================================================
    // 8. RUN SIMULATION
    // ============================================================

    Simulator::Run();


    // ============================================================
    // 9. GET RECEIVED PACKETS
    // ============================================================

    uint32_t packetsReceived =
        serverPtr->GetReceived();

    // We configured the client to send 100 packets.
    uint32_t packetsSent = 100;

    uint32_t packetsLost =
        packetsSent - packetsReceived;

    double packetLossRate =
        (static_cast<double>(packetsLost)
         / packetsSent) * 100.0;


    // ============================================================
    // 10. DISPLAY RESULTS
    // ============================================================

    std::cout << std::endl;
    std::cout << "========================================"
              << std::endl;

    std::cout << "       BASELINE PACKET ANALYSIS"
              << std::endl;

    std::cout << "========================================"
              << std::endl;

    std::cout << "Packets Sent     : "
              << packetsSent << std::endl;

    std::cout << "Packets Received : "
              << packetsReceived << std::endl;

    std::cout << "Packets Lost     : "
              << packetsLost << std::endl;

    std::cout << "Packet Loss Rate : "
              << packetLossRate << "%"
              << std::endl;

    std::cout << "Channel Bandwidth: 10 Mbps"
              << std::endl;

    std::cout << "Channel Delay    : 10 ms"
              << std::endl;

    std::cout << "Packet Size      : 1024 bytes"
              << std::endl;

    std::cout << "========================================"
              << std::endl;


    // ============================================================
    // 11. DESTROY SIMULATION
    // ============================================================

    Simulator::Destroy();

    return 0;
}
