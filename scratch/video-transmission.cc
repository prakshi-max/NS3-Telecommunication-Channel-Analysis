#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ppp-header.h"

#include <fstream>
#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <cmath>
#include <iomanip>

using namespace ns3;

// ============================================================
// PARAMETERS
// ============================================================

static const double FRAME_INTERVAL = 0.1;   // 10 FPS
static const double PACKET_INTERVAL = 0.001; // 1 ms
static const uint32_t PAYLOAD_SIZE = 1024;
static const double SIMULATION_TIME = 20.0;

static const uint16_t PORT = 5000;

// ============================================================
// CUSTOM SEQUENCE HEADER
// ============================================================

class SequenceHeader : public Header
{
public:
  SequenceHeader()
      : m_sequence(0),
        m_frame(0)
  {
  }

  SequenceHeader(uint32_t sequence, uint32_t frame)
      : m_sequence(sequence),
        m_frame(frame)
  {
  }

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

  uint32_t GetSerializedSize() const override
  {
    return 8;
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

  void Print(std::ostream &os) const override
  {
    os << "Seq=" << m_sequence
       << " Frame=" << m_frame;
  }

  uint32_t GetSequence() const
  {
    return m_sequence;
  }

  uint32_t GetFrame() const
  {
    return m_frame;
  }

public:
  uint32_t m_sequence;
  uint32_t m_frame;
};

// ============================================================
// IMAGE FRAME
// ============================================================

struct ImageFrame
{
  std::string name;
  std::vector<uint8_t> data;
};

// ============================================================
// PACKET INFORMATION
// ============================================================

struct PacketInfo
{
  uint32_t sequence;
  uint32_t frame;
  uint32_t bytes;

  double sendTime;
  double txStartTime;
  double receiveTime;
  double dropTime;

  bool sent;
  bool txStarted;
  bool received;
  bool dropped;

  PacketInfo()
      : sequence(0),
        frame(0),
        bytes(0),
        sendTime(-1.0),
        txStartTime(-1.0),
        receiveTime(-1.0),
        dropTime(-1.0),
        sent(false),
        txStarted(false),
        received(false),
        dropped(false)
  {
  }
};

// ============================================================
// VIDEO SENDER
// ============================================================

class VideoSender : public Application
{
public:
  VideoSender();

  void Setup(Ptr<Socket> socket,
             Address destination,
             std::vector<ImageFrame> frames);

  static uint32_t GetTotalSent();
  static uint32_t GetTotalReceived();
  static uint32_t GetTotalDropped();

  static const std::map<uint32_t, PacketInfo> &GetPacketDatabase();

  double GetVideoStartTime() const
  {
    return m_videoStartTime;
  }

  double GetVideoEndTime() const
  {
    return m_videoEndTime;
  }

public:
  void StartApplication() override;
  void StopApplication() override;

  void StartFrame(uint32_t frameIndex);
  void SendFramePacket(uint32_t frameIndex,
                       uint32_t offset);

  Ptr<Socket> m_socket;
  Address m_destination;
  std::vector<ImageFrame> m_frames;

  bool m_running;
  uint32_t m_nextSequence;
  double m_videoStartTime;
  double m_videoEndTime;

  static uint32_t m_totalSent;
  static uint32_t m_totalReceived;
  static uint32_t m_totalDropped;

