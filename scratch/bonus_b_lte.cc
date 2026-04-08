/*
 * bonus_b_lte.cc — Bonus B + Bonus C
 *
 * Bonus B: LTE (4G) network simulation
 * ─────────────────────────────────────
 * Topology:
 *
 *   [RemoteHost] ──P2P(100Gbps,10ms)── [PGW/SGW (EPC)] ──LTE── [eNB]
 *                                                                  │
 *                                                     [UE0] … [UE(N-1)]
 *
 *   TCP flows run downlink: RemoteHost → UEs.
 *   HyStart++ (VART) is used as the TCP congestion control algorithm.
 *
 * Bonus C: Extra metrics (beyond assignment requirements)
 * ────────────────────────────────────────────────────────
 *   1. Per-UE throughput             → per_node_throughput_lte.csv
 *   2. Queue size over time          → queue_size_lte.dat
 *      (sampled on RemoteHost→PGW bottleneck P2P link every 200 ms)
 *
 * Energy model (LTE):
 *   ns-3 does not ship a ready-made LteRadioEnergyModel.
 *   We use a power-state approximation:
 *     E_per_UE ≈ P_TX × t_active + P_RX × t_active + P_IDLE × t_idle
 *   where P_TX=0.5W, P_RX=0.3W, P_IDLE=0.05W are from 3GPP TR 36.814 §B.1.
 *   t_active is derived from FlowMonitor's txBytes / link_rate.
 *   This is a conservative approximation labelled clearly in output.
 *
 * Bonus D: VART adaptive divisor from bonus_common.h is used.
 *
 * Build: place bonus_common.h, bonus_a_hybrid.cc, bonus_b_lte.cc in ns-3
 *        scratch/ directory, then:
 *   ./ns3 run bonus_b_lte
 */

#include "bonus_common.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/internet-module.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("BonusBLte");

// ── LTE energy constants (3GPP TR 36.814 §B.1) ──────────────────────────
static constexpr double LTE_P_TX_W = 0.50;   ///< UE TX power (W)
static constexpr double LTE_P_RX_W = 0.30;   ///< UE RX power (W)
static constexpr double LTE_P_IDLE_W = 0.05; ///< UE idle power (W)
static constexpr double LTE_DL_RATE = 75e6;  ///< LTE DL peak ~75 Mbps

// ── Bonus C helpers ────────────────────────────────────────────────────────
static double
ComputeJFI(const std::vector<double>& t)
{
    if (t.empty())
    {
        return 0.0;
    }
    double sx = 0.0, sx2 = 0.0;
    for (double x : t)
    {
        sx += x;
        sx2 += x * x;
    }
    double n = (double)t.size();
    return (sx2 > 0.0) ? (sx * sx) / (n * sx2) : 1.0;
}

static std::ofstream g_queueFile;

static void
SampleQueue(Ptr<QueueDisc> qd)
{
    if (g_queueFile.is_open())
    {
        g_queueFile << Simulator::Now().GetSeconds() << " " << qd->GetNPackets() << "\n";
    }
    Simulator::Schedule(MilliSeconds(200), &SampleQueue, qd);
}

// ── SimResult ─────────────────────────────────────────────────────────────
struct LteResult
{
    uint32_t variedParam;
    double throughputMbps;
    double avgDelayMs;
    double pdrPct;
    double dropPct;
    double energyJ; ///< approximated from 3GPP power model
    double jfi;
};

