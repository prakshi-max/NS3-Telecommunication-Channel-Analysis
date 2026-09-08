#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"

#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <cmath>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("VideoTransmission");

class SequenceHeader : public Header
{
public:
    SequenceHeader() : m_sequence(0), m_frame(0) {}

    SequenceHeader(uint32_t sequence, uint32_t frame)
        : m_sequence(sequence), m_frame(frame) {}

    static TypeId GetTypeId()
    {
        static TypeId tid =
            TypeId("SequenceHeader")
                .SetParent<Header>()
                .AddConstructor<SequenceHeader>();
        return tid;
    }

    TypeId GetInstanceTypeId() const override
    {
        return GetTypeId();
    }

    void Serialize(Buffer::Iterator start) const override
    {
        start.WriteHtonU32(m_sequence);
        start.WriteHtonU32(m_frame);
    }

    uint32_t Deserialize(Buffer::Iterator start) override
    {
        m_sequence = start.ReadNtohU32();
        m_frame = start.ReadNtohU32();
        return 8;
    }

    uint32_t GetSerializedSize() const override
    {
        return 8;
    }

    void Print(std::ostream &os) const override
    {
        os << "Sequence=" << m_sequence
           << ", Frame=" << m_frame;
    }

    uint32_t GetSequence() const
    {
        return m_sequence;
    }

    uint32_t GetFrame() const
    {
        return m_frame;
    }

private:
    uint32_t m_sequence;
    uint32_t m_frame;
};

struct PacketInfo
{
    uint32_t frame;
    uint32_t sequence;
    uint32_t payloadSize;
};

static std::vector<PacketInfo> receivedPackets;

static uint32_t totalSent = 0;
static uint32_t totalReceived = 0;
static uint32_t totalBytesReceived = 0;

static std::map<uint32_t, uint32_t> frameSent;
static std::map<uint32_t, uint32_t> frameReceived;
static std::map<uint32_t, uint32_t> frameBytesReceived;

static Ptr<Socket> senderSocket;
static Ptr<Socket> receiverSocket;

static std::vector<std::string> imageFiles = {
    "scratch/01_chess.jpg",
    "scratch/02_sudoku.jpg",
    "scratch/03_geometric.jpg",
    "scratch/04_building.jpg",
    "scratch/05_traffic.jpg",
    "scratch/06_car.jpg",
    "scratch/07_portrait.jpg",
    "scratch/08_animal.jpg",
    "scratch/09_map.jpg",
    "scratch/10_chart.jpg",
    "scratch/11_document.jpg",
    "scratch/12_city.jpg"
};

static const uint32_t PACKET_PAYLOAD_SIZE = 1024;

// Video-like frame rate: 10 frames per second
static const double FRAME_INTERVAL = 0.1;

// Packet interval inside each frame
static const double PACKET_INTERVAL = 0.001;

static const uint32_t CHANNEL_BANDWIDTH_KBPS = 1000;

static std::vector<std::vector<uint8_t>> frameData;

static uint32_t globalSequence = 0;

void ReceivePacket(Ptr<Socket> socket)
{
    Address from;

    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
        SequenceHeader header;

        if (packet->GetSize() < header.GetSerializedSize())
        {
            continue;
        }

        packet->RemoveHeader(header);

        uint32_t frame = header.GetFrame();
        uint32_t sequence = header.GetSequence();

        uint32_t payloadSize = packet->GetSize();

        totalReceived++;
        totalBytesReceived += payloadSize;

        frameReceived[frame]++;
        frameBytesReceived[frame] += payloadSize;

        PacketInfo info;
        info.frame = frame;
        info.sequence = sequence;
        info.payloadSize = payloadSize;

        receivedPackets.push_back(info);
    }
}

void SendPacket(uint32_t frameIndex, uint32_t packetIndex)
{
    if (frameIndex >= frameData.size())
    {
        return;
    }

    const std::vector<uint8_t> &data = frameData[frameIndex];

    uint32_t start =
        packetIndex * PACKET_PAYLOAD_SIZE;

    if (start >= data.size())
    {
        return;
    }

    uint32_t remaining = data.size() - start;

    uint32_t payloadSize =
        std::min(PACKET_PAYLOAD_SIZE, remaining);

    Ptr<Packet> packet =
        Create<Packet>(&data[start], payloadSize);

    SequenceHeader header(globalSequence++, frameIndex);

    packet->AddHeader(header);

    senderSocket->Send(packet);

    totalSent++;

    frameSent[frameIndex]++;

    if (start + payloadSize < data.size())
    {
        Simulator::Schedule(
            Seconds(PACKET_INTERVAL),
            &SendPacket,
            frameIndex,
            packetIndex + 1);
    }
}

void StartFrame(uint32_t frameIndex)
{
    if (frameIndex >= frameData.size())
    {
        return;
    }

    std::cout << "\nStarting Frame "
              << frameIndex + 1
              << " : "
              << imageFiles[frameIndex]
              << std::endl;

    SendPacket(frameIndex, 0);

    if (frameIndex + 1 < frameData.size())
    {
        Simulator::Schedule(
            Seconds(FRAME_INTERVAL),
            &StartFrame,
            frameIndex + 1);
    }
}