  static std::map<uint32_t, PacketInfo> m_packetDatabase;
  static std::map<uint64_t, uint32_t> m_uidToSequence;
};

uint32_t VideoSender::m_totalSent = 0;
uint32_t VideoSender::m_totalReceived = 0;
uint32_t VideoSender::m_totalDropped = 0;

std::map<uint32_t, PacketInfo> VideoSender::m_packetDatabase;
std::map<uint64_t, uint32_t> VideoSender::m_uidToSequence;

// ============================================================
// CONSTRUCTOR
// ============================================================

VideoSender::VideoSender()
    : m_socket(nullptr),
      m_destination(),
      m_frames(),
      m_running(false),
      m_nextSequence(0),
      m_videoStartTime(0.0),
      m_videoEndTime(0.0)
{
}

// ============================================================
// SETUP
// ============================================================

void
VideoSender::Setup(Ptr<Socket> socket,
                   Address destination,
                   std::vector<ImageFrame> frames)
{
  m_socket = socket;
  m_destination = destination;
  m_frames = frames;
}

// ============================================================
// START APPLICATION
// ============================================================

void
VideoSender::StartApplication()
{
  m_running = true;
  m_videoStartTime = Simulator::Now().GetSeconds();

  std::cout << "\n=============================================\n";
  std::cout << "VIDEO TRANSMISSION STARTED\n";
  std::cout << "=============================================\n";

  std::cout << "Frame rate       : 10 FPS\n";
  std::cout << "Frame interval   : " << FRAME_INTERVAL << " sec\n";
  std::cout << "Packet interval  : " << PACKET_INTERVAL << " sec\n";
  std::cout << "Payload size     : " << PAYLOAD_SIZE << " bytes\n";

  std::cout << "\nFrame start schedule:\n";

  for (uint32_t i = 0; i < m_frames.size(); ++i)
  {
    double startTime = i * FRAME_INTERVAL;

    std::cout << "Frame " << (i + 1)
              << " (" << m_frames[i].name << ")"
              << " -> "
              << std::fixed << std::setprecision(3)
              << startTime << " s\n";

    Simulator::Schedule(
        Seconds(startTime),
        &VideoSender::StartFrame,
        this,
        i);
  }
}

// ============================================================
// START FRAME
// ============================================================

void
VideoSender::StartFrame(uint32_t frameIndex)
{
  if (!m_running)
  {
    return;
  }

  if (frameIndex >= m_frames.size())
  {
    return;
  }

  std::cout << "\n[FRAME START] "
            << std::fixed << std::setprecision(6)
            << Simulator::Now().GetSeconds()
            << " s"
            << " | Frame " << (frameIndex + 1)
            << " | " << m_frames[frameIndex].name
            << "\n";

  Simulator::Schedule(
      Seconds(0.0),
      &VideoSender::SendFramePacket,
      this,
      frameIndex,
      0);
}

// ============================================================
// SEND FRAME PACKET
// ============================================================

void
VideoSender::SendFramePacket(uint32_t frameIndex,
                             uint32_t offset)
{
  if (!m_running)
  {
    return;
  }

  if (frameIndex >= m_frames.size())
  {
    return;
  }

  const std::vector<uint8_t> &data =
      m_frames[frameIndex].data;

  if (offset >= data.size())
  {
    std::cout << "[FRAME COMPLETE] "
              << std::fixed << std::setprecision(6)
              << Simulator::Now().GetSeconds()
              << " s"
              << " | Frame " << (frameIndex + 1)
              << "\n";

    return;
  }

  uint32_t remaining =
      static_cast<uint32_t>(data.size() - offset);

  uint32_t payloadBytes =
      std::min(PAYLOAD_SIZE, remaining);

  Ptr<Packet> packet =
      Create<Packet>(&data[offset], payloadBytes);

  uint32_t sequence = m_nextSequence++;

  SequenceHeader header(sequence, frameIndex);
  packet->AddHeader(header);

  uint64_t uid = packet->GetUid();

  PacketInfo info;
  info.sequence = sequence;
  info.frame = frameIndex;
  info.bytes = payloadBytes + 8;
  info.sendTime = Simulator::Now().GetSeconds();
  info.sent = true;

  m_packetDatabase[sequence] = info;
  m_uidToSequence[uid] = sequence;

  int sendResult =
      m_socket->SendTo(
          packet,
          0,
          m_destination);

  if (sendResult >= 0)
  {
    m_totalSent++;

    std::cout << "[PACKET SEND ACCEPTED] "
              << std::fixed << std::setprecision(6)
              << Simulator::Now().GetSeconds()
              << " s"
              << " | Frame=" << (frameIndex + 1)
              << " | Seq=" << sequence
              << " | Bytes=" << payloadBytes
              << " | SendResult=" << sendResult
              << "\n";
  }
  else
  {
    auto it = m_packetDatabase.find(sequence);

    if (it != m_packetDatabase.end())
    {
      it->second.dropped = true;
      it->second.dropTime =
          Simulator::Now().GetSeconds();
    }

    m_totalDropped++;

    std::cout << "[PACKET SEND FAILED] "
              << std::fixed << std::setprecision(6)
              << Simulator::Now().GetSeconds()
              << " s"
              << " | Frame=" << (frameIndex + 1)
              << " | Seq=" << sequence
              << " | Bytes=" << payloadBytes
              << "\n";
  }

  uint32_t nextOffset =
      offset + payloadBytes;

  if (nextOffset < data.size())
  {
    Simulator::Schedule(
        Seconds(PACKET_INTERVAL),
        &VideoSender::SendFramePacket,
        this,
        frameIndex,
        nextOffset);
  }
  else
  {
    if (frameIndex == m_frames.size() - 1)
    {
      m_videoEndTime = Simulator::Now().GetSeconds();

      std::cout << "[VIDEO TRANSMISSION END] "
                << std::fixed << std::setprecision(6)
                << m_videoEndTime
                << " s"
                << " | Last Frame=" << (frameIndex + 1)
                << "\n";
    }
    std::cout << "[TRANSMISSION STOP FOR FRAME] "
              << std::fixed << std::setprecision(6)
              << Simulator::Now().GetSeconds()
              << " s"
              << " | Frame " << (frameIndex + 1)
              << "\n";
  }
}

// ============================================================
// STOP APPLICATION
// ============================================================

void
VideoSender::StopApplication()
{
  m_running = false;

  std::cout << "\n=============================================\n";
  std::cout << "VIDEO TRANSMISSION APPLICATION STOPPED\n";
  std::cout << "=============================================\n";

  std::cout << "Time: "
            << std::fixed << std::setprecision(6)
            << Simulator::Now().GetSeconds()
            << " s\n";
}

// ============================================================
// GETTERS
// ============================================================

uint32_t
VideoSender::GetTotalSent()
{
  return m_totalSent;
}

uint32_t
VideoSender::GetTotalReceived()
{
  return m_totalReceived;
}

uint32_t
VideoSender::GetTotalDropped()
{
  return m_totalDropped;
}

const std::map<uint32_t, PacketInfo> &
VideoSender::GetPacketDatabase()
{
  return m_packetDatabase;
}

// ============================================================
// TRACE HELPERS
// ============================================================

static bool
ExtractSequenceFromPacket(Ptr<const Packet> packet,
                          uint32_t &sequence,
                          uint32_t &frame)
{
  Ptr<Packet> copy = packet->Copy();

  PppHeader ppp;
  if (copy->PeekHeader(ppp) == 0)
  {
    return false;
  }

  copy->RemoveHeader(ppp);

  Ipv4Header ip;
  if (copy->PeekHeader(ip) == 0)
  {
    return false;
  }

  copy->RemoveHeader(ip);

  UdpHeader udp;
  if (copy->PeekHeader(udp) == 0)
  {
    return false;
  }

  copy->RemoveHeader(udp);

  SequenceHeader header;

  if (copy->PeekHeader(header) == 0)
  {
    return false;
  }

  sequence = header.GetSequence();
  frame = header.GetFrame();

  return true;
}

// ============================================================
// TX QUEUE DEQUEUE
// ============================================================

static void
TxQueueDequeueCallback(Ptr<const Packet> packet)
{
  uint32_t sequence = 0;
  uint32_t frame = 0;

  bool found =
      ExtractSequenceFromPacket(
          packet,
          sequence,
          frame);

  if (!found)
  {
    uint64_t uid = packet->GetUid();

    auto uidIt =
        VideoSender::m_uidToSequence.find(uid);

    if (uidIt != VideoSender::m_uidToSequence.end())
    {
      sequence = uidIt->second;
      found = true;
    }
  }

  if (!found)
  {
    return;
  }

  auto it =
      VideoSender::m_packetDatabase.find(sequence);

  if (it == VideoSender::m_packetDatabase.end())
  {
    return;
  }

  it->second.txStarted = true;
  it->second.txStartTime =
      Simulator::Now().GetSeconds();

  std::cout << "[TX START] "
            << std::fixed << std::setprecision(6)
            << Simulator::Now().GetSeconds()
            << " s"
            << " | Frame=" << (it->second.frame + 1)
            << " | Seq=" << sequence
            << "\n";
}

// ============================================================
// TX QUEUE DROP
// ============================================================

static void
TxQueueDropCallback(Ptr<const Packet> packet)
{
  uint32_t sequence = 0;
  uint32_t frame = 0;

  bool found =
      ExtractSequenceFromPacket(
          packet,
          sequence,
          frame);

  if (!found)
  {
    uint64_t uid = packet->GetUid();

    auto uidIt =
        VideoSender::m_uidToSequence.find(uid);

    if (uidIt != VideoSender::m_uidToSequence.end())
    {
      sequence = uidIt->second;
      found = true;
    }
  }

  if (!found)
  {
    std::cout << "[QUEUE DROP] "
              << std::fixed << std::setprecision(6)
              << Simulator::Now().GetSeconds()
              << " s"
              << " | Unable to identify packet\n";
    return;
  }

  auto it =
      VideoSender::m_packetDatabase.find(sequence);

  if (it == VideoSender::m_packetDatabase.end())
  {
    return;
  }

  if (!it->second.dropped &&
      !it->second.received)
  {
    it->second.dropped = true;
    it->second.dropTime =
        Simulator::Now().GetSeconds();

    VideoSender::m_totalDropped++;

    std::cout << "[PACKET DROP] "
              << std::fixed << std::setprecision(6)
              << Simulator::Now().GetSeconds()
              << " s"
              << " | Frame=" << (it->second.frame + 1)
              << " | Seq=" << sequence
              << "\n";
  }
}

// ============================================================
// RECEIVER
// ============================================================

class VideoReceiver : public Application
{
public:
  VideoReceiver();

