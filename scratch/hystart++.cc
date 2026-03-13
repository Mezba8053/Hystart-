// /*
//  * Topology: Sender ---[10Mbps,5ms]--- Router ---[1Mbps,50ms]--- Receiver
//  */

// #include "ns3/applications-module.h"
// #include "ns3/core-module.h"
// #include "ns3/flow-monitor-helper.h"
// #include "ns3/internet-module.h"
// #include "ns3/ipv4-flow-classifier.h"
// #include "ns3/network-module.h"
// #include "ns3/point-to-point-module.h"
// #include "ns3/traffic-control-module.h"

// using namespace ns3;

// NS_LOG_COMPONENT_DEFINE("HyStartPlusPlusImpl");

// static double g_bdpBytes = 0;
// static uint32_t g_bdpOvershootCount = 0;
// static uint32_t g_queueDropCount = 0;

// class TcpHyStartPlusPlus : public TcpLinuxReno
// {
//   public:
//     enum HyStartPhase
//     {
//         HYSTART_SS,
//         HYSTART_CSS,
//         HYSTART_CA
//     };

//     static TypeId GetTypeId();

//     TcpHyStartPlusPlus();
//     TcpHyStartPlusPlus(const TcpHyStartPlusPlus& sock);
//     ~TcpHyStartPlusPlus() override;
//     std::string GetName() const override;
//     void IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked) override;
//     void PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt) override;
//     uint32_t GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight) override;
//     void CongestionStateSet(Ptr<TcpSocketState> tcb,
//                             const TcpSocketState::TcpCongState_t newState) override;
//     Ptr<TcpCongestionOps> Fork() override;

//   private:
//     Time minRttThresh;      ///< MIN_RTT_THRESH  = 4 ms
//     Time maxRttThresh;      ///< MAX_RTT_THRESH  = 16 ms
//     uint32_t minRttDivisor; ///< MIN_RTT_DIVISOR = 8
//     uint32_t nRttSample;    ///< N_RTT_SAMPLE    = 8
//     uint32_t cssGrowthDiv;  ///< CSS_GROWTH_DIVISOR = 4
//     uint32_t cssMaxRounds;  ///< CSS_ROUNDS       = 5
//     uint32_t m_L;

//     HyStartPhase phase;
//     bool roundStarted;
//     SequenceNumber32 windowEnd;

//     Time lastRoundMinRtt;
//     Time currentRoundMinRtt;
//     uint32_t rttSampleCount;
//     std::vector<Time> lastRtt;
//     Time m_cssBaselineMinRtt;
//     uint32_t cssRoundCount;
//     uint32_t m_cssCwndAccum;
//     uint32_t windowTracker;
//     bool m_isInitialSlowStart;

//     uint32_t ssExitCount;
//     uint32_t cssSpuriousCount;
//     uint32_t ssExitCwnd;
//     Time m_ssExitTime;

//     void BeginNewRttRound(Ptr<TcpSocketState> tcb);

//     /** RttThresh = max(MIN_RTT_THRESH, min(lastRoundMinRTT / MIN_RTT_DIVISOR, MAX_RTT_THRESH))
//     */ Time ComputeRttThresh() const;

//     static const char* PhaseName(HyStartPhase p);
// };

// NS_OBJECT_ENSURE_REGISTERED(TcpHyStartPlusPlus);

// TypeId
// TcpHyStartPlusPlus::GetTypeId()
// {
//     static TypeId tid =
//         TypeId("ns3::TcpHyStartPlusPlus")
//             .SetParent<TcpLinuxReno>()
//             .SetGroupName("Internet")
//             .AddConstructor<TcpHyStartPlusPlus>()
//             .AddAttribute(
//                 "MinRttThresh",
//                 "MIN_RTT_THRESH – lower bound on delay-increase sensitivity (RFC 9406 §4.3)",
//                 TimeValue(MilliSeconds(4)),
//                 MakeTimeAccessor(&TcpHyStartPlusPlus::minRttThresh),
//                 MakeTimeChecker())
//             .AddAttribute("MaxRttThresh",
//                           "MAX_RTT_THRESH – upper bound on delay-increase sensitivity",
//                           TimeValue(MilliSeconds(16)),
//                           MakeTimeAccessor(&TcpHyStartPlusPlus::maxRttThresh),
//                           MakeTimeChecker())
//             .AddAttribute("MinRttDivisor",
//                           "MIN_RTT_DIVISOR – fraction of RTT for delay threshold",
//                           UintegerValue(8),
//                           MakeUintegerAccessor(&TcpHyStartPlusPlus::minRttDivisor),
//                           MakeUintegerChecker<uint32_t>(1))
//             .AddAttribute("NRttSample",
//                           "N_RTT_SAMPLE – minimum RTT samples per round",
//                           UintegerValue(8),
//                           MakeUintegerAccessor(&TcpHyStartPlusPlus::nRttSample),
//                           MakeUintegerChecker<uint32_t>(1))
//             .AddAttribute("CssGrowthDivisor",
//                           "CSS_GROWTH_DIVISOR – CSS growth = SS_growth / this (min 2)",
//                           UintegerValue(4),
//                           MakeUintegerAccessor(&TcpHyStartPlusPlus::cssGrowthDiv),
//                           MakeUintegerChecker<uint32_t>(2))
//             .AddAttribute("CssMaxRounds",
//                           "CSS_ROUNDS – max rounds in CSS before entering CA",
//                           UintegerValue(5),
//                           MakeUintegerAccessor(&TcpHyStartPlusPlus::cssMaxRounds),
//                           MakeUintegerChecker<uint32_t>(1))
//             .AddAttribute("L",
//                           "Aggressiveness limit (segments). 8 for non-paced, very large for
//                           paced.", UintegerValue(8),
//                           MakeUintegerAccessor(&TcpHyStartPlusPlus::m_L),
//                           MakeUintegerChecker<uint32_t>(1));
//     return tid;
// }

// TcpHyStartPlusPlus::TcpHyStartPlusPlus()
//     : TcpLinuxReno(),
//       minRttThresh(MilliSeconds(4)),
//       maxRttThresh(MilliSeconds(16)),
//       minRttDivisor(8),
//       nRttSample(8),
//       cssGrowthDiv(4),
//       cssMaxRounds(5),
//       m_L(8),
//       phase(HYSTART_SS),
//       roundStarted(false),
//       windowTracker(3),
//       windowEnd(SequenceNumber32(0)),
//       lastRoundMinRtt(Time::Max()),
//       currentRoundMinRtt(Time::Max()),
//       rttSampleCount(0),
//       m_cssBaselineMinRtt(Time::Max()),
//       cssRoundCount(0),
//       m_cssCwndAccum(0),
//       m_isInitialSlowStart(true),
//       ssExitCount(0),
//       cssSpuriousCount(0),
//       ssExitCwnd(0),
//       m_ssExitTime(Seconds(0)),
//       lastRtt()
// {
//     NS_LOG_FUNCTION(this);
// }

// TcpHyStartPlusPlus::TcpHyStartPlusPlus(const TcpHyStartPlusPlus& sock)
//     : TcpLinuxReno(sock),
//       minRttThresh(sock.minRttThresh),
//       maxRttThresh(sock.maxRttThresh),
//       minRttDivisor(sock.minRttDivisor),
//       nRttSample(sock.nRttSample),
//       cssGrowthDiv(sock.cssGrowthDiv),
//       cssMaxRounds(sock.cssMaxRounds),
//       m_L(sock.m_L),
//       phase(sock.phase),
//       roundStarted(sock.roundStarted),
//       windowEnd(sock.windowEnd),
//       lastRoundMinRtt(sock.lastRoundMinRtt),
//       currentRoundMinRtt(sock.currentRoundMinRtt),
//       rttSampleCount(sock.rttSampleCount),
//       m_cssBaselineMinRtt(sock.m_cssBaselineMinRtt),
//       cssRoundCount(sock.cssRoundCount),
//       m_cssCwndAccum(sock.m_cssCwndAccum),
//       m_isInitialSlowStart(sock.m_isInitialSlowStart),
//       ssExitCount(sock.ssExitCount),
//       cssSpuriousCount(sock.cssSpuriousCount),
//       ssExitCwnd(sock.ssExitCwnd),
//       m_ssExitTime(sock.m_ssExitTime),
//       lastRtt(sock.lastRtt)
// {
//     NS_LOG_FUNCTION(this);
// }

