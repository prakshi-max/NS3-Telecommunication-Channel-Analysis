#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

int main()
{
    // ============================================================
    // 1. CREATE TWO NODES
    // ============================================================

    NodeContainer nodes;
    nodes.Create(2);


    // ============================================================
    // 2. CREATE COMMUNICATION CHANNEL
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
    // 4. INSTALL INTERNET STACK
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

    serverApp.Start(Seconds(1.0));
    serverApp.Stop(Seconds(15.0));


    // ============================================================
    // 7. UDP CLIENT — SENDER
    // ============================================================

    UdpClientHelper client(
        interfaces.GetAddress(1),
        port
    );

    client.SetAttribute(
        "MaxPackets",
        UintegerValue(1000)
    );

    client.SetAttribute(
        "Interval",
        TimeValue(MilliSeconds(10))
    );

    client.SetAttribute(
        "PacketSize",
        UintegerValue(1024)
    );

    ApplicationContainer clientApp;

    clientApp = client.Install(nodes.Get(0));

    clientApp.Start(Seconds(2.0));
    clientApp.Stop(Seconds(14.0));


    // ============================================================
    // 8. INSTALL FLOW MONITOR
    // ============================================================

    FlowMonitorHelper flowMonitorHelper;

    Ptr<FlowMonitor> monitor =
        flowMonitorHelper.InstallAll();


    // ============================================================
    // 9. STOP SIMULATION
    // ============================================================

    Simulator::Stop(Seconds(16.0));


    // ============================================================
    // 10. RUN SIMULATION
    // ============================================================

    Simulator::Run();


    // ============================================================
    // 11. GET FLOW STATISTICS
    // ============================================================

    monitor->CheckForLostPackets();

    std::map<FlowId, FlowMonitor::FlowStats> stats =
        monitor->GetFlowStats();


    // ============================================================
    // 12. DISPLAY RESULTS
    // ============================================================

    std::cout << std::endl;

    std::cout << "========================================"
              << std::endl;

    std::cout << "       THROUGHPUT ANALYSIS"
              << std::endl;

    std::cout << "========================================"
              << std::endl;


    for (const auto& flow : stats)
    {
        const FlowMonitor::FlowStats& flowStats =
            flow.second;

        uint32_t packetsSent =
            flowStats.txPackets;

        uint32_t packetsReceived =
            flowStats.rxPackets;

        uint32_t packetsLost =
            flowStats.lostPackets;

        double packetLossRate = 0.0;

        if (packetsSent > 0)
        {
            packetLossRate =
                (static_cast<double>(packetsLost)
                 / packetsSent) * 100.0;
        }


        // --------------------------------------------------------
        // CALCULATE THROUGHPUT
        // --------------------------------------------------------

        double throughputMbps = 0.0;

        if (flowStats.timeLastRxPacket >
            flowStats.timeFirstRxPacket)
        {
            double duration =
                (flowStats.timeLastRxPacket -
                 flowStats.timeFirstRxPacket)
                .GetSeconds();

            throughputMbps =
                (flowStats.rxBytes * 8.0)
                / duration
                / 1000000.0;
        }


        // --------------------------------------------------------
        // CALCULATE AVERAGE LATENCY
        // --------------------------------------------------------

        double averageLatencyMs = 0.0;

        if (packetsReceived > 0)
        {
            averageLatencyMs =
                (flowStats.delaySum.GetSeconds()
                 / packetsReceived)
                * 1000.0;
        }


        // --------------------------------------------------------
        // DISPLAY
        // --------------------------------------------------------

        std::cout << "Packets Sent     : "
                  << packetsSent
                  << std::endl;

        std::cout << "Packets Received : "
                  << packetsReceived
                  << std::endl;

        std::cout << "Packets Lost     : "
                  << packetsLost
                  << std::endl;

        std::cout << "Packet Loss Rate : "
                  << packetLossRate
                  << "%"
                  << std::endl;

        std::cout << "Received Bytes   : "
                  << flowStats.rxBytes
                  << std::endl;

        std::cout << "Throughput       : "
                  << throughputMbps
                  << " Mbps"
                  << std::endl;

        std::cout << "Average Latency  : "
                  << averageLatencyMs
                  << " ms"
                  << std::endl;
    }


    std::cout << "========================================"
              << std::endl;


    // ============================================================
    // 13. DESTROY SIMULATION
    // ============================================================

    Simulator::Destroy();

    return 0;
}