  void Setup(Ptr<Socket> socket);

  uint32_t GetReceivedCount() const;
  double GetAverageLatency() const;
  double GetMinLatency() const;
  double GetMaxLatency() const;
  double GetAverageJitter() const;

  const std::vector<uint32_t> &GetReceivedSequences() const;

private:
  void StartApplication() override;
  void StopApplication() override;

  void HandleRead(Ptr<Socket> socket);

  Ptr<Socket> m_socket;

  std::vector<uint32_t> m_receivedSequences;

  double m_totalLatency;
  double m_minLatency;
  double m_maxLatency;

  double m_previousLatency;
  double m_totalJitter;

  uint32_t m_receivedCount;
  uint32_t m_jitterSamples;
};

// ============================================================
// RECEIVER CONSTRUCTOR
// ============================================================

VideoReceiver::VideoReceiver()
    : m_socket(nullptr),
      m_receivedSequences(),
      m_totalLatency(0.0),
      m_minLatency(1e9),
      m_maxLatency(0.0),
      m_previousLatency(-1.0),
      m_totalJitter(0.0),
      m_receivedCount(0),
      m_jitterSamples(0)
{
}

// ============================================================
// RECEIVER SETUP
// ============================================================

void
VideoReceiver::Setup(Ptr<Socket> socket)
{
  m_socket = socket;
}

// ============================================================
// RECEIVER START
// ============================================================

void
VideoReceiver::StartApplication()
{
  m_socket->SetRecvCallback(
      MakeCallback(
          &VideoReceiver::HandleRead,
          this));

  std::cout << "\n[RECEIVER STARTED] "
            << Simulator::Now().GetSeconds()
            << " s\n";
}

// ============================================================
// RECEIVER STOP
// ============================================================

void
VideoReceiver::StopApplication()
{
  m_socket->SetRecvCallback(
      MakeNullCallback<void, Ptr<Socket>>());

  std::cout << "[RECEIVER STOPPED] "
            << Simulator::Now().GetSeconds()
            << " s\n";
}

// ============================================================
// HANDLE RECEIVE
// ============================================================

void
VideoReceiver::HandleRead(Ptr<Socket> socket)
{
  Address from;

  while (Ptr<Packet> packet =
             socket->RecvFrom(from))
  {
    SequenceHeader header;

    if (packet->PeekHeader(header) == 0)
    {
      continue;
    }

    packet->RemoveHeader(header);

    uint32_t sequence =
        header.GetSequence();

    uint32_t frame =
        header.GetFrame();

    auto it =
        VideoSender::m_packetDatabase.find(sequence);

    if (it == VideoSender::m_packetDatabase.end())
    {
      continue;
    }

    double receiveTime =
        Simulator::Now().GetSeconds();

    it->second.received = true;
    it->second.receiveTime = receiveTime;

    double latency =
        receiveTime - it->second.sendTime;

    m_totalLatency += latency;

    m_minLatency =
        std::min(m_minLatency, latency);

    m_maxLatency =
        std::max(m_maxLatency, latency);

    if (m_previousLatency >= 0.0)
    {
      double jitter =
          std::fabs(latency - m_previousLatency);

      m_totalJitter += jitter;
      m_jitterSamples++;
    }

    m_previousLatency = latency;

    m_receivedSequences.push_back(sequence);

    m_receivedCount++;

    VideoSender::m_totalReceived++;

    std::cout << "[PACKET RECEIVE] "
              << std::fixed << std::setprecision(6)
              << receiveTime
              << " s"
              << " | Frame=" << (frame + 1)
              << " | Seq=" << sequence
              << " | Latency=" << latency
              << " s\n";
  }
}

// ============================================================
// RECEIVER GETTERS
// ============================================================

uint32_t
VideoReceiver::GetReceivedCount() const
{
  return m_receivedCount;
}

double
VideoReceiver::GetAverageLatency() const
{
  if (m_receivedCount == 0)
  {
    return 0.0;
  }

  return m_totalLatency /
         static_cast<double>(m_receivedCount);
}

double
VideoReceiver::GetMinLatency() const
{
  if (m_receivedCount == 0)
  {
    return 0.0;
  }

  return m_minLatency;
}

double
VideoReceiver::GetMaxLatency() const
{
  return m_maxLatency;
}

double
VideoReceiver::GetAverageJitter() const
{
  if (m_jitterSamples == 0)
  {
    return 0.0;
  }

  return m_totalJitter /
         static_cast<double>(m_jitterSamples);
}

const std::vector<uint32_t> &
VideoReceiver::GetReceivedSequences() const
{
  return m_receivedSequences;
}

// ============================================================
// LOAD IMAGE
// ============================================================

static bool
LoadImage(const std::string &filename,
          ImageFrame &frame)
{
  std::string path =
      "scratch/" + filename;

  std::ifstream file(
      path,
      std::ios::binary);

  if (!file)
  {
    std::cout << "ERROR: Cannot open "
              << path << "\n";

    return false;
  }

  file.seekg(
      0,
      std::ios::end);

  std::streamsize size =
      file.tellg();

  file.seekg(
      0,
      std::ios::beg);

  frame.data.resize(
      static_cast<size_t>(size));

  if (size > 0)
  {
    file.read(
        reinterpret_cast<char *>(
            frame.data.data()),
        size);
  }

  frame.name = filename;

  std::cout << "Loaded "
            << filename
            << " | "
            << size
            << " bytes | "
            << ((size + PAYLOAD_SIZE - 1) /
                PAYLOAD_SIZE)
            << " packets\n";

  return true;
}

// ============================================================
// MAIN
// ============================================================

int
main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.Parse(argc, argv);