// TcpHyStartPlusPlus::~TcpHyStartPlusPlus()
// {
// }

// const char*
// TcpHyStartPlusPlus::PhaseName(HyStartPhase p)
// {
//     switch (p)
//     {
//     case HYSTART_SS:
//         return "SS";
//     case HYSTART_CSS:
//         return "CSS";
//     case HYSTART_CA:
//         return "CA";
//     }
//     return "??";
// }

// std::string
// TcpHyStartPlusPlus::GetName() const
// {
//     return "TcpHyStartPlusPlus";
// }

// void
// TcpHyStartPlusPlus::BeginNewRttRound(Ptr<TcpSocketState> tcb)
// {
//     lastRoundMinRtt = currentRoundMinRtt;
//     currentRoundMinRtt = Time::Max();
//     rttSampleCount = 0;
//     windowEnd = tcb->m_highTxMark;

//     std::cout << "HYSTART_ROUND " << Simulator::Now().GetSeconds() << " phase=" <<
//     PhaseName(phase)
//               << " lastMinRTT="
//               << (lastRoundMinRtt == Time::Max() ? -1.0 : lastRoundMinRtt.GetMilliSeconds())
//               << "ms windowEnd=" << windowEnd << " cwnd=" << tcb->m_cWnd << std::endl;
// }

// Time
// TcpHyStartPlusPlus::ComputeRttThresh() const
// {
//     /*
//      *   RttThresh = max(MIN_RTT_THRESH,
//      *                   min(lastRoundMinRTT / MIN_RTT_DIVISOR, MAX_RTT_THRESH)) */
//     Time fraction = Time::FromDouble(lastRoundMinRtt.GetDouble() / (double)minRttDivisor,
//     Time::NS); return std::max(minRttThresh, std::min(fraction, maxRttThresh));
// }

// void
// TcpHyStartPlusPlus::PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt)
// {
//     NS_LOG_FUNCTION(this << tcb << segmentsAcked << rtt);

//     if (rtt.IsZero() || rtt.IsNegative())
//     {
//         return;
//     }

//     if (!m_isInitialSlowStart)
//     {
//         return;
//     }

//     if (phase == HYSTART_CA)
//     {
//         return;
//     }

//     if (!roundStarted)
//     {
//         windowEnd = tcb->m_highTxMark;
//         roundStarted = true;
//         std::cout << "HYSTART_INIT " << Simulator::Now().GetSeconds() << " windowEnd=" <<
//         windowEnd
//                   << " cwnd=" << tcb->m_cWnd << std::endl;
//     }
//     lastRtt.push_back(rtt);
//     if (lastRtt.size() > 3)
//     {
//         lastRtt.erase(lastRtt.begin());
//     }
//     Time weightedAvgRtt = Time::Max();
//     if (lastRtt.size() == 3)
//     {
//         weightedAvgRtt =
//             Time::FromDouble((lastRtt[0].GetDouble() * 0.2 + lastRtt[1].GetDouble() * 0.3 +
//                               lastRtt[2].GetDouble() * 0.5),
//                              Time::NS);
//         NS_LOG_INFO("[ACKed] RTT=" << rtt.GetMilliSeconds()
//                                    << "ms  weightedAvgRTT=" << weightedAvgRtt.GetMilliSeconds()
//                                    << "ms  samples=" << lastRtt.size());
//     }
//     else
//     {
//         currentRoundMinRtt = std::min(currentRoundMinRtt, rtt);
//         weightedAvgRtt = currentRoundMinRtt;
//     }
//     currentRoundMinRtt = weightedAvgRtt;
//     rttSampleCount++;
//     if (tcb->m_lastAckedSeq >= windowEnd)
//     {
//         if (phase == HYSTART_CSS)
//         {
//             cssRoundCount++;

//             std::cout << "HYSTART_CSS_ROUND " << Simulator::Now().GetSeconds()
//                       << " round=" << cssRoundCount << "/" << cssMaxRounds
//                       << " cwnd=" << tcb->m_cWnd << std::endl;

//             if (cssRoundCount >= cssMaxRounds)
//             {
//                 phase = HYSTART_CA;
//                 tcb->m_ssThresh = tcb->m_cWnd;

//                 std::cout << "HYSTART_PHASE " << Simulator::Now().GetSeconds()
//                           << " CSS->CA reason=css_rounds_complete"
//                           << " ssthresh=" << tcb->m_ssThresh << " cwnd=" << tcb->m_cWnd << " ("
//                           << tcb->m_cWnd / tcb->m_segmentSize << " segs)" << std::endl;
//                 return;
//             }
//         }

//         BeginNewRttRound(tcb);
//     }

//     currentRoundMinRtt = std::min(currentRoundMinRtt, rtt);
//     // rttSampleCount++;

//     if (phase == HYSTART_SS)
//     {
//         if (rttSampleCount >= nRttSample && currentRoundMinRtt != Time::Max() &&
//             lastRoundMinRtt != Time::Max())
//         {
//             Time rttThresh = ComputeRttThresh();
//             Time delayTarget = lastRoundMinRtt + rttThresh;

//             NS_LOG_INFO("[SS delay check] curMinRTT="
//                         << currentRoundMinRtt.GetMilliSeconds()
//                         << "ms  target=" << delayTarget.GetMilliSeconds()
//                         << "ms  (lastMinRTT=" << lastRoundMinRtt.GetMilliSeconds()
//                         << "ms + thresh=" << rttThresh.GetMilliSeconds() << "ms)"
//                         << "  samples=" << rttSampleCount);

//             if (currentRoundMinRtt >= delayTarget)
//             {
//                 m_cssBaselineMinRtt = currentRoundMinRtt;
//                 phase = HYSTART_CSS;
//                 cssRoundCount = 0;
//                 m_cssCwndAccum = 0;
//                 ssExitCount++;
//                 ssExitCwnd = tcb->m_cWnd;
//                 m_ssExitTime = Simulator::Now();

//                 std::cout << "HYSTART_PHASE " << Simulator::Now().GetSeconds()
//                           << " SS->CSS reason=delay_increase"
//                           << " curMinRTT=" << currentRoundMinRtt.GetMilliSeconds()
//                           << "ms >= target=" << delayTarget.GetMilliSeconds()
//                           << "ms (lastMinRTT=" << lastRoundMinRtt.GetMilliSeconds()
//                           << "ms + rttThresh=" << rttThresh.GetMilliSeconds() << "ms)"
//                           << " cwnd=" << tcb->m_cWnd << " (" << tcb->m_cWnd / tcb->m_segmentSize
//                           << " segs)"
//                           << " cssBaseline=" << m_cssBaselineMinRtt.GetMilliSeconds() << "ms"
//                           << std::endl;
//             }
//         }
//     }
//     else if (phase == HYSTART_CSS)
//     {
//         if (rttSampleCount >= nRttSample && currentRoundMinRtt < m_cssBaselineMinRtt)
//         {
//             m_cssBaselineMinRtt = Time::Max();
//             phase = HYSTART_SS;
//             cssSpuriousCount++;

//             std::cout << "HYSTART_PHASE " << Simulator::Now().GetSeconds()
//                       << " CSS->SS reason=spurious_exit"
//                       << " curMinRTT=" << currentRoundMinRtt.GetMilliSeconds() << "ms <
//                       cssBaseline"
//                       << " cwnd=" << tcb->m_cWnd << " (" << tcb->m_cWnd / tcb->m_segmentSize
//                       << " segs)" << std::endl;
//         }
//     }
// }

// void
// TcpHyStartPlusPlus::IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
// {
//     NS_LOG_FUNCTION(this << tcb << segmentsAcked);

//     uint32_t effectiveL = tcb->m_pacing ? UINT32_MAX : m_L;

//     if (tcb->m_cWnd < tcb->m_ssThresh)
//     {
//         if (phase == HYSTART_SS)
//         {
//             uint32_t limited = std::min(segmentsAcked, effectiveL);
//             uint32_t oldCwnd = tcb->m_cWnd;
//             tcb->m_cWnd =
//                 std::min(oldCwnd + limited * tcb->m_segmentSize, (uint32_t)tcb->m_ssThresh);

