import json
import os
import re

def simulation_generate_streams(meta, json_path, start_time=1.0, stop_time=10.0):
    """Parse the JSON streams configuration and inject Ethernet traffic generators."""
    simulation_path = meta["path"]
    
    with open(json_path, 'r', encoding="utf-8") as f:
        data = json.load(f)
        
    streams = data.get("streams", [])
    delay_unit = "MicroSeconds" if data.get("delay_units") == "MICRO_SECOND" else "MilliSeconds"
    
    cpp = []
    cpp.append("  // -------------------------------------------------------------")
    cpp.append(f"  // --- Streams Auto-Generated from {os.path.basename(json_path)} ---")
    cpp.append("  // -------------------------------------------------------------")
    cpp.append("")
    cpp.append("  // Application Container to hold all generators")
    cpp.append("  ApplicationContainer apps;")
    cpp.append("")
    
    for stream in streams:
        stream_id = stream.get("id", 0)
        name = stream.get("name", f"Stream{stream_id}")
        source = stream.get("source", "")
        destinations = stream.get("destinations", [])
        stream_type = stream.get("type", "BEST-EFFORT")
        pcp = stream.get("PCP", 0)
        size = stream.get("size", 1500)
        period = stream.get("period")
        
        source_var = source.lower()       # ex: es7
        source_port = f"{source_var}_p0"
        
        dest_ids = [int(re.sub(r"\D", "", d["id"])) for d in destinations]
        dest_names = [d["id"] for d in destinations]
        
        # Comment to describe the flow
        cpp.append("  // ==========================================")
        cpp.append(f"  // {name.upper()} (Source: {source} -> Dest: {', '.join(dest_names)})")
        period_str = f"{period}{'us' if delay_unit == 'MicroSeconds' else 'ms'}" if period is not None else "null (Continuous/Asynchronous)"
        cpp.append(f"  // {stream_type}, PCP: {pcp}, Size: {size}B, Period: {period_str}")
        cpp.append("  // ==========================================")
        
        if period is None:
            time_value_str = "Seconds(1)"
        else:
            time_value_str = f"{delay_unit}({period})"
            
        if len(dest_ids) > 1:
            # multi-destination loop
            cpp.append(f"  std::vector<int> stream{stream_id}_dests = {{{', '.join(map(str, dest_ids))}}};")
            cpp.append(f"  for (int dest_idx : stream{stream_id}_dests)")
            cpp.append("  {")
            cpp.append(f"      Ptr<EthernetGenerator> app{stream_id} = CreateObject<EthernetGenerator>();")
            cpp.append(f"      app{stream_id}->Setup({source_port});")
            cpp.append(f"      app{stream_id}->SetAttribute(\"Address\", AddressValue(esMacs[dest_idx]));")
            cpp.append(f"      app{stream_id}->SetAttribute(\"PayloadSize\", UintegerValue({size}));")
            cpp.append(f"      app{stream_id}->SetAttribute(\"Period\", TimeValue({time_value_str}));")
            cpp.append(f"      app{stream_id}->SetAttribute(\"PCP\", UintegerValue({pcp}));")
            cpp.append(f"      app{stream_id}->SetAttribute(\"VlanID\", UintegerValue(1));")
            cpp.append(f"      {source_var}->AddApplication(app{stream_id});")
            cpp.append(f"      apps.Add(app{stream_id});")
            cpp.append("  }")
        else:
            # unique destination
            dest_idx = dest_ids[0]
            cpp.append(f"  Ptr<EthernetGenerator> app{stream_id} = CreateObject<EthernetGenerator>();")
            cpp.append(f"  app{stream_id}->Setup({source_port});")
            cpp.append(f"  app{stream_id}->SetAttribute(\"Address\", AddressValue(esMacs[{dest_idx}]));")
            cpp.append(f"  app{stream_id}->SetAttribute(\"PayloadSize\", UintegerValue({size}));")
            cpp.append(f"  app{stream_id}->SetAttribute(\"Period\", TimeValue({time_value_str}));")
            cpp.append(f"  app{stream_id}->SetAttribute(\"PCP\", UintegerValue({pcp}));")
            cpp.append(f"  app{stream_id}->SetAttribute(\"VlanID\", UintegerValue(1));")
            cpp.append(f"  {source_var}->AddApplication(app{stream_id});")
            cpp.append(f"  apps.Add(app{stream_id});")
            
        cpp.append("")
        
    # Start and end of the simulation
    cpp.append(f"  // Start all traffic applications")
    cpp.append(f"  apps.Start(Seconds({start_time:.1f}));")
    cpp.append(f"  apps.Stop(Seconds({stop_time:.1f}));")
    cpp.append("")

    with open(simulation_path, "a", encoding="utf-8") as file:
        file.write("\n".join(cpp))
        
    print(f"Streams successfully injected from configuration file: {json_path}")