  std::cout << "\n=============================================\n";
  std::cout << "NS-3 VIDEO-LIKE IMAGE TRANSMISSION\n";
  std::cout << "=============================================\n";

  // ----------------------------------------------------------
  // IMAGE LIST
  // ----------------------------------------------------------

  std::vector<std::string> imageNames =
      {
          "01_chess.jpg",
          "02_sudoku.jpg",
          "03_geometric.jpg",
          "04_building.jpg",
          "05_traffic.jpg",
          "06_car.jpg",
          "07_portrait.jpg",
          "08_animal.jpg",
          "09_map.jpg",
          "10_chart.jpg",
          "11_document.jpg",
          "12_city.jpg"};

  std::vector<ImageFrame> frames;

  for (const auto &name : imageNames)
  {
    ImageFrame frame;

    if (!LoadImage(name, frame))
    {
      return 1;
    }

    frames.push_back(frame);
  }

  // ----------------------------------------------------------
  // NETWORK
  // ----------------------------------------------------------

  NodeContainer nodes;
  nodes.Create(2);

  PointToPointHelper pointToPoint;

  pointToPoint.SetDeviceAttribute(
      "DataRate",
      StringValue("10Mbps"));

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

  // ----------------------------------------------------------
  // SENDER SOCKET
  // ----------------------------------------------------------