//             NS_LOG_INFO("[SS increase] cwnd " << oldCwnd << " -> " << tcb->m_cWnd << " (+"
//                                               << limited << " segs, L=" << effectiveL << ")");
//         }
//         else if (phase == HYSTART_CSS)
//         {
//             uint32_t limited = std::min(segmentsAcked, effectiveL);
//             uint32_t bytesToAdd = limited * tcb->m_segmentSize;

//             m_cssCwndAccum += bytesToAdd;
//             uint32_t increment = m_cssCwndAccum / cssGrowthDiv;

//             if (increment > 0)
//             {
//                 uint32_t oldCwnd = tcb->m_cWnd;
//                 tcb->m_cWnd += increment;
//                 m_cssCwndAccum -= increment * cssGrowthDiv;

//                 NS_LOG_INFO("[CSS increase] cwnd " << oldCwnd << " -> " << tcb->m_cWnd << " (+"
//                                                    << increment << " bytes, 1/" << cssGrowthDiv
//                                                    << " of SS rate)");
//             }
//         }
//         else
//         {
//             TcpLinuxReno::IncreaseWindow(tcb, segmentsAcked);
//         }
//     }
//     else
//     {
//         if (phase != HYSTART_CA)
//         {
//             std::cout << "HYSTART_PHASE " << Simulator::Now().GetSeconds() << " "
//                       << PhaseName(phase) << "->CA"
//                       << " reason=cwnd_reached_ssthresh"
//                       << " cwnd=" << tcb->m_cWnd << " ssthresh=" << tcb->m_ssThresh << std::endl;
//             phase = HYSTART_CA;
//         }
//         TcpLinuxReno::IncreaseWindow(tcb, segmentsAcked);
//     }
// }

// uint32_t
// TcpHyStartPlusPlus::GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
// {
//     NS_LOG_FUNCTION(this << tcb << bytesInFlight);

//     if (phase == HYSTART_SS || phase == HYSTART_CSS)
//     {
//         uint32_t ssThresh = std::max(tcb->m_cWnd.Get(), 2 * tcb->m_segmentSize);

//         std::cout << "HYSTART_LOSS " << Simulator::Now().GetSeconds()
//                   << " phase=" << PhaseName(phase) << " ssthresh=cwnd=" << ssThresh
//                   << " cwnd=" << tcb->m_cWnd << " bytesInFlight=" << bytesInFlight << std::endl;

//         phase = HYSTART_CA;
//         m_isInitialSlowStart = false;

//         return ssThresh;
//     }

//     uint32_t ssThresh = TcpLinuxReno::GetSsThresh(tcb, bytesInFlight);
//     std::cout << "HYSTART_LOSS " << Simulator::Now().GetSeconds()
//               << " phase=CA ssthresh=" << ssThresh << " cwnd=" << tcb->m_cWnd
//               << " bytesInFlight=" << bytesInFlight << std::endl;
//     return ssThresh;
// }

// void
// TcpHyStartPlusPlus::CongestionStateSet(Ptr<TcpSocketState> tcb,
//                                        const TcpSocketState::TcpCongState_t newState)
// {
//     NS_LOG_FUNCTION(this << tcb << newState);

//     if (newState == TcpSocketState::CA_LOSS)
//     {
//         std::cout << "HYSTART_STATE " << Simulator::Now().GetSeconds()
//                   << " CA_LOSS phase=" << PhaseName(phase) << "->CA"
//                   << " cwnd=" << tcb->m_cWnd << std::endl;

//         phase = HYSTART_CA;
//         m_isInitialSlowStart = false;
//     }
// }

// static void
// TraceCwnd(uint32_t oldVal, uint32_t newVal)
// {
//     std::cout << "CWND " << Simulator::Now().GetSeconds() << " " << oldVal << " " << newVal
//               << std::endl;

//     // Output to file for plotting
//     std::ofstream cwndFile("cwnd.dat", std::ios::app);
//     cwndFile << Simulator::Now().GetSeconds() << " " << newVal << std::endl;
//     cwndFile.close();

//     if (g_bdpBytes > 0 && (double)newVal > g_bdpBytes)
//     {
//         double overshootPct = ((double)newVal / g_bdpBytes - 1.0) * 100.0;
//         std::cout << "BDP_OVERSHOOT " << Simulator::Now().GetSeconds() << " " << newVal << " "
//                   << (uint32_t)g_bdpBytes << " " << overshootPct << std::endl;
//         g_bdpOvershootCount++;
//     }
// }

// static void
// TraceSsthresh(uint32_t oldVal, uint32_t newVal)
// {
//     std::cout << "SSTHRESH " << Simulator::Now().GetSeconds() << " " << oldVal << " " << newVal
//               << std::endl;
// }

// Ptr<TcpCongestionOps>
// TcpHyStartPlusPlus::Fork()
// {
//     return CopyObject<TcpHyStartPlusPlus>(this);
// }

// static void
// TraceRtt(Time oldVal, Time newVal)
// {
//     std::cout << "RTT " << Simulator::Now().GetSeconds() << " " << oldVal.GetMilliSeconds() << "
//     "
//               << newVal.GetMilliSeconds() << std::endl;

//     // Output to file for plotting
//     std::ofstream rttFile("rtt.dat", std::ios::app);
//     rttFile << Simulator::Now().GetSeconds() << " " << newVal.GetMilliSeconds() << std::endl;
//     rttFile.close();
// }

// static void
// TraceCongState(TcpSocketState::TcpCongState_t oldVal, TcpSocketState::TcpCongState_t newVal)
// {
//     std::cout << "CONGSTATE " << Simulator::Now().GetSeconds() << " " << oldVal << " " << newVal
//               << std::endl;
// }

// static void
// TraceBytesInFlight(uint32_t oldVal, uint32_t newVal)
// {
//     std::cout << "BIF " << Simulator::Now().GetSeconds() << " " << oldVal << " " << newVal
//               << std::endl;
// }

// static void
// TraceQueueDrop(Ptr<const QueueDiscItem> item)
// {
//     std::cout << "QUEUE_DROP " << Simulator::Now().GetSeconds() << " " << item->GetSize()
//               << std::endl;
//     g_queueDropCount++;
// }

// static void
// ConnectSocketTraces()
// {
//     Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
//                                   MakeCallback(&TraceCwnd));
//     Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/SlowStartThreshold",
//                                   MakeCallback(&TraceSsthresh));
//     Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/RTT",
//                                   MakeCallback(&TraceRtt));
//     Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongState",
//                                   MakeCallback(&TraceCongState));
//     Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/BytesInFlight",
//                                   MakeCallback(&TraceBytesInFlight));

//     std::cout << "TRACE_CONNECTED " << Simulator::Now().GetSeconds() << std::endl;
// }

// int
// main(int argc, char* argv[])
// {
//     std::string tcpVariant = "TcpHyStartPlusPlus"; // Default to HyStart++
//     std::string bandwidth = "1Mbps";
//     std::string delay = "50ms";
//     uint32_t dataSize = 10000000; // 10 MB
//     double simTime = 30.0;
//     uint32_t queueSize = 50;

//     CommandLine cmd(__FILE__);
//     cmd.AddValue("tcp", "TCP variant: TcpNewReno or TcpHyStartPlusPlus", tcpVariant);
//     cmd.AddValue("bandwidth", "Bottleneck bandwidth", bandwidth);
//     cmd.AddValue("delay", "Bottleneck delay (one-way)", delay);
//     cmd.AddValue("data", "Data size in bytes", dataSize);
//     cmd.AddValue("time", "Simulation time", simTime);
//     cmd.AddValue("queue", "Queue size in packets", queueSize);
//     // Reset global counters and clear data files
//     g_bdpOvershootCount = 0;
//     g_queueDropCount = 0;
//     std::ofstream cwndFile("cwnd.dat", std::ios::trunc);
//     cwndFile.close();
//     std::ofstream rttFile("rtt.dat", std::ios::trunc);
//     rttFile.close();
//     std::ofstream throughputFile("throughput.dat", std::ios::trunc);
//     throughputFile.close();

//     // ---- Compute BDP ----
//     DataRate bwRate(bandwidth);
//     Time delayTime(delay);
//     Time accessDelay("5ms");
//     Time rtt = 2 * (accessDelay + delayTime); // full RTT
//     g_bdpBytes = bwRate.GetBitRate() * rtt.GetSeconds() / 8.0;

