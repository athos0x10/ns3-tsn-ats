import json
import os

def simulation_generate_topology(meta, json_path):
    """Parse the JSON topology configuration and inject the network architecture code."""
    simulation_path = meta["path"]
    
    with open(json_path, 'r', encoding="utf-8") as f:
        data = json.load(f)
    
    topo = data["topology"]
    switches = topo["switches"]
    end_systems = topo["end_systems"]
    links = topo["links"]
    
    delay_unit = "MicroSeconds" if topo.get("delay_units") == "MICRO_SECOND" else "MilliSeconds"
    
    cpp = []
    cpp.append("  // -------------------------------------------------------------")
    cpp.append(f"  // --- Topology Auto-Generated from {os.path.basename(json_path)} ---")
    cpp.append("  // -------------------------------------------------------------")
    cpp.append("")
    
    cpp.append("  // Creation of the nodes")
    cpp.append("  // --- Switches ---")
    for sw in switches:
        sw_id = sw["id"]
        idx = sw_id.replace("SW", "")
        cpp.append(f'  Ptr<TsnNode> n{idx} = CreateObject<TsnNode>();')
        cpp.append(f'  Names::Add("{sw_id}", n{idx});')
        cpp.append("")
        
    cpp.append("  // --- End Systems ---")
    for es in end_systems:
        es_id = es["id"]
        idx = es_id.replace("ES", "")
        cpp.append(f'  Ptr<TsnNode> es{idx} = CreateObject<TsnNode>();')
        cpp.append(f'  Names::Add("{es_id}", es{idx});')
        cpp.append("")

    node_ports = {}
    for link in links:
        src, src_p = link["source"], link["sourcePort"]
        dst, dst_p = link["destination"], link["destinationPort"]
        
        if src not in node_ports: node_ports[src] = set()
        if dst not in node_ports: node_ports[dst] = set()
        
        node_ports[src].add(src_p)
        node_ports[dst].add(dst_p)

    cpp.append("  // Create and add a netDevice to each node")
    for node_id in sorted(node_ports.keys()):
        cpp.append(f"  // --- {node_id} Ports ---")
        var_prefix = node_id.lower()
        
        if node_id.startswith("SW"):
            parent_node_var = f"n{node_id.replace('SW', '')}"
        else:
            parent_node_var = node_id.lower()
            
        for port in sorted(node_ports[node_id]):
            port_str = f"{port:02d}"
            dev_var = f"{var_prefix}_p{port}"
            cpp.append(f"  Ptr<TsnNetDevice> {dev_var} = CreateObject<TsnNetDevice>();")
            cpp.append(f"  {parent_node_var}->AddDevice({dev_var});")
            cpp.append(f'  Names::Add("{node_id}#{port_str}", {dev_var});')
            cpp.append("")

    cpp.append("  // Switch device configuration")
    cpp.append("  // --- Creation and Configuration of SwitchNetDevices ---")
    for sw in switches:
        sw_id = sw["id"]
        sw_var = sw_id.lower()
        sw_node = f"n{sw_id.replace('SW', '')}"
        
        cpp.append(f"  Ptr<SwitchNetDevice> {sw_var} = CreateObject<SwitchNetDevice>();")
        cpp.append(f'  {sw_var}->SetAttribute("MinForwardingLatency", TimeValue(MicroSeconds(2)));')
        cpp.append(f'  {sw_var}->SetAttribute("MaxForwardingLatency", TimeValue(MicroSeconds(5)));')
        cpp.append(f"  {sw_node}->AddDevice({sw_var});")
        
        if sw_id in node_ports:
            for port in sorted(node_ports[sw_id]):
                cpp.append(f"  {sw_var}->AddSwitchPort({sw_var}_p{port});")
        cpp.append("")

    cpp.append("  // Data rate configuration")
    configured_datarates = set()
    for link in links:
        src, src_p = link["source"], link["sourcePort"]
        dst, dst_p = link["destination"], link["destinationPort"]
        bw = link["bandwidth_mbps"]
        
        port_pair = tuple(sorted([f"{src}_p{src_p}", f"{dst}_p{dst_p}"]))
        if port_pair not in configured_datarates:
            configured_datarates.add(port_pair)
            rate_str = f"{bw}Mbps" if bw < 1000 else f"{bw//1000}Gbps"
            src_dev = f"{src.lower()}_p{src_p}"
            dst_dev = f"{dst.lower()}_p{dst_p}"
            
            cpp.append(f"  // {src} <-> {dst}")
            cpp.append(f'  {src_dev}->SetAttribute("DataRate", DataRateValue(DataRate("{rate_str}")));')
            cpp.append(f'  {dst_dev}->SetAttribute("DataRate", DataRateValue(DataRate("{rate_str}")));')
            cpp.append("")

    cpp.append("  // Create full-duplex channel")
    processed_channels = set()
    for link in links:
        src, src_p = link["source"], link["sourcePort"]
        dst, dst_p = link["destination"], link["destinationPort"]
        delay = link["delay"]
        
        channel_key = tuple(sorted([f"{src}_p{src_p}", f"{dst}_p{dst_p}"]))
        if channel_key not in processed_channels:
            processed_channels.add(channel_key)
            ch_name = f"ch_{src.lower()}_{dst.lower()}"
            src_dev = f"{src.lower()}_p{src_p}"
            dst_dev = f"{dst.lower()}_p{dst_p}"
            
            cpp.append(f"  // --- {src} <-> {dst} (Delay: {delay} us) ---")
            cpp.append(f"  Ptr<EthernetChannel> {ch_name} = CreateObject<EthernetChannel>();")
            cpp.append(f'  {ch_name}->SetAttribute("Delay", TimeValue({delay_unit}({delay})));')
            cpp.append(f"  {src_dev}->Attach({ch_name});")
            cpp.append(f"  {dst_dev}->Attach({ch_name});")
            cpp.append("")

    cpp.append("  // Allocate a Mac address and create a FIFO for each netDevice")
    cpp.append("  // --- Switch Ports Vector for Automation ---")
    cpp.append("  std::vector<Ptr<TsnNetDevice>> sw_ports = {")
    all_sw_ports = []
    for sw in switches:
        sw_id = sw["id"]
        if sw_id in node_ports:
            for port in sorted(node_ports[sw_id]):
                all_sw_ports.append(f"{sw_id.lower()}_p{port}")
    cpp.append("      " + ", ".join(all_sw_ports) + "};")
    cpp.append("")
    cpp.append("  for (auto &port : sw_ports)")
    cpp.append("  {")
    cpp.append("      port->SetAddress(Mac48Address::Allocate());")
    cpp.append("      for (int i = 0; i < 8; ++i)")
    cpp.append("      {")
    cpp.append("          port->SetQueue(CreateObject<DropTailQueue<Packet>>());")
    cpp.append("      }")
    cpp.append("  }")
    cpp.append("")

    cpp.append("  // --- End System Ports Vector ---")
    cpp.append("  std::vector<Ptr<TsnNetDevice>> es_ports = {")
    all_es_ports = []
    for es in end_systems:
        es_id = es["id"]
        if es_id in node_ports:
            for port in sorted(node_ports[es_id]):
                all_es_ports.append(f"{es_id.lower()}_p{port}")
    cpp.append("      " + ", ".join(all_es_ports) + "};")
    cpp.append("")
    cpp.append(f"  std::vector<Mac48Address> esMacs({len(all_es_ports)});")
    cpp.append("")
    cpp.append("  for (size_t i = 0; i < es_ports.size(); ++i)")
    cpp.append("  {")
    cpp.append("      esMacs[i] = Mac48Address::Allocate();")
    cpp.append("      es_ports[i]->SetAddress(esMacs[i]);")
    cpp.append("      for (int q = 0; q < 8; ++q)")
    cpp.append("      {")
    cpp.append("          es_ports[i]->SetQueue(CreateObject<DropTailQueue<Packet>>());")
    cpp.append("      }")
    cpp.append("  }")
    cpp.append("")

    with open(simulation_path, "a", encoding="utf-8") as file:
        file.write("\n".join(cpp))
        file.write("\n")
        
    print(f"Topology successfully injected from configuration file: {json_path}")