  Ptr<Socket> senderSocket =
      Socket::CreateSocket(
          nodes.Get(0),
          UdpSocketFactory::GetTypeId());


  InetSocketAddress destination(
      interfaces.GetAddress(1),
      PORT);

  // ----------------------------------------------------------
  // RECEIVER SOCKET
  // ----------------------------------------------------------

  Ptr<Socket> receiverSocket =
      Socket::CreateSocket(
          nodes.Get(1),
          UdpSocketFactory::GetTypeId());

  receiverSocket->SetAttribute(
      "RcvBufSize",
      UintegerValue(4 * 1024 * 1024));

  receiverSocket->Bind(
      InetSocketAddress(
          Ipv4Address::GetAny(),
          PORT));

  // ----------------------------------------------------------
  // APPLICATIONS
  // ----------------------------------------------------------

  Ptr<VideoSender> sender =
      CreateObject<VideoSender>();

  sender->Setup(
      senderSocket,
      destination,
      frames);

  nodes.Get(0)->AddApplication(sender);

  sender->SetStartTime(Seconds(0.0));
  sender->SetStopTime(Seconds(SIMULATION_TIME));

  Ptr<VideoReceiver> receiver =
      CreateObject<VideoReceiver>();

  receiver->Setup(receiverSocket);

  nodes.Get(1)->AddApplication(receiver);