//     std::cout << "===== TCP SIMULATION: " << tcpVariant << " =====" << std::endl;
//     std::cout << "CONFIG bottleneck=" << bandwidth << " delay=" << delay
//               << " RTT=" << rtt.GetMilliSeconds() << "ms"
//               << " BDP=" << (uint32_t)g_bdpBytes << "bytes"
//               << " BDP_segments=" << (uint32_t)(g_bdpBytes / 536) << " queue=" << queueSize
//               << "pkts"
//               << " data=" << dataSize << "bytes" << std::endl;
//     std::cout << "BDP_LINE " << g_bdpBytes << std::endl;

//     if (tcpVariant == "TcpHyStartPlusPlus")
//     {
//         std::cout << "CONSTANTS MIN_RTT_THRESH=4ms MAX_RTT_THRESH=16ms"
//                   << " MIN_RTT_DIVISOR=8 N_RTT_SAMPLE=8"
//                   << " CSS_GROWTH_DIVISOR=4 CSS_ROUNDS=5 L=8" << std::endl;
//         Config::SetDefault("ns3::TcpL4Protocol::SocketType",
//                            StringValue("ns3::TcpHyStartPlusPlus"));
//     }
//     else if (tcpVariant == "TcpNewReno")
//     {
//         std::cout << "Using standard TcpNewReno (Slow Start)" << std::endl;
//         Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
//     }
//     else
//     {
//         std::cerr << "Invalid TCP variant. Use TcpNewReno or TcpHyStartPlusPlus." << std::endl;
//         return 1;
//     }

//     Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(10));
//     Config::SetDefault("ns3::PfifoFastQueueDisc::MaxSize",
//                        QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSize)));

//     // ---- Create topology: Sender(0) --[10Mbps,5ms]-- Router(1) --[1Mbps,50ms]-- Receiver(2)
//     ---- NodeContainer nodes; nodes.Create(3);

//     PointToPointHelper p2pAccess;
//     p2pAccess.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
//     p2pAccess.SetChannelAttribute("Delay", StringValue("5ms"));

//     PointToPointHelper p2pBottleneck;
//     p2pBottleneck.SetDeviceAttribute("DataRate", StringValue(bandwidth));
//     p2pBottleneck.SetChannelAttribute("Delay", StringValue(delay));

//     NetDeviceContainer devices1 = p2pAccess.Install(nodes.Get(0), nodes.Get(1));
//     NetDeviceContainer devices2 = p2pBottleneck.Install(nodes.Get(1), nodes.Get(2));

//     InternetStackHelper stack;
//     stack.Install(nodes);

//     Ipv4AddressHelper address;
//     address.SetBase("10.1.1.0", "255.255.255.0");
//     Ipv4InterfaceContainer interfaces1 = address.Assign(devices1);
//     address.SetBase("10.1.2.0", "255.255.255.0");
//     Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

//     Ipv4GlobalRoutingHelper::PopulateRoutingTables();

//     // ---- Applications ----
//     Ipv4Address destAddr = interfaces2.GetAddress(1);
//     BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(destAddr, 9));
//     source.SetAttribute("MaxBytes", UintegerValue(dataSize));
//     ApplicationContainer sourceApps = source.Install(nodes.Get(0));
//     sourceApps.Start(Seconds(1.0));
//     sourceApps.Stop(Seconds(simTime));

//     PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
//     ApplicationContainer sinkApps = sink.Install(nodes.Get(2));
//     sinkApps.Start(Seconds(0.0));
//     sinkApps.Stop(Seconds(simTime));

//     TrafficControlHelper tch;
//     tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
//     tch.Uninstall(devices2.Get(0));
//     QueueDiscContainer qd = tch.Install(devices2.Get(0));
//     qd.Get(0)->TraceConnectWithoutContext("Drop", MakeCallback(&TraceQueueDrop));

//     Simulator::Schedule(Seconds(1.001), &ConnectSocketTraces);

//     FlowMonitorHelper flowmon;
//     Ptr<FlowMonitor> monitor = flowmon.InstallAll();

//     Simulator::Stop(Seconds(simTime));
//     Simulator::Run();

//     // ---- Print flow statistics ----
//     monitor->CheckForLostPackets();
//     Ptr<Ipv4FlowClassifier> classifier =
//     DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier()); auto stats =
//     monitor->GetFlowStats();

//     std::cout << "\n===== FLOW STATISTICS =====" << std::endl;
//     for (auto& entry : stats)
//     {
//         Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(entry.first);
//         double duration = entry.second.timeLastRxPacket.GetSeconds() -
//                           entry.second.timeFirstTxPacket.GetSeconds();
//         double throughput = (duration > 0) ? (entry.second.rxBytes * 8.0 / duration / 1e6) : 0;
//         double lossRate = (entry.second.txPackets > 0)
//                               ? ((double)entry.second.lostPackets / entry.second.txPackets *
//                               100.0) : 0;
//         double avgDelay = (entry.second.rxPackets > 0)
//                               ? (entry.second.delaySum.GetMilliSeconds() /
//                               entry.second.rxPackets) : 0;
//         double avgJitter =
//             (entry.second.rxPackets > 1)
//                 ? (entry.second.jitterSum.GetMilliSeconds() / (entry.second.rxPackets - 1))
//                 : 0;

//         std::cout << "FLOW " << entry.first << " " << t.sourceAddress << " -> "
//                   << t.destinationAddress << std::endl;
//         std::cout << "FLOW_STATS tx=" << entry.second.txPackets << " rx=" <<
//         entry.second.rxPackets
//                   << " lost=" << entry.second.lostPackets << " lossRate=" << lossRate << "%"
//                   << " throughput=" << throughput << "Mbps"
//                   << " avgDelay=" << avgDelay << "ms"
//                   << " avgJitter=" << avgJitter << "ms" << std::endl;

//         // Output throughput to file
//         std::ofstream throughputFile("throughput.dat", std::ios::app);
//         throughputFile << tcpVariant << " " << throughput << " " << lossRate << " "
//                        << g_bdpOvershootCount << " " << g_queueDropCount << std::endl;
//         throughputFile.close();
//     }

//     if (tcpVariant == "TcpHyStartPlusPlus")
//     {
//         std::cout << "\n===== HYSTART++ (RFC 9406) ALGORITHM SUMMARY =====" << std::endl;
//         std::cout << "BDP = " << (uint32_t)g_bdpBytes << " bytes (" << (uint32_t)(g_bdpBytes /
//         536)
//                   << " segments)" << std::endl;
//         std::cout << "Constants: MIN_RTT_THRESH=4ms  MAX_RTT_THRESH=16ms"
//                   << "  MIN_RTT_DIVISOR=8  N_RTT_SAMPLE=8"
//                   << "  CSS_GROWTH_DIVISOR=4  CSS_ROUNDS=5  L=8" << std::endl;
//         std::cout << "\nPhase diagram:" << std::endl;
//         std::cout << "  [SS] --delay increase--> [CSS] --5 rounds--> [CA]" << std::endl;
//         std::cout << "                            |  ^                    " << std::endl;
//         std::cout << "                            +--+  (spurious: back to SS)" << std::endl;
//         std::cout << "  [SS/CSS] --loss/ECN--> [CA]  (ssthresh = cwnd)" << std::endl;
//     }
//     else
//     {
//         std::cout << "\n===== STANDARD SLOW START (TcpNewReno) SUMMARY =====" << std::endl;
//         std::cout << "BDP = " << (uint32_t)g_bdpBytes << " bytes (" << (uint32_t)(g_bdpBytes /
//         536)
//                   << " segments)" << std::endl;
//         std::cout << "Standard Slow Start: cwnd doubles per RTT until loss or ssthresh."
//                   << std::endl;
//     }

//     Simulator::Destroy();
//     return 0;
// }