int main(int argc, char *argv[])
{
    CommandLine cmd(__FILE__);
    cmd.Parse(argc, argv);

    std::cout << "\n========================================"
              << std::endl;
    std::cout << "VIDEO-LIKE IMAGE TRANSMISSION"
              << std::endl;
    std::cout << "========================================"
              << std::endl;

    std::cout << "Channel Bandwidth : "
              << CHANNEL_BANDWIDTH_KBPS
              << " Kbps" << std::endl;

    std::cout << "Frame Rate        : 10 FPS" << std::endl;

    std::cout << "Packet Payload    : "
              << PACKET_PAYLOAD_SIZE
              << " bytes" << std::endl;

    std::cout << "Packet Interval   : "
              << PACKET_INTERVAL
              << " seconds" << std::endl;

    std::cout << "Frame Interval    : "
              << FRAME_INTERVAL
              << " seconds" << std::endl;

    // ------------------------------------------------
    // Read all image files
    // ------------------------------------------------

    frameData.resize(imageFiles.size());

    for (uint32_t i = 0; i < imageFiles.size(); ++i)
    {
        std::ifstream file(
            imageFiles[i],
            std::ios::binary);

        if (!file)
        {
            std::cerr << "ERROR: Cannot open "
                      << imageFiles[i]
                      << std::endl;

            return 1;
        }

        file.seekg(0, std::ios::end);

        std::streamsize size =
            file.tellg();

        file.seekg(0, std::ios::beg);

        frameData[i].resize(size);

        file.read(
            reinterpret_cast<char *>(frameData[i].data()),
            size);

        file.close();

        uint32_t packets =
            (size + PACKET_PAYLOAD_SIZE - 1) /
            PACKET_PAYLOAD_SIZE;

        std::cout << "\nFrame "
                  << i + 1
                  << " : "
                  << imageFiles[i]
                  << std::endl;

        std::cout << "Image Size : "
                  << size
                  << " bytes" << std::endl;

        std::cout << "Packets    : "
                  << packets
                  << std::endl;
    }

    // ------------------------------------------------
    // Create NS-3 nodes
    // ------------------------------------------------

    NodeContainer nodes;

    nodes.Create(2);

    PointToPointHelper pointToPoint;

    pointToPoint.SetDeviceAttribute(
        "DataRate",
        StringValue("1000Kbps"));

    pointToPoint.SetChannelAttribute(
        "Delay",
        StringValue("10ms"));

    NetDeviceContainer devices =
        pointToPoint.Install(nodes);

    InternetStackHelper internet;

    internet.Install(nodes);

    Ipv4AddressHelper address;

    address.SetBase(
        "10.1.1.0",
        "255.255.255.0");

    Ipv4InterfaceContainer interfaces =
        address.Assign(devices);

    // ------------------------------------------------
    // UDP sockets
    // ------------------------------------------------

    uint16_t port = 5000;

    receiverSocket =
        Socket::CreateSocket(
            nodes.Get(1),
            UdpSocketFactory::GetTypeId());

    InetSocketAddress local =
        InetSocketAddress(
            Ipv4Address::GetAny(),
            port);

    receiverSocket->Bind(local);

    receiverSocket->SetRecvCallback(
        MakeCallback(&ReceivePacket));

    senderSocket =
        Socket::CreateSocket(
            nodes.Get(0),
            UdpSocketFactory::GetTypeId());

    InetSocketAddress remote =
        InetSocketAddress(
            interfaces.GetAddress(1),
            port);

    senderSocket->Connect(remote);

    // ------------------------------------------------
    // Start continuous video-like transmission
    // ------------------------------------------------

    Simulator::Schedule(
        Seconds(0.0),
        &StartFrame,
        0);

    // Allow enough time for all packets to drain
    Simulator::Stop(
        Seconds(5.0));

    Simulator::Run();

    Simulator::Destroy();

    // ------------------------------------------------
    // Results
    // ------------------------------------------------

    uint32_t totalLost =
        totalSent - totalReceived;

    double lossPercentage =
        totalSent > 0
            ? (100.0 * totalLost / totalSent)
            : 0.0;

    double simulationTime = 5.0;

    double throughputMbps =
        (totalBytesReceived * 8.0) /
        (simulationTime * 1000000.0);

    std::cout << "\n\n========================================"
              << std::endl;

    std::cout << "OVERALL VIDEO TRANSMISSION RESULTS"
              << std::endl;

    std::cout << "========================================"
              << std::endl;

    std::cout << "Total Frames Sent       : "
              << imageFiles.size()
              << std::endl;

    std::cout << "Total Packets Sent      : "
              << totalSent
              << std::endl;

    std::cout << "Total Packets Received  : "
              << totalReceived
              << std::endl;

    std::cout << "Total Packets Lost      : "
              << totalLost
              << std::endl;

    std::cout << "Overall Packet Loss     : "
              << lossPercentage
              << "%"
              << std::endl;

    std::cout << "Received Bytes          : "
              << totalBytesReceived
              << std::endl;

    std::cout << "Approx. Throughput      : "
              << throughputMbps
              << " Mbps"
              << std::endl;

    std::cout << "\n----------------------------------------"
              << std::endl;

    std::cout << "FRAME-WISE RESULTS"
              << std::endl;

    std::cout << "----------------------------------------"
              << std::endl;

    for (uint32_t i = 0; i < imageFiles.size(); ++i)
    {
        uint32_t sent =
            frameSent[i];

        uint32_t received =
            frameReceived[i];

        uint32_t lost =
            sent - received;

        double loss =
            sent > 0
                ? (100.0 * lost / sent)
                : 0.0;

        std::cout << "\nFrame "
                  << i + 1
                  << " ("
                  << imageFiles[i]
                  << ")"
                  << std::endl;

        std::cout << "  Sent       : "
                  << sent
                  << std::endl;

        std::cout << "  Received   : "
                  << received
                  << std::endl;

        std::cout << "  Lost       : "
                  << lost
                  << std::endl;

        std::cout << "  Loss       : "
                  << loss
                  << "%"
                  << std::endl;

        std::cout << "  Bytes Recv : "
                  << frameBytesReceived[i]
                  << std::endl;
    }

    std::cout << "\n========================================"
              << std::endl;

    return 0;
}