  receiver->SetStartTime(Seconds(0.0));
  receiver->SetStopTime(Seconds(SIMULATION_TIME));

  // ----------------------------------------------------------
  // TRACES
  // ----------------------------------------------------------

  Config::ConnectWithoutContext(
      "/NodeList/0/DeviceList/0/"
      "$ns3::PointToPointNetDevice/"
      "TxQueue/Dequeue",
      MakeCallback(
          &TxQueueDequeueCallback));

  Config::ConnectWithoutContext(
      "/NodeList/0/DeviceList/0/"
      "$ns3::PointToPointNetDevice/"
      "TxQueue/Drop",
      MakeCallback(
          &TxQueueDropCallback));

  // ----------------------------------------------------------
  // RUN
  // ----------------------------------------------------------

  std::cout << "\n=============================================\n";
  std::cout << "SIMULATION START\n";
  std::cout << "=============================================\n";

  Simulator::Stop(
      Seconds(SIMULATION_TIME));

  Simulator::Run();

  Simulator::Destroy();

  // ==========================================================
  // FINAL REPORT
  // ==========================================================

  uint32_t sent =
      sender->GetTotalSent();

  uint32_t received =
      receiver->GetReceivedCount();

  uint32_t dropped =
      sender->GetTotalDropped();

  uint32_t missing =
      0;

  if (sent >= received + dropped)
  {
    missing =
        sent - received - dropped;
  }

  double lossPercent =
      sent == 0
          ? 0.0
          : 100.0 *
                static_cast<double>(
                    sent - received) /
                static_cast<double>(sent);

  std::cout << "\n\n";
  std::cout << "=============================================\n";
  std::cout << "FINAL VIDEO TRANSMISSION REPORT\n";
  std::cout << "=============================================\n";

  std::cout << "Simulation duration : "
            << SIMULATION_TIME
            << " s\n";

  std::cout << "Video start time    : "
            << sender->GetVideoStartTime()
            << " s\\n";

  std::cout << "Video end time      : "
            << sender->GetVideoEndTime()
            << " s\\n";

  std::cout << "Video transmission  : "
            << (sender->GetVideoEndTime() -
                sender->GetVideoStartTime())
            << " s\\n";

  std::cout << "Frames transmitted  : "
            << frames.size()
            << "\n";

  std::cout << "Total packets sent   : "
            << sent
            << "\n";

  std::cout << "Total packets recv   : "
            << received
            << "\n";

  std::cout << "Queue drops traced   : "
            << dropped
            << "\n";

  std::cout << "Unaccounted missing  : "
            << missing
            << "\n";

  std::cout << "Packet loss          : "
            << std::fixed
            << std::setprecision(4)
            << lossPercent
            << " %\n";

  std::cout << "Average latency      : "
            << receiver->GetAverageLatency() * 1000.0
            << " ms\n";

  std::cout << "Minimum latency      : "
            << receiver->GetMinLatency() * 1000.0
            << " ms\n";

  std::cout << "Maximum latency      : "
            << receiver->GetMaxLatency() * 1000.0
            << " ms\n";