/*
 * HyStart++ (RFC 9406) — Fixed Implementation for NS-3
 * Topology: Sender ---[10Mbps,5ms]--- Router ---[1Mbps,50ms]--- Receiver
 *
 * Fixes over previous version:
 *  - rttSampleCount was incremented TWICE per ACK (before and after round check)
 *  - currentRoundMinRtt was set twice per ACK (weighted avg then overridden by raw min)
 *  - Round boundary was checked BEFORE RTT update, causing off-by-one round tracking
 *  - windowTracker declared but never used (removed)
 *
 * Design extension (beyond RFC 9406):
 *  - currentRoundMinRtt is fed by a 3-sample sliding weighted-average smoother
 *    instead of the raw RTT minimum.  Weights: oldest 0.2, middle 0.3, newest 0.5.
 *    This reduces false SS→CSS triggers from transient RTT spikes (e.g. ACK bunching)
 *    while still reacting promptly to sustained queue growth.
 *    The round is re-warmed (window cleared) at each BeginNewRttRound() call.
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("HyStartPlusPlusImpl");

// ---- Global stats (for summary output) ----
static double g_bdpBytes = 0;
static uint32_t g_bdpOvershootCount = 0;
static uint32_t g_queueDropCount = 0;

// ---- Output file streams (opened once, appended throughout) ----
static std::ofstream g_cwndFile;
static std::ofstream g_rttFile;
static std::ofstream g_ssthreshFile;
static std::ofstream g_dropsFile;
static std::ofstream g_phaseFile;

// ============================================================================
//  TcpHyStartPlusPlus
// ============================================================================
class TcpHyStartPlusPlus : public TcpLinuxReno
{
  public:
    enum HyStartPhase
    {
        HYSTART_SS,
        HYSTART_CSS,
        HYSTART_CA
    };

    static TypeId GetTypeId();

    TcpHyStartPlusPlus();
    TcpHyStartPlusPlus(const TcpHyStartPlusPlus& sock);
    ~TcpHyStartPlusPlus() override;

    std::string GetName() const override;
    void IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked) override;
    void PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt) override;
    uint32_t GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight) override;
    void CongestionStateSet(Ptr<TcpSocketState> tcb,
                            const TcpSocketState::TcpCongState_t newState) override;
    Ptr<TcpCongestionOps> Fork() override;

  private:
    // RFC 9406 §4.3 constants
    Time minRttThresh;      ///< MIN_RTT_THRESH   = 4 ms
    Time maxRttThresh;      ///< MAX_RTT_THRESH   = 16 ms
    uint32_t minRttDivisor; ///< MIN_RTT_DIVISOR  = 8
    uint32_t nRttSample;    ///< N_RTT_SAMPLE     = 8
    uint32_t cssGrowthDiv;  ///< CSS_GROWTH_DIVISOR = 4
    uint32_t cssMaxRounds;  ///< CSS_ROUNDS       = 5
    uint32_t m_L;           ///< L = 8 (non-paced)

    // Algorithm state
    HyStartPhase phase;
    bool roundStarted;
    SequenceNumber32 windowEnd;

    // RTT tracking (RFC 9406 §4.2)
    Time lastRoundMinRtt;
    Time currentRoundMinRtt;
    uint32_t rttSampleCount;

    // Windowed weighted-average smoother (extension beyond RFC 9406)
    // Keeps the last RTT_WINDOW_SIZE raw samples; the weighted average
    // of those is used as the "smoothed RTT" fed into currentRoundMinRtt.
    // Weights (oldest→newest): 0.2, 0.3, 0.5  (sum = 1.0)
    // If fewer than RTT_WINDOW_SIZE samples have been seen this round,
    // the raw sample is used directly so the algorithm still starts fast.
    static constexpr uint32_t RTT_WINDOW_SIZE = 3;
    std::vector<Time> m_rttWindow; ///< sliding window of last RTT_WINDOW_SIZE samples

    // CSS state
    Time m_cssBaselineMinRtt;
    uint32_t cssRoundCount;
    uint32_t m_cssCwndAccum;

    bool m_isInitialSlowStart;

    // Statistics
    uint32_t ssExitCount;
    uint32_t cssSpuriousCount;
    uint32_t ssExitCwnd;
    Time m_ssExitTime;

    void BeginNewRttRound(Ptr<TcpSocketState> tcb);
    Time ComputeRttThresh() const;
    static const char* PhaseName(HyStartPhase p);

    void LogPhaseTransition(double t,
                            const char* from,
                            const char* to,
                            const char* reason,
                            uint32_t cwnd,
                            uint32_t segSz);
};

NS_OBJECT_ENSURE_REGISTERED(TcpHyStartPlusPlus);

// ============================================================================
//  TypeId
// ============================================================================
TypeId
TcpHyStartPlusPlus::GetTypeId()
{
    static TypeId tid = TypeId("ns3::TcpHyStartPlusPlus")
                            .SetParent<TcpLinuxReno>()
                            .SetGroupName("Internet")
                            .AddConstructor<TcpHyStartPlusPlus>()
                            .AddAttribute("MinRttThresh",
                                          "MIN_RTT_THRESH (RFC 9406 §4.3)",
                                          TimeValue(MilliSeconds(4)),
                                          MakeTimeAccessor(&TcpHyStartPlusPlus::minRttThresh),
                                          MakeTimeChecker())
                            .AddAttribute("MaxRttThresh",
                                          "MAX_RTT_THRESH",
                                          TimeValue(MilliSeconds(16)),
                                          MakeTimeAccessor(&TcpHyStartPlusPlus::maxRttThresh),
                                          MakeTimeChecker())
                            .AddAttribute("MinRttDivisor",
                                          "MIN_RTT_DIVISOR",
                                          UintegerValue(8),
                                          MakeUintegerAccessor(&TcpHyStartPlusPlus::minRttDivisor),
                                          MakeUintegerChecker<uint32_t>(1))
                            .AddAttribute("NRttSample",
                                          "N_RTT_SAMPLE – minimum samples per round",
                                          UintegerValue(8),
                                          MakeUintegerAccessor(&TcpHyStartPlusPlus::nRttSample),
                                          MakeUintegerChecker<uint32_t>(1))
                            .AddAttribute("CssGrowthDivisor",
                                          "CSS_GROWTH_DIVISOR (min 2)",
                                          UintegerValue(4),
                                          MakeUintegerAccessor(&TcpHyStartPlusPlus::cssGrowthDiv),
                                          MakeUintegerChecker<uint32_t>(2))
                            .AddAttribute("CssMaxRounds",
                                          "CSS_ROUNDS",
                                          UintegerValue(5),
                                          MakeUintegerAccessor(&TcpHyStartPlusPlus::cssMaxRounds),
                                          MakeUintegerChecker<uint32_t>(1))
                            .AddAttribute("L",
                                          "Aggressiveness limit (8 non-paced, ∞ paced)",
                                          UintegerValue(8),
                                          MakeUintegerAccessor(&TcpHyStartPlusPlus::m_L),
                                          MakeUintegerChecker<uint32_t>(1));
    return tid;
}

// ============================================================================
//  Constructors
// ============================================================================
TcpHyStartPlusPlus::TcpHyStartPlusPlus()
    : TcpLinuxReno(),
      minRttThresh(MilliSeconds(4)),
      maxRttThresh(MilliSeconds(16)),
      minRttDivisor(8),
      nRttSample(8),
      cssGrowthDiv(4),
      cssMaxRounds(5),
      m_L(8),
      phase(HYSTART_SS),
      roundStarted(false),
      windowEnd(SequenceNumber32(0)),
      lastRoundMinRtt(Time::Max()),
      currentRoundMinRtt(Time::Max()),
      rttSampleCount(0),
      m_rttWindow(),
      m_cssBaselineMinRtt(Time::Max()),
      cssRoundCount(0),
      m_cssCwndAccum(0),
      m_isInitialSlowStart(true),
      ssExitCount(0),
      cssSpuriousCount(0),
      ssExitCwnd(0),
      m_ssExitTime(Seconds(0))
{
}

TcpHyStartPlusPlus::TcpHyStartPlusPlus(const TcpHyStartPlusPlus& sock)
    : TcpLinuxReno(sock),
      minRttThresh(sock.minRttThresh),
      maxRttThresh(sock.maxRttThresh),
      minRttDivisor(sock.minRttDivisor),
      nRttSample(sock.nRttSample),
      cssGrowthDiv(sock.cssGrowthDiv),
      cssMaxRounds(sock.cssMaxRounds),
      m_L(sock.m_L),
      phase(sock.phase),
      roundStarted(sock.roundStarted),
      windowEnd(sock.windowEnd),
      lastRoundMinRtt(sock.lastRoundMinRtt),
      currentRoundMinRtt(sock.currentRoundMinRtt),
      rttSampleCount(sock.rttSampleCount),
      m_rttWindow(sock.m_rttWindow),
      m_cssBaselineMinRtt(sock.m_cssBaselineMinRtt),
      cssRoundCount(sock.cssRoundCount),
      m_cssCwndAccum(sock.m_cssCwndAccum),
      m_isInitialSlowStart(sock.m_isInitialSlowStart),
      ssExitCount(sock.ssExitCount),
      cssSpuriousCount(sock.cssSpuriousCount),
      ssExitCwnd(sock.ssExitCwnd),
      m_ssExitTime(sock.m_ssExitTime)
{
}

TcpHyStartPlusPlus::~TcpHyStartPlusPlus()
{
}

// ============================================================================
//  Helpers
// ============================================================================
const char*
TcpHyStartPlusPlus::PhaseName(HyStartPhase p)
{
    switch (p)
    {
    case HYSTART_SS:
        return "SS";
    case HYSTART_CSS:
        return "CSS";
    case HYSTART_CA:
        return "CA";
    }
    return "??";
}

std::string
TcpHyStartPlusPlus::GetName() const
{
    return "TcpHyStartPlusPlus";
}

void
TcpHyStartPlusPlus::LogPhaseTransition(double t,
                                       const char* from,
                                       const char* to,
                                       const char* reason,
                                       uint32_t cwnd,
                                       uint32_t segSz)
{
    // stdout (parsed by Python)
    std::cout << "HYSTART_PHASE " << t << " " << from << "->" << to << " reason=" << reason
              << " cwnd=" << cwnd << " segs=" << cwnd / segSz << std::endl;
    // dedicated file: time  from  to  reason  cwnd_bytes
    if (g_phaseFile.is_open())
    {
        g_phaseFile << t << " " << from << " " << to << " " << reason << " " << cwnd << "\n";
    }
}

// ============================================================================
//  BeginNewRttRound – RFC 9406 §4.2 round management
// ============================================================================
void
TcpHyStartPlusPlus::BeginNewRttRound(Ptr<TcpSocketState> tcb)
{
    lastRoundMinRtt = currentRoundMinRtt; // promote current → last
    currentRoundMinRtt = Time::Max();     // reset for new round
    rttSampleCount = 0;
    m_rttWindow.clear();           // re-warm smoother each round
    windowEnd = tcb->m_highTxMark; // SND.NXT in NS-3

    std::cout << "HYSTART_ROUND " << Simulator::Now().GetSeconds() << " phase=" << PhaseName(phase)
              << " lastMinRTT="
              << (lastRoundMinRtt == Time::Max() ? -1.0 : lastRoundMinRtt.GetMilliSeconds())
              << "ms windowEnd=" << windowEnd << " cwnd=" << tcb->m_cWnd << std::endl;
}

// ============================================================================
//  ComputeRttThresh – RFC 9406 §4.2
// ============================================================================
Time
TcpHyStartPlusPlus::ComputeRttThresh() const
{
    Time fraction = Time::FromDouble(lastRoundMinRtt.GetDouble() / (double)minRttDivisor, Time::NS);
    return std::max(minRttThresh, std::min(fraction, maxRttThresh));
}

// ============================================================================
//  PktsAcked – RTT monitoring and phase transitions  (RFC 9406 §4.2)
//
//  FIXED ordering (compared to previous version):
//    1. Guard conditions
//    2. Init round tracking on first ACK
//    3. Update currentRoundMinRtt and rttSampleCount  ← ONCE only, here
//    4. Check round boundary (uses the just-updated values)
//    5. Phase-specific delay checks
// ============================================================================
void
TcpHyStartPlusPlus::PktsAcked(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked, const Time& rtt)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked << rtt);

    // 1 ── Guard conditions
    if (rtt.IsZero() || rtt.IsNegative())
    {
        return;
    }
    if (!m_isInitialSlowStart)
    {
        return;
    }
    if (phase == HYSTART_CA)
    {
        return;
    }

    // 2 ── Initialise round tracking on the very first ACK
    if (!roundStarted)
    {
        windowEnd = tcb->m_highTxMark;
        roundStarted = true;
        std::cout << "HYSTART_INIT " << Simulator::Now().GetSeconds() << " windowEnd=" << windowEnd
                  << " cwnd=" << tcb->m_cWnd << std::endl;
    }

    // 3 ── Compute smoothed RTT via sliding weighted-average window, then
    //      update currentRoundMinRtt with that smoothed value.
    //      (EXACTLY ONE update to both currentRoundMinRtt and rttSampleCount)
    //
    //  Window layout (oldest → newest):  [0]×0.2  [1]×0.3  [2]×0.5
    //  Rationale: recent samples carry more weight, dampening transient
    //  spikes without fully ignoring them the way a pure minimum would.
    //  The minimum over the round is still tracked so that one "clean"
    //  low-RTT sample in a noisy round doesn't hide real queue growth.

    // Slide the window: append new sample, drop oldest if full
    m_rttWindow.push_back(rtt);
    if (m_rttWindow.size() > RTT_WINDOW_SIZE)
    {
        m_rttWindow.erase(m_rttWindow.begin());
    }

    Time smoothedRtt;
    if (m_rttWindow.size() == RTT_WINDOW_SIZE)
    {
        // Full window — weighted average (0.2 : 0.3 : 0.5)
        smoothedRtt =
            Time::FromDouble(m_rttWindow[0].GetDouble() * 0.2 + m_rttWindow[1].GetDouble() * 0.3 +
                                 m_rttWindow[2].GetDouble() * 0.5,
                             Time::NS);
        NS_LOG_INFO("[ACKed-WA] raw=" << rtt.GetMilliSeconds()
                                      << "ms  smoothed=" << smoothedRtt.GetMilliSeconds()
                                      << "ms  window=[" << m_rttWindow[0].GetMilliSeconds() << ", "
                                      << m_rttWindow[1].GetMilliSeconds() << ", "
                                      << m_rttWindow[2].GetMilliSeconds() << "]ms");
    }
    else
    {
        // Window not yet full — use raw sample so we don't stall
        smoothedRtt = rtt;
        NS_LOG_INFO("[ACKed-WA] raw=" << rtt.GetMilliSeconds() << "ms  (warm-up, window="
                                      << m_rttWindow.size() << "/" << RTT_WINDOW_SIZE << ")");
    }

    // Track the minimum smoothed RTT seen this round
    currentRoundMinRtt = std::min(currentRoundMinRtt, smoothedRtt);
    rttSampleCount++;

    // 4 ── Round-boundary check: when windowEnd is ACKed, start a new round
    if (tcb->m_lastAckedSeq >= windowEnd)
    {
        if (phase == HYSTART_CSS)
        {
            cssRoundCount++;
            std::cout << "HYSTART_CSS_ROUND " << Simulator::Now().GetSeconds()
                      << " round=" << cssRoundCount << "/" << cssMaxRounds
                      << " cwnd=" << tcb->m_cWnd << std::endl;

            // CSS → CA: enough CSS rounds completed
            if (cssRoundCount >= cssMaxRounds)
            {
                phase = HYSTART_CA;
                tcb->m_ssThresh = tcb->m_cWnd;
                LogPhaseTransition(Simulator::Now().GetSeconds(),
                                   "CSS",
                                   "CA",
                                   "css_rounds_complete",
                                   tcb->m_cWnd,
                                   tcb->m_segmentSize);
                return;
            }
        }
        BeginNewRttRound(tcb); // promotes currentRoundMinRtt → lastRoundMinRtt
    }

    // 5 ── Phase-specific delay checks
    if (phase == HYSTART_SS)
    {
        // RFC 9406 §4.2: need N_RTT_SAMPLE samples and both rounds measured
        if (rttSampleCount >= nRttSample && currentRoundMinRtt != Time::Max() &&
            lastRoundMinRtt != Time::Max())
        {
            Time rttThresh = ComputeRttThresh();
            Time delayTarget = lastRoundMinRtt + rttThresh;

            NS_LOG_INFO("[SS delay check] curMinRTT=" << currentRoundMinRtt.GetMilliSeconds()
                                                      << "ms  target="
                                                      << delayTarget.GetMilliSeconds() << "ms"
                                                      << "  samples=" << rttSampleCount);

            if (currentRoundMinRtt >= delayTarget)
            {
                // *** SS → CSS ***
                m_cssBaselineMinRtt = currentRoundMinRtt;
                phase = HYSTART_CSS;
                cssRoundCount = 0;
                m_cssCwndAccum = 0;
                ssExitCount++;
                ssExitCwnd = tcb->m_cWnd;
                m_ssExitTime = Simulator::Now();

                LogPhaseTransition(Simulator::Now().GetSeconds(),
                                   "SS",
                                   "CSS",
                                   "delay_increase",
                                   tcb->m_cWnd,
                                   tcb->m_segmentSize);

                std::cout << "HYSTART_PHASE_DETAIL"
                          << " curMinRTT=" << currentRoundMinRtt.GetMilliSeconds() << "ms"
                          << " >= target=" << delayTarget.GetMilliSeconds() << "ms"
                          << " lastMinRTT=" << lastRoundMinRtt.GetMilliSeconds() << "ms"
                          << " rttThresh=" << rttThresh.GetMilliSeconds() << "ms"
                          << " cssBaseline=" << m_cssBaselineMinRtt.GetMilliSeconds() << "ms"
                          << std::endl;
            }
        }
    }
    else if (phase == HYSTART_CSS)
    {
        if (rttSampleCount >= nRttSample && currentRoundMinRtt < m_cssBaselineMinRtt)
        {
            // *** CSS → SS (spurious) ***
            m_cssBaselineMinRtt = Time::Max();
            phase = HYSTART_SS;
            cssSpuriousCount++;
            LogPhaseTransition(Simulator::Now().GetSeconds(),
                               "CSS",
                               "SS",
                               "spurious_exit",
                               tcb->m_cWnd,
                               tcb->m_segmentSize);
        }
    }
}

void
TcpHyStartPlusPlus::IncreaseWindow(Ptr<TcpSocketState> tcb, uint32_t segmentsAcked)
{
    NS_LOG_FUNCTION(this << tcb << segmentsAcked);

    uint32_t effectiveL = tcb->m_pacing ? UINT32_MAX : m_L;

    if (tcb->m_cWnd < tcb->m_ssThresh)
    {
        if (phase == HYSTART_SS)
        {
            uint32_t limited = std::min(segmentsAcked, effectiveL);
            uint32_t oldCwnd = tcb->m_cWnd;
            tcb->m_cWnd =
                std::min(oldCwnd + limited * tcb->m_segmentSize, (uint32_t)tcb->m_ssThresh);
            NS_LOG_INFO("[SS increase] cwnd " << oldCwnd << " -> " << tcb->m_cWnd);
        }
        else if (phase == HYSTART_CSS)
        {
            uint32_t limited = std::min(segmentsAcked, effectiveL);
            uint32_t bytesToAdd = limited * tcb->m_segmentSize;
            m_cssCwndAccum += bytesToAdd;
            uint32_t increment = m_cssCwndAccum / cssGrowthDiv;
            if (increment > 0)
            {
                tcb->m_cWnd += increment;
                m_cssCwndAccum -= increment * cssGrowthDiv;
                NS_LOG_INFO("[CSS increase] cwnd -> " << tcb->m_cWnd << " (+" << increment
                                                      << " bytes, 1/" << cssGrowthDiv << " SS)");
            }
        }
        else
        {
            TcpLinuxReno::IncreaseWindow(tcb, segmentsAcked);
        }
    }
    else
    {
        if (phase != HYSTART_CA)
        {
            LogPhaseTransition(Simulator::Now().GetSeconds(),
                               PhaseName(phase),
                               "CA",
                               "cwnd_reached_ssthresh",
                               tcb->m_cWnd,
                               tcb->m_segmentSize);
            phase = HYSTART_CA;
        }
        TcpLinuxReno::IncreaseWindow(tcb, segmentsAcked);
    }
}

uint32_t
TcpHyStartPlusPlus::GetSsThresh(Ptr<const TcpSocketState> tcb, uint32_t bytesInFlight)
{
    NS_LOG_FUNCTION(this << tcb << bytesInFlight);

    if (phase == HYSTART_SS || phase == HYSTART_CSS)
    {
        uint32_t ssThresh = std::max(tcb->m_cWnd.Get(), 2 * tcb->m_segmentSize);

        std::cout << "HYSTART_LOSS " << Simulator::Now().GetSeconds()
                  << " phase=" << PhaseName(phase) << " ssthresh=cwnd=" << ssThresh
                  << " cwnd=" << tcb->m_cWnd << " bytesInFlight=" << bytesInFlight << std::endl;

        phase = HYSTART_CA;
        m_isInitialSlowStart = false; // disable HyStart++ for future slow starts
        return ssThresh;
    }

    uint32_t ssThresh = TcpLinuxReno::GetSsThresh(tcb, bytesInFlight);
    std::cout << "HYSTART_LOSS " << Simulator::Now().GetSeconds()
              << " phase=CA ssthresh=" << ssThresh << " cwnd=" << tcb->m_cWnd
              << " bytesInFlight=" << bytesInFlight << std::endl;
    return ssThresh;
}

void
TcpHyStartPlusPlus::CongestionStateSet(Ptr<TcpSocketState> tcb,
                                       const TcpSocketState::TcpCongState_t newState)
{
    NS_LOG_FUNCTION(this << tcb << newState);
    if (newState == TcpSocketState::CA_LOSS)
    {
        std::cout << "HYSTART_STATE " << Simulator::Now().GetSeconds()
                  << " CA_LOSS phase=" << PhaseName(phase) << "->CA"
                  << " cwnd=" << tcb->m_cWnd << std::endl;
        phase = HYSTART_CA;
        m_isInitialSlowStart = false;
    }
}

Ptr<TcpCongestionOps>
TcpHyStartPlusPlus::Fork()
{
    return CopyObject<TcpHyStartPlusPlus>(this);
}

static void
TraceCwnd(uint32_t oldVal, uint32_t newVal)
{
    double t = Simulator::Now().GetSeconds();
    std::cout << "CWND " << t << " " << oldVal << " " << newVal << std::endl;
    if (g_cwndFile.is_open())
    {
        g_cwndFile << t << " " << newVal << "\n";
    }

    if (g_bdpBytes > 0 && (double)newVal > g_bdpBytes)
    {
        double pct = ((double)newVal / g_bdpBytes - 1.0) * 100.0;
        std::cout << "BDP_OVERSHOOT " << t << " " << newVal << " " << (uint32_t)g_bdpBytes << " "
                  << pct << std::endl;
        g_bdpOvershootCount++;
    }
}

static void
TraceSsthresh(uint32_t oldVal, uint32_t newVal)
{
    double t = Simulator::Now().GetSeconds();
    std::cout << "SSTHRESH " << t << " " << oldVal << " " << newVal << std::endl;
    // reuse ssthresh column in cwnd file: write to separate file
    static std::ofstream ssFile("ssthresh.dat", std::ios::app);
    if (ssFile.is_open())
    {
        ssFile << t << " " << newVal << "\n";
    }
}

static void
TraceRtt(Time oldVal, Time newVal)
{
    double t = Simulator::Now().GetSeconds();
    std::cout << "RTT " << t << " " << oldVal.GetMilliSeconds() << " " << newVal.GetMilliSeconds()
              << std::endl;
    if (g_rttFile.is_open())
    {
        g_rttFile << t << " " << newVal.GetMilliSeconds() << "\n";
    }
}

static void
TraceCongState(TcpSocketState::TcpCongState_t oldVal, TcpSocketState::TcpCongState_t newVal)
{
    std::cout << "CONGSTATE " << Simulator::Now().GetSeconds() << " " << oldVal << " " << newVal
              << std::endl;
}

static void
TraceBytesInFlight(uint32_t oldVal, uint32_t newVal)
{
    std::cout << "BIF " << Simulator::Now().GetSeconds() << " " << oldVal << " " << newVal
              << std::endl;
}

static void
TraceQueueDrop(Ptr<const QueueDiscItem> item)
{
    double t = Simulator::Now().GetSeconds();
    std::cout << "QUEUE_DROP " << t << " " << item->GetSize() << std::endl;
    if (g_dropsFile.is_open())
    {
        g_dropsFile << t << " " << item->GetSize() << "\n";
    }
    g_queueDropCount++;
}

static void
ConnectSocketTraces()
{
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow",
                                  MakeCallback(&TraceCwnd));
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/SlowStartThreshold",
                                  MakeCallback(&TraceSsthresh));
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/RTT",
                                  MakeCallback(&TraceRtt));
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongState",
                                  MakeCallback(&TraceCongState));
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/BytesInFlight",
                                  MakeCallback(&TraceBytesInFlight));
    std::cout << "TRACE_CONNECTED " << Simulator::Now().GetSeconds() << std::endl;
}

int
main(int argc, char* argv[])
{
    std::string tcpVariant = "TcpHyStartPlusPlus";
    std::string bandwidth = "1Mbps";
    std::string delay = "50ms";
    uint32_t dataSize = 10000000; // 10 MB
    double simTime = 30.0;
    uint32_t queueSize = 50;

    CommandLine cmd(__FILE__);
    cmd.AddValue("tcp", "TCP variant: TcpNewReno or TcpHyStartPlusPlus", tcpVariant);
    cmd.AddValue("bandwidth", "Bottleneck bandwidth", bandwidth);
    cmd.AddValue("delay", "Bottleneck delay (one-way)", delay);
    cmd.AddValue("data", "Data size in bytes", dataSize);
    cmd.AddValue("time", "Simulation time (s)", simTime);
    cmd.AddValue("queue", "Queue size (packets)", queueSize);
    cmd.Parse(argc, argv);

    g_bdpOvershootCount = 0;
    g_queueDropCount = 0;

    std::string tag = (tcpVariant == "TcpHyStartPlusPlus") ? "hystart" : "newreno";
    g_cwndFile.open("cwnd_" + tag + ".dat", std::ios::trunc);
    g_rttFile.open("rtt_" + tag + ".dat", std::ios::trunc);
    g_dropsFile.open("drops_" + tag + ".dat", std::ios::trunc);
    g_phaseFile.open("phases_" + tag + ".dat", std::ios::trunc);
    {
        std::ofstream ss("ssthresh_" + tag + ".dat", std::ios::trunc);
        ss.close();
    }

    DataRate bwRate(bandwidth);
    Time delayTime(delay);
    Time accessDelay("5ms");
    Time fullRtt = 2 * (accessDelay + delayTime);
    g_bdpBytes = bwRate.GetBitRate() * fullRtt.GetSeconds() / 8.0;

    std::cout << "===== TCP SIMULATION: " << tcpVariant << " =====" << std::endl;
    std::cout << "CONFIG bottleneck=" << bandwidth << " delay=" << delay
              << " RTT=" << fullRtt.GetMilliSeconds() << "ms"
              << " BDP=" << (uint32_t)g_bdpBytes << "bytes"
              << " BDP_segments=" << (uint32_t)(g_bdpBytes / 536) << " queue=" << queueSize
              << "pkts data=" << dataSize << "bytes" << std::endl;
    std::cout << "BDP_LINE " << g_bdpBytes << std::endl;

    if (tcpVariant == "TcpHyStartPlusPlus")
    {
        std::cout << "CONSTANTS MIN_RTT_THRESH=4ms MAX_RTT_THRESH=16ms"
                  << " MIN_RTT_DIVISOR=8 N_RTT_SAMPLE=8"
                  << " CSS_GROWTH_DIVISOR=4 CSS_ROUNDS=5 L=8" << std::endl;
        Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                           StringValue("ns3::TcpHyStartPlusPlus"));
    }
    else if (tcpVariant == "TcpNewReno")
    {
        std::cout << "Using standard TcpNewReno (Slow Start)" << std::endl;
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpNewReno"));
    }
    else
    {
        std::cerr << "Invalid TCP variant. Use TcpNewReno or TcpHyStartPlusPlus." << std::endl;
        return 1;
    }

    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(10));
    Config::SetDefault("ns3::PfifoFastQueueDisc::MaxSize",
                       QueueSizeValue(QueueSize(QueueSizeUnit::PACKETS, queueSize)));

    // ---- Topology: Sender(0) --[10Mbps,5ms]-- Router(1) --[1Mbps,50ms]-- Receiver(2) ----
    NodeContainer nodes;
    nodes.Create(3);

    PointToPointHelper p2pAccess;
    p2pAccess.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2pAccess.SetChannelAttribute("Delay", StringValue("5ms"));

    PointToPointHelper p2pBN;
    p2pBN.SetDeviceAttribute("DataRate", StringValue(bandwidth));
    p2pBN.SetChannelAttribute("Delay", StringValue(delay));

    NetDeviceContainer dev1 = p2pAccess.Install(nodes.Get(0), nodes.Get(1));
    NetDeviceContainer dev2 = p2pBN.Install(nodes.Get(1), nodes.Get(2));

    InternetStackHelper stack;
    stack.Install(nodes);

    Ipv4AddressHelper addr;
    addr.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer iface1 = addr.Assign(dev1);
    addr.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer iface2 = addr.Assign(dev2);
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ipv4Address dest = iface2.GetAddress(1);
    BulkSendHelper source("ns3::TcpSocketFactory", InetSocketAddress(dest, 9));
    source.SetAttribute("MaxBytes", UintegerValue(dataSize));
    ApplicationContainer srcApps = source.Install(nodes.Get(0));
    srcApps.Start(Seconds(1.0));
    srcApps.Stop(Seconds(simTime));

    PacketSinkHelper sink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
    ApplicationContainer sinkApps = sink.Install(nodes.Get(2));
    sinkApps.Start(Seconds(0.0));
    sinkApps.Stop(Seconds(simTime));

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::PfifoFastQueueDisc");
    tch.Uninstall(dev2.Get(0));
    QueueDiscContainer qd = tch.Install(dev2.Get(0));
    qd.Get(0)->TraceConnectWithoutContext("Drop", MakeCallback(&TraceQueueDrop));

    Simulator::Schedule(Seconds(1.001), &ConnectSocketTraces);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    auto flowStats = monitor->GetFlowStats();

    std::cout << "\n===== FLOW STATISTICS =====" << std::endl;
    for (auto& entry : flowStats)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(entry.first);
        double dur = entry.second.timeLastRxPacket.GetSeconds() -
                     entry.second.timeFirstTxPacket.GetSeconds();
        double throughput = (dur > 0) ? (entry.second.rxBytes * 8.0 / dur / 1e6) : 0;
        double lossRate = (entry.second.txPackets > 0)
                              ? ((double)entry.second.lostPackets / entry.second.txPackets * 100.0)
                              : 0;
        double avgDelay = (entry.second.rxPackets > 0)
                              ? (entry.second.delaySum.GetMilliSeconds() / entry.second.rxPackets)
                              : 0;
        double avgJitter =
            (entry.second.rxPackets > 1)
                ? (entry.second.jitterSum.GetMilliSeconds() / (entry.second.rxPackets - 1))
                : 0;

        std::cout << "FLOW " << entry.first << " " << t.sourceAddress << " -> "
                  << t.destinationAddress << std::endl;
        std::cout << "FLOW_STATS"
                  << " tx=" << entry.second.txPackets << " rx=" << entry.second.rxPackets
                  << " lost=" << entry.second.lostPackets << " lossRate=" << lossRate << "%"
                  << " throughput=" << throughput << "Mbps"
                  << " avgDelay=" << avgDelay << "ms"
                  << " avgJitter=" << avgJitter << "ms"
                  << " queueDrops=" << g_queueDropCount << " bdpOvershoot=" << g_bdpOvershootCount
                  << std::endl;

        std::ofstream sf("stats_" + tag + ".dat", std::ios::app);
        sf << throughput << " " << lossRate << " " << avgDelay << " " << avgJitter << " "
           << entry.second.txPackets << " " << entry.second.rxPackets << " "
           << entry.second.lostPackets << " " << g_queueDropCount << " " << g_bdpOvershootCount
           << "\n";
    }

    if (g_cwndFile.is_open())
    {
        g_cwndFile.close();
    }
    if (g_rttFile.is_open())
    {
        g_rttFile.close();
    }
    if (g_dropsFile.is_open())
    {
        g_dropsFile.close();
    }
    if (g_phaseFile.is_open())
    {
        g_phaseFile.close();
    }

    Simulator::Destroy();
    return 0;
}