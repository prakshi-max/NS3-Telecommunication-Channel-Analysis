#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

using namespace ns3;

class ImagePacketHeader : public Header
{
public:
  ImagePacketHeader() : m_sequence(0) {}
  explicit ImagePacketHeader(uint32_t sequence) : m_sequence(sequence) {}

  static TypeId GetTypeId()
  {
    static TypeId tid =
        TypeId("ImagePacketHeader")
            .SetParent<Header>()
            .AddConstructor<ImagePacketHeader>();
    return tid;
  }

  TypeId GetInstanceTypeId() const override { return GetTypeId(); }
  uint32_t GetSerializedSize() const override { return 4; }

  void Serialize(Buffer::Iterator start) const override
  {
    start.WriteHtonU32(m_sequence);
  }

  uint32_t Deserialize(Buffer::Iterator start) override
  {
    m_sequence = start.ReadNtohU32();
    return 4;
  }

  void Print(std::ostream &os) const override
  {
    os << "Sequence=" << m_sequence;
  }

  uint32_t GetSequence() const { return m_sequence; }

private:
  uint32_t m_sequence;
};

class ImageReceiver : public Application
{
public:
  ImageReceiver() : m_socket(nullptr), m_totalPackets(0), m_payloadSize(0) {}

  void Setup(Ptr<Socket> socket, uint32_t totalPackets, uint32_t payloadSize)
  {
    m_socket = socket;
    m_totalPackets = totalPackets;
    m_payloadSize = payloadSize;
    m_received.resize(totalPackets);
  }

  const std::vector<std::vector<uint8_t>>& GetReceivedPackets() const
  {
    return m_received;
  }

protected:
  void StartApplication() override
  {
    m_socket->SetRecvCallback(
        MakeCallback(&ImageReceiver::ReceivePacket, this));
  }

  void StopApplication() override
  {
    m_socket->SetRecvCallback(
        MakeNullCallback<void, Ptr<Socket>>());
  }

private:
  void ReceivePacket(Ptr<Socket> socket)
  {
    Address from;

    while (Ptr<Packet> packet = socket->RecvFrom(from))
    {
      ImagePacketHeader header;

      if (packet->GetSize() < header.GetSerializedSize())
        continue;

      packet->RemoveHeader(header);

      uint32_t sequence = header.GetSequence();

      if (sequence >= m_totalPackets)
        continue;

      uint32_t copySize =
          std::min<uint32_t>(packet->GetSize(), m_payloadSize);

      m_received[sequence].resize(copySize);

      if (copySize > 0)
        packet->CopyData(m_received[sequence].data(), copySize);
    }
  }

  Ptr<Socket> m_socket;
  uint32_t m_totalPackets;
  uint32_t m_payloadSize;
  std::vector<std::vector<uint8_t>> m_received;
};

static void SendImagePacket(Ptr<Socket> socket,
                            Ptr<Packet> packet,
                            InetSocketAddress destination)
{
  socket->SendTo(packet, 0, destination);
}

static bool ReadBinaryFile(const std::string& filename,
                           std::vector<uint8_t>& data)
{
  std::ifstream file(filename, std::ios::binary | std::ios::ate);

  if (!file.is_open())
    return false;

  std::streamsize size = file.tellg();

  if (size < 0)
    return false;

  file.seekg(0, std::ios::beg);
  data.resize(static_cast<size_t>(size));

  if (size > 0)
    file.read(reinterpret_cast<char*>(data.data()), size);

  return file.good() || file.eof();
}

static bool WriteBinaryFile(const std::string& filename,
                            const std::vector<uint8_t>& data)
{
  std::ofstream file(filename, std::ios::binary);

  if (!file.is_open())
    return false;

  if (!data.empty())
  {
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
  }

  return file.good();
}