  std::cout << "Average jitter       : "
            << receiver->GetAverageJitter() * 1000.0
            << " ms\n";

  std::cout << "=============================================\n";

  // ==========================================================
  // FRAME-WISE REPORT
  // ==========================================================

  std::cout << "\nFRAME-WISE RESULTS\n";
  std::cout << "---------------------------------------------\n";

  for (uint32_t frame = 0;
       frame < frames.size();
       ++frame)
  {
    uint32_t frameSent = 0;
    uint32_t frameReceived = 0;
    uint32_t frameDropped = 0;

    for (const auto &entry :
         VideoSender::GetPacketDatabase())
    {
      const PacketInfo &info =
          entry.second;

      if (info.frame != frame)
      {
        continue;
      }

      frameSent++;

      if (info.received)
      {
        frameReceived++;
      }

      if (info.dropped)
      {
        frameDropped++;
      }
    }

    uint32_t frameLost =
        frameSent - frameReceived;

    double frameLoss =
        frameSent == 0
            ? 0.0
            : 100.0 *
                  static_cast<double>(
                      frameLost) /
                  static_cast<double>(
                      frameSent);

    std::cout << "Frame "
              << (frame + 1)
              << " | "
              << frames[frame].name
              << " | Sent="
              << frameSent
              << " | Received="
              << frameReceived
              << " | Dropped="
              << frameDropped
              << " | Loss="
              << std::fixed
              << std::setprecision(2)
              << frameLoss
              << "%\n";
  }

  // ==========================================================
  // CSV 1: PACKET METRICS
  // ==========================================================

  std::ofstream packetCsv(
      "video-packet-metrics.csv");

  packetCsv
      << "Sequence,Frame,Bytes,"
      << "SendTime,TxStartTime,"
      << "ReceiveTime,DropTime,"
      << "Status,LatencyMs\n";

  for (const auto &entry :
       VideoSender::GetPacketDatabase())
  {
    const PacketInfo &info =
        entry.second;

    std::string status;

    if (info.received)
    {
      status = "RECEIVED";
    }
    else if (info.dropped)
    {
      status = "DROPPED";
    }
    else
    {
      status = "MISSING";
    }

    double latencyMs = 0.0;

    if (info.received)
    {
      latencyMs =
          (info.receiveTime -
           info.sendTime) *
          1000.0;
    }

    packetCsv
        << info.sequence << ","
        << (info.frame + 1) << ","
        << info.bytes << ","
        << info.sendTime << ","
        << info.txStartTime << ","
        << info.receiveTime << ","
        << info.dropTime << ","
        << status << ","
        << latencyMs
        << "\n";
  }

  packetCsv.close();

  // ==========================================================
  // CSV 2: FRAME SUMMARY
  // ==========================================================

  std::ofstream frameCsv(
      "video-frame-summary.csv");

  frameCsv
      << "Frame,Image,PacketsSent,"
      << "PacketsReceived,PacketsLost,"
      << "LossPercent\n";

  for (uint32_t frame = 0;
       frame < frames.size();
       ++frame)
  {
    uint32_t frameSent = 0;
    uint32_t frameReceived = 0;

    for (const auto &entry :
         VideoSender::GetPacketDatabase())
    {
      const PacketInfo &info =
          entry.second;

      if (info.frame != frame)
      {
        continue;
      }

      frameSent++;

      if (info.received)
      {
        frameReceived++;
      }
    }

    uint32_t frameLost =
        frameSent - frameReceived;

    double frameLoss =
        frameSent == 0
            ? 0.0
            : 100.0 *
                  static_cast<double>(
                      frameLost) /
                  static_cast<double>(
                      frameSent);

    frameCsv
        << (frame + 1) << ","
        << frames[frame].name << ","
        << frameSent << ","
        << frameReceived << ","
        << frameLost << ","
        << frameLoss
        << "\n";
  }

  frameCsv.close();

  std::cout << "\nCSV files generated:\n";
  std::cout << "  video-packet-metrics.csv\n";
  std::cout << "  video-frame-summary.csv\n";

  std::cout << "\n=============================================\n";
  std::cout << "SIMULATION COMPLETED\n";
  std::cout << "=============================================\n";

  return 0;
}