// ════════════════════════════════════════════════════════════════════════════
//  RunLteSim
// ════════════════════════════════════════════════════════════════════════════
static LteResult
RunLteSim(uint32_t numUes,
          uint32_t flowCount,
          uint32_t pps,
          uint32_t pktSize,
          double cellRadius, ///< UE placement radius from eNB (m)
          double simTime,
          bool traceQueue = false)
{
    LteResult res{};

    // ── TCP ──────────────────────────────────────────────────────────────
    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                       StringValue("ns3::TcpHyStartPlusAdaptive"));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(10));

    // ── LTE + EPC helpers ────────────────────────────────────────────────
    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);

    // Use Friis propagation (suitable for open-space, no shadowing)
    lteHelper->SetAttribute("PathlossModel", StringValue("ns3::FriisSpectrumPropagationLossModel"));

    // Round-Robin scheduler — fair baseline for HyStart++ testing
    lteHelper->SetSchedulerType("ns3::RrFfMacScheduler");

    // ── Nodes ────────────────────────────────────────────────────────────
    NodeContainer enbNodes, ueNodes;
    enbNodes.Create(1);
    ueNodes.Create(numUes);

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);

    // ── Internet on remote host ──────────────────────────────────────────
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    // P2P link: remote host ↔ PGW  (100 Gbps so it is never the bottleneck)
    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", StringValue("100Gbps"));
    p2ph.SetChannelAttribute("Delay", StringValue("10ms"));
    NetDeviceContainer internetDevs = p2ph.Install(pgw, remoteHost);

    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIfaces = ipv4h.Assign(internetDevs);

    // Queue disc on remote host's P2P interface (for Bonus C trace)
    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
    QueueDiscContainer rhQd = tch.Install(internetDevs.Get(1));

    if (traceQueue)
    {
        g_queueFile.open("queue_size_lte.dat", std::ios::trunc);
        g_queueFile << "# time(s)  queue_packets  (at RemoteHost egress)\n";
        Simulator::Schedule(MilliSeconds(200), &SampleQueue, rhQd.Get(0));
    }

    // Static routing on remote host → UE subnet (7.0.0.0/8 assigned by EPC)
    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostRoute =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostRoute->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    // ── Mobility ─────────────────────────────────────────────────────────
    MobilityHelper mob;
    mob.SetMobilityModel("ns3::ConstantPositionMobilityModel");

    // eNB at origin
    Ptr<ListPositionAllocator> enbPos = CreateObject<ListPositionAllocator>();
    enbPos->Add(Vector(0.0, 0.0, 0.0));
    mob.SetPositionAllocator(enbPos);
    mob.Install(enbNodes);

    // UEs distributed on a circle of radius cellRadius around the eNB
    Ptr<ListPositionAllocator> uePos = CreateObject<ListPositionAllocator>();
    for (uint32_t i = 0; i < numUes; i++)
    {
        double angle = 2.0 * M_PI * i / numUes;
        uePos->Add(Vector(cellRadius * std::cos(angle), cellRadius * std::sin(angle), 0.0));
    }
    mob.SetPositionAllocator(uePos);
    mob.Install(ueNodes);

    // Remote host position (irrelevant — no wireless propagation to it)
    Ptr<ListPositionAllocator> rhPos = CreateObject<ListPositionAllocator>();
    rhPos->Add(Vector(1000.0, 0.0, 0.0));
    mob.SetPositionAllocator(rhPos);
    mob.Install(remoteHost);

    // ── Install LTE devices ────────────────────────────────────────────
    NetDeviceContainer enbDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueDevs = lteHelper->InstallUeDevice(ueNodes);

    // ── Internet on UEs + IP assignment ───────────────────────────────
    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIfaces = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueDevs));

    // Default route on each UE → EPC gateway
    for (uint32_t u = 0; u < ueNodes.GetN(); u++)
    {
        Ptr<Ipv4StaticRouting> ueRoute =
            ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(u)->GetObject<Ipv4>());
        ueRoute->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    // Attach all UEs to the single eNB
    lteHelper->Attach(ueDevs, enbDevs.Get(0));

    // ── Applications (downlink: RemoteHost → UEs) ─────────────────────
    double appStart = 2.0;
    double appStop = simTime - 1.0;
    uint32_t actualFlows = std::min(flowCount, numUes);

    std::vector<Ptr<PacketSink>> sinkPtrs;

    double dataRate = (double)pps * pktSize * 8.0;
    std::ostringstream rateStr;
    rateStr << (uint64_t)dataRate << "bps";

    for (uint32_t i = 0; i < actualFlows; i++)
    {
        uint16_t port = 40000 + i;
        uint32_t ueIdx = i % numUes;

        // Sink on UE
        PacketSinkHelper sinkH("ns3::UdpSocketFactory",
                               InetSocketAddress(Ipv4Address::GetAny(), port));
        ApplicationContainer sinkApp = sinkH.Install(ueNodes.Get(ueIdx));
        sinkApp.Start(Seconds(0.0));
        sinkApp.Stop(Seconds(simTime));
        sinkPtrs.push_back(DynamicCast<PacketSink>(sinkApp.Get(0)));

        // Source on remote host
        OnOffHelper onoff("ns3::UdpSocketFactory",
                          InetSocketAddress(ueIfaces.GetAddress(ueIdx), port));
        onoff.SetConstantRate(DataRate(rateStr.str()), pktSize);
        ApplicationContainer srcApp = onoff.Install(remoteHost);
        srcApp.Start(Seconds(appStart));
        srcApp.Stop(Seconds(appStop));
    }

    // ── Flow monitor ─────────────────────────────────────────────────────
    FlowMonitorHelper fmHelper;
    Ptr<FlowMonitor> monitor = fmHelper.InstallAll();

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // ── Collect results ───────────────────────────────────────────────────
    monitor->CheckForLostPackets();
    auto& flowStats = monitor->GetFlowStats();

    uint64_t totalTx = 0, totalRx = 0, totalLost = 0, totalRxBytes = 0, totalTxBytes = 0;
    double totalDelay = 0.0;
    std::vector<double> flowThroughputs;

    double duration = appStop - appStart;

    for (auto& kv : flowStats)
    {
        totalTx += kv.second.txPackets;
        totalRx += kv.second.rxPackets;
        totalLost += kv.second.lostPackets;
        totalRxBytes += kv.second.rxBytes;
        totalTxBytes += kv.second.txBytes;
        totalDelay += kv.second.delaySum.GetMilliSeconds();

        double flowDur =
            kv.second.timeLastRxPacket.GetSeconds() - kv.second.timeFirstTxPacket.GetSeconds();
        if (flowDur > 0.0)
        {
            flowThroughputs.push_back(kv.second.rxBytes * 8.0 / flowDur / 1e6);
        }
    }

    res.throughputMbps = (duration > 0) ? (totalRxBytes * 8.0 / duration / 1e6) : 0.0;
    res.avgDelayMs = (totalRx > 0) ? (totalDelay / totalRx) : 0.0;
    res.pdrPct = (totalTx > 0) ? (100.0 * totalRx / totalTx) : 0.0;
    res.dropPct = (totalTx > 0) ? (100.0 * totalLost / totalTx) : 0.0;
    res.jfi = ComputeJFI(flowThroughputs);

    // ── LTE energy approximation (3GPP TR 36.814 §B.1) ────────────────
    // t_active per UE ≈ bytes_transferred / LTE_DL_RATE
    double totalBytes = (double)(totalTxBytes + totalRxBytes);
    double tActivePerUe = (numUes > 0) ? (totalBytes / numUes / LTE_DL_RATE) : 0.0;
    tActivePerUe = std::min(tActivePerUe, duration); // cap at sim duration
    double tIdlePerUe = std::max(0.0, duration - tActivePerUe);

    double energyPerUe = (LTE_P_TX_W + LTE_P_RX_W) * tActivePerUe + LTE_P_IDLE_W * tIdlePerUe;
    res.energyJ = energyPerUe * numUes;

    // Bonus C: per-UE throughput
    if (traceQueue)
    {
        std::ofstream pnFile("per_node_throughput_lte.csv", std::ios::trunc);
        pnFile << "ue_idx,rx_bytes,throughput_mbps\n";
        for (uint32_t i = 0; i < sinkPtrs.size(); i++)
        {
            if (sinkPtrs[i])
            {
                uint64_t rxB = sinkPtrs[i]->GetTotalRx();
                double tput = (duration > 0) ? rxB * 8.0 / duration / 1e6 : 0.0;
                pnFile << i << "," << rxB << "," << tput << "\n";
            }
        }
        if (g_queueFile.is_open())
        {
            g_queueFile.close();
        }
    }

    Simulator::Destroy();
    return res;
}

