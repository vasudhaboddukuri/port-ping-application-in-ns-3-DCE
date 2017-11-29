

#include <iostream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/netanim-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ccnx/misc-tools.h"
#include "ns3/constant-position-mobility-model.h"

using namespace ns3;

int main (int argc, char *argv[])
{
  Config::SetDefault ("ns3::OnOffApplication::PacketSize", UintegerValue (512));
  Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("500kb/s"));

  uint32_t    nLeftLeaf = 2;
  uint32_t    nRightLeaf = 1;
  uint32_t    nLeaf = 0; // If non-zero, number of both left and right
  std::string animFile = "dumbbell-animation.xml" ;  // Name of file for animation output
   std::string stack = "ns3";
  bool useUdp = 0;
  std::string bandWidth = "1m";
  CommandLine cmd;
  cmd.AddValue ("nLeftLeaf", "Number of left side leaf nodes", nLeftLeaf);
  cmd.AddValue ("nRightLeaf","Number of right side leaf nodes", nRightLeaf);
  cmd.AddValue ("nLeaf",     "Number of left and right side leaf nodes", nLeaf);
  cmd.AddValue ("animFile",  "File Name for Animation Output", animFile);
  
  cmd.AddValue ("stack", "Name of IP stack: ns3/linux/freebsd.", stack);
  cmd.AddValue ("udp", "Use UDP. Default false (0)", useUdp);
  cmd.AddValue ("bw", "BandWidth. Default 1m.", bandWidth);
  cmd.Parse (argc, argv);

  cmd.Parse (argc,argv);
  if (nLeaf > 0)
    {
      nLeftLeaf = nLeaf;
      nRightLeaf = nLeaf;
    }



  // Create the point-to-point link helpers
  PointToPointHelper pointToPointRouter;
  pointToPointRouter.SetDeviceAttribute  ("DataRate", StringValue ("10Mbps"));
  pointToPointRouter.SetChannelAttribute ("Delay", StringValue ("1ms"));
  PointToPointHelper pointToPointLeaf;
  pointToPointLeaf.SetDeviceAttribute    ("DataRate", StringValue ("10Mbps"));
  pointToPointLeaf.SetChannelAttribute   ("Delay", StringValue ("1ms"));

PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("1ms"));

  PointToPointDumbbellHelper d (nLeftLeaf, pointToPointLeaf,
                                nRightLeaf, pointToPointLeaf,
                                pointToPointRouter);

  // Install Stack
  InternetStackHelper stack;
  d.InstallStack (stack);
DceManagerHelper dceManager;
  dceManager.SetTaskManagerAttribute ("FiberManagerType", StringValue ("UcontextFiberManager"));

  if (stack == "ns3")
    {
      InternetStackHelper stack;
      stack.Install (nodes);
      dceManager.Install (nodes);
    }
  else if (stack == "linux")
    {
#ifdef KERNEL_STACK
      dceManager.SetNetworkStack ("ns3::LinuxSocketFdFactory", "Library", StringValue ("liblinux.so"));
      dceManager.Install (nodes);
      LinuxStackHelper stack;
      stack.Install (nodes);
#else
      NS_LOG_ERROR ("Linux kernel stack for DCE is not available. build with dce-linux module.");
      // silently exit
      return 0;
#endif
    }
  else if (stack == "freebsd")
    {
#ifdef KERNEL_STACK
      dceManager.SetNetworkStack ("ns3::FreeBSDSocketFdFactory", "Library", StringValue ("libfreebsd.so"));
      dceManager.Install (nodes);
      FreeBSDStackHelper stack;
      stack.Install (nodes);
#else
      NS_LOG_ERROR ("FreeBSD kernel stack for DCE is not available. build with dce-freebsd module.");
      // silently exit
      return 0;
#endif
    }

  // Assign IP Addresses
  d.AssignIpv4Addresses (Ipv4AddressHelper ("10.1.1.0", "255.255.255.0"),
                         Ipv4AddressHelper ("10.2.1.0", "255.255.255.0"),
                         Ipv4AddressHelper ("10.3.1.0", "255.255.255.0"));

  // Install on/off app on all right side nodes
  OnOffHelper clientHelper ("ns3::UdpSocketFactory", Address ());
  clientHelper.SetAttribute ("OnTime", StringValue ("ns3::UniformRandomVariable"));
  clientHelper.SetAttribute ("OffTime", StringValue ("ns3::UniformRandomVariable"));
  ApplicationContainer clientApps;

  for (uint32_t i = 0; i < d.RightCount (); ++i)
    {
      // Create an on/off app sending packets to the same leaf right side
      AddressValue remoteAddress (InetSocketAddress (d.GetLeftIpv4Address (i), 1000));
      clientHelper.SetAttribute ("Remote", remoteAddress);
      clientApps.Add (clientHelper.Install (d.GetRight (i)));
    }

  clientApps.Start (Seconds (0.0));
  clientApps.Stop (Seconds (10.0));

  // Set the bounding box for animation
  d.BoundingBox (1, 1, 100, 100);

  // Create the animation object and configure for specified output
  AnimationInterface anim (animFile);
  anim.EnablePacketMetadata (); // Optional
  anim.EnableIpv4L3ProtocolCounters (Seconds (0), Seconds (10)); // Optional
  
  // Set up the acutal simulation
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

DceApplicationHelper dce;
  ApplicationContainer apps;

  dce.SetStackSize (1 << 20);

  // Launch ping client on node 0
 
  dce.SetBinary ("ping");
  dce.ResetArguments ();
  dce.ResetEnvironment ();
  dce.AddArgument ("10.1.1.2");
  
  if (useUdp)
    {
      dce.AddArgument ("-u");
      dce.AddArgument ("-b");
      dce.AddArgument (bandWidth);
    }

  apps = dce.Install (nodes.Get (0));
  apps.Start (Seconds (0.7));
  apps.Stop (Seconds (20));

  // Launch iperf server on node 1
  dce.SetBinary("ping"); 
  dce.ResetArguments ();
  dce.ResetEnvironment ();
  dce.AddArgument ("10.1.1.1");


  if (useUdp)
    {
      dce.AddArgument ("-u");
    }

  apps = dce.Install (nodes.Get (1));

  pointToPoint.EnablePcapAll ("iperf-" + stack, false);

  apps.Start (Seconds (0.6));

  setPos (nodes.Get (0), 1, 10, 0);
  setPos (nodes.Get (1), 50,10, 0);

  Simulator::Stop (Seconds (40.0));
  

  Simulator::Run ();
  std::cout << "Animation Trace file created:" << animFile.c_str ()<< std::endl;
  Simulator::Destroy ();
  return 0;
}