int main(int argc, char* argv[])
{
  CommandLine cmd(__FILE__);
  cmd.Parse(argc, argv);

  const std::string bandwidth = "1000Kbps";
  const std::string delay = "10ms";
  const uint32_t packetSize = 1024;

  const std::vector<std::string> imageFiles = {
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
      "scratch/12_city.jpg"};

  std::cout << "\n============================================================\n";
  std::cout << "       NS-3 MULTI-IMAGE TRANSMISSION EXPERIMENT\n";
  std::cout << "============================================================\n";
  std::cout << "Channel Bandwidth : " << bandwidth << "\n";
  std::cout << "Channel Delay     : " << delay << "\n";
  std::cout << "Packet Payload    : " << packetSize << " bytes\n";
  std::cout << "Number of Images  : " << imageFiles.size() << "\n";
  std::cout << "============================================================\n\n";

  for (size_t imageIndex = 0; imageIndex < imageFiles.size(); ++imageIndex)
  {
    std::vector<uint8_t> imageData;

    if (!ReadBinaryFile(imageFiles[imageIndex], imageData))
    {
      std::cout << "ERROR: Could not open "
                << imageFiles[imageIndex] << "\n";
      continue;
    }

    uint32_t totalBytes = static_cast<uint32_t>(imageData.size());
    uint32_t totalPackets =
        (totalBytes + packetSize - 1) / packetSize;

    NodeContainer nodes;
    nodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate",
                                    StringValue(bandwidth));
    pointToPoint.SetChannelAttribute("Delay",
                                     StringValue(delay));

    NetDeviceContainer devices = pointToPoint.Install(nodes);

    InternetStackHelper internet;
    internet.Install(nodes);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);

    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");

    Ptr<Socket> receiverSocket =
        Socket::CreateSocket(nodes.Get(1), tid);

    InetSocketAddress receiverAddress(Ipv4Address::GetAny(), 9000);

    if (receiverSocket->Bind(receiverAddress) != 0)
    {
      std::cout << "ERROR: Receiver socket bind failed.\n";
      Simulator::Destroy();
      continue;
    }

    Ptr<ImageReceiver> receiver =
        CreateObject<ImageReceiver>();

    receiver->Setup(receiverSocket, totalPackets, packetSize);
    nodes.Get(1)->AddApplication(receiver);
    receiver->SetStartTime(Seconds(1.0));
    receiver->SetStopTime(Seconds(10.0));

    Ptr<Socket> senderSocket =
        Socket::CreateSocket(nodes.Get(0), tid);

    InetSocketAddress destination(interfaces.GetAddress(1), 9000);

    std::cout << "------------------------------------------------------------\n";
    std::cout << "Image " << (imageIndex + 1)
              << " / " << imageFiles.size() << "\n";
    std::cout << "------------------------------------------------------------\n";
    std::cout << "File          : " << imageFiles[imageIndex] << "\n";
    std::cout << "Image Size    : " << totalBytes << " bytes\n";
    std::cout << "Total Packets : " << totalPackets << "\n";

    uint32_t packetsSent = 0;

    for (uint32_t sequence = 0; sequence < totalPackets; ++sequence)
    {
      uint32_t offset = sequence * packetSize;
      uint32_t bytesToSend =
          std::min<uint32_t>(packetSize, totalBytes - offset);

      Ptr<Packet> packet =
          Create<Packet>(imageData.data() + offset, bytesToSend);

      ImagePacketHeader header(sequence);
      packet->AddHeader(header);

      Time sendTime =
          Seconds(2.0) + MilliSeconds(sequence);

      Simulator::Schedule(sendTime,
                          &SendImagePacket,
                          senderSocket,
                          packet,
                          destination);

      ++packetsSent;
    }

    Simulator::Stop(Seconds(10.0));
    Simulator::Run();

    const auto& receivedPackets = receiver->GetReceivedPackets();

    uint32_t packetsReceived = 0;
    uint32_t receivedBytes = 0;
    std::vector<uint32_t> lostPackets;

    for (uint32_t sequence = 0; sequence < totalPackets; ++sequence)
    {
      if (!receivedPackets[sequence].empty())
      {
        ++packetsReceived;
        receivedBytes +=
            static_cast<uint32_t>(receivedPackets[sequence].size());
      }
      else
      {
        lostPackets.push_back(sequence);
      }
    }

    uint32_t packetsLost = totalPackets - packetsReceived;

    double lossRate =
        totalPackets == 0
            ? 0.0
            : (100.0 * packetsLost / totalPackets);

    std::vector<uint8_t> reconstructed;
    reconstructed.reserve(totalBytes);

    for (uint32_t sequence = 0; sequence < totalPackets; ++sequence)
    {
      uint32_t expectedBytes =
          std::min<uint32_t>(
              packetSize,
              totalBytes - sequence * packetSize);

      if (!receivedPackets[sequence].empty())
      {
        reconstructed.insert(
            reconstructed.end(),
            receivedPackets[sequence].begin(),
            receivedPackets[sequence].end());

        if (receivedPackets[sequence].size() < expectedBytes)
        {
          reconstructed.insert(
              reconstructed.end(),
              expectedBytes - receivedPackets[sequence].size(),
              0);
        }
      }
      else
      {
        reconstructed.insert(
            reconstructed.end(), expectedBytes, 0);
      }
    }

    size_t slashPos =
        imageFiles[imageIndex].find_last_of("/\\");
    std::string baseName =
        slashPos == std::string::npos
            ? imageFiles[imageIndex]
            : imageFiles[imageIndex].substr(slashPos + 1);

    size_t dotPos = baseName.find_last_of('.');
    std::string stem =
        dotPos == std::string::npos
            ? baseName
            : baseName.substr(0, dotPos);

    std::string outputFile =
        "scratch/received_" + stem + ".jpg";

    bool saved = WriteBinaryFile(outputFile, reconstructed);

    std::cout << "\nRESULT\n";
    std::cout << "Packets Sent     : " << packetsSent << "\n";
    std::cout << "Packets Received : " << packetsReceived << "\n";
    std::cout << "Packets Lost     : " << packetsLost << "\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Packet Loss Rate : " << lossRate << "%\n";
    std::cout << "Received Bytes   : " << receivedBytes << "\n";

    if (lostPackets.empty())
    {
      std::cout << "Lost Packet Nos. : None\n";
    }
    else
    {
      std::cout << "Lost Packet Nos. : ";
      for (size_t i = 0; i < lostPackets.size(); ++i)
      {
        std::cout << lostPackets[i];
        if (i + 1 < lostPackets.size())
          std::cout << ", ";
      }
      std::cout << "\n";
    }

    std::cout << "Received Image   : "
              << (saved ? outputFile : "ERROR creating file")
              << "\n\n";

    Simulator::Destroy();
  }

  std::cout << "============================================================\n";
  std::cout << "              EXPERIMENT COMPLETED\n";
  std::cout << "============================================================\n";

  return 0;
}