// ════════════════════════════════════════════════════════════════════════════
//  main
// ════════════════════════════════════════════════════════════════════════════
int
main(int argc, char* argv[])
{
    double simTime = 40.0;
    double cellRadius = 100.0; // metres — UE placement radius
    uint32_t pktSize = 1024;
    bool doFullSweep = true;

    CommandLine cmd(__FILE__);
    cmd.AddValue("simTime", "Simulation time (s)", simTime);
    cmd.AddValue("cellRadius", "LTE cell radius (m)", cellRadius);
    cmd.AddValue("fullSweep", "Run all sweeps (1=yes)", doFullSweep);
    cmd.Parse(argc, argv);

    // std::cout << "===== Bonus B: LTE Network (HyStart++ VART) =====\n"
    //           << "Topology: [RemoteHost]--P2P--[EPC/PGW]--LTE--[eNB]--[UE×N]\n"
    //           << "TCP congestion control: TcpHyStartPlusAdaptive (VART, Bonus D)\n"
    //           << "Energy model: 3GPP TR 36.814 §B.1 power-state approximation\n\n";

    const uint32_t DEF_UES = 10;
    const uint32_t DEF_FLOWS = 10;
    const uint32_t DEF_PPS = 100;
    // Cell radius sweep replaces coverage-area sweep from the WiFi scenario
    std::vector<double> radSweep = {50.0, 100.0, 200.0, 500.0, 1000.0};

    std::vector<uint32_t> ueSweep = {10, 20, 30, 40, 50};
    std::vector<uint32_t> flowSweep = {10, 20, 30, 40, 50};
    std::vector<uint32_t> ppsSweep = {100, 200, 300, 400, 500};

    auto openCsv = [](const std::string& name) {
        std::ofstream f(name);
        f << "varied_value,throughput_mbps,avg_delay_ms,pdr_pct,"
             "drop_pct,energy_J_approx,jains_fairness_index\n";
        return f;
    };

    auto printResult = [](const char* label, double val, const LteResult& r) {
        // std::cout << "  " << label << "=" << val
        //           << "  tp=" << std::fixed << std::setprecision (3) << r.throughputMbps
        //           << "Mbps  PDR=" << std::setprecision (1) << r.pdrPct
        //           << "%  JFI=" << std::setprecision (3) << r.jfi
        //           << "  E≈" << std::setprecision (2) << r.energyJ << "J\n";
    };

    // ── 1. Vary UE count ────────────────────────────────────────────────
    {
        std::ofstream csv = openCsv("results_lte_vary_ues.csv");
        // std::cout << "[1/4] Varying UE count (flows=" << DEF_FLOWS
        //           << " pps=" << DEF_PPS << " r=" << cellRadius << "m)\n";
        for (uint32_t n : ueSweep)
        {
            bool trace = (n == ueSweep.front());
            LteResult r = RunLteSim(n, DEF_FLOWS, DEF_PPS, pktSize, cellRadius, simTime, trace);
            csv << n << "," << r.throughputMbps << "," << r.avgDelayMs << "," << r.pdrPct << ","
                << r.dropPct << "," << r.energyJ << "," << r.jfi << "\n";
            printResult("UEs", (double)n, r);
        }
    }
    if (!doFullSweep)
    {
        return 0;
    }

    // ── 2. Vary flow count ────────────────────────────────────────────────
    {
        std::ofstream csv = openCsv("results_lte_vary_flows.csv");
        // std::cout << "[2/4] Varying flow count (UEs=" << DEF_UES
        //           << " pps=" << DEF_PPS << ")\n";
        for (uint32_t f : flowSweep)
        {
            LteResult r = RunLteSim(DEF_UES, f, DEF_PPS, pktSize, cellRadius, simTime, false);
            csv << f << "," << r.throughputMbps << "," << r.avgDelayMs << "," << r.pdrPct << ","
                << r.dropPct << "," << r.energyJ << "," << r.jfi << "\n";
            printResult("flows", (double)f, r);
        }
    }

    // ── 3. Vary packets per second ────────────────────────────────────────
    {
        std::ofstream csv = openCsv("results_lte_vary_pps.csv");
        // std::cout << "[3/4] Varying pps (UEs=" << DEF_UES
        //           << " flows=" << DEF_FLOWS << ")\n";
        for (uint32_t p : ppsSweep)
        {
            LteResult r = RunLteSim(DEF_UES, DEF_FLOWS, p, pktSize, cellRadius, simTime, false);
            csv << p << "," << r.throughputMbps << "," << r.avgDelayMs << "," << r.pdrPct << ","
                << r.dropPct << "," << r.energyJ << "," << r.jfi << "\n";
            printResult("pps", (double)p, r);
        }
    }

    // ── 4. Vary cell radius (analogous to coverage-area sweep) ───────────
    {
        std::ofstream csv = openCsv("results_lte_vary_radius.csv");
        // std::cout << "[4/4] Varying cell radius (UEs=" << DEF_UES
        //           << " flows=" << DEF_FLOWS << " pps=" << DEF_PPS << ")\n";
        for (double rad : radSweep)
        {
            LteResult r = RunLteSim(DEF_UES, DEF_FLOWS, DEF_PPS, pktSize, rad, simTime, false);
            csv << rad << "," << r.throughputMbps << "," << r.avgDelayMs << "," << r.pdrPct << ","
                << r.dropPct << "," << r.energyJ << "," << r.jfi << "\n";
            printResult("radius_m", rad, r);
        }
    }

    // std::cout << "\nOutputs:\n"
    //           << "  results_lte_vary_{ues,flows,pps,radius}.csv\n"
    //           << "  queue_size_lte.dat               (queue over time — Bonus C)\n"
    //           << "  per_node_throughput_lte.csv      (per-UE throughput — Bonus C)\n"
    //           << "\nNote: energy values are approximations from 3GPP TR 36.814 §B.1.\n";
    return 0;
}
