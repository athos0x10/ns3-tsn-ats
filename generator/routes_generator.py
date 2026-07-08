import json
import os
import re

def simulation_generate_routes(meta, json_path):
    """Parse the JSON routes configuration and inject static forwarding table entries."""
    simulation_path = meta["path"]
    
    with open(json_path, 'r', encoding="utf-8") as f:
        data = json.load(f)
        
    routes = data.get("routes", [])
    
    cpp = []
    cpp.append("  // -------------------------------------------------------------")
    cpp.append(f"  // --- Routes Auto-Generated from {os.path.basename(json_path)} ---")
    cpp.append("  // -------------------------------------------------------------")
    cpp.append("")
    
    for flow in routes :
        flow_id = flow.get("flow_id", 0)
        paths = flow.get("paths", [])
        
        src_node = "Unknown"
        dst_nodes = []
        
        for path in paths :
            if len(path) >= 2:
                src_node = path[0]["node"]
                dst_nodes.append(path[-1]["node"])
        
        dst_str = ", ".join(sorted(list(set(dst_nodes))))
        cpp.append(f"  // FLOW {flow_id} ({src_node} -> {dst_str})")
        
        added_entries = set()
        
        for path in paths :
            # Going through the different switches.
            for i in range(len(path) - 1):
                current_hop = path[i]
                next_hop = path[i+1]
                
                node_name = current_hop["node"]
                
                # Only switches have a forwarding table
                if node_name.startswith("SW"):
                    sw_var = node_name.lower()  # ex: sw1
                    out_port = current_hop["port"]
                    port_var = f"{sw_var}_p{out_port}"  # ex: sw1_p6
                    
                    # The final destination
                    final_dst_node = path[-1]["node"]
                    es_index = re.sub(r"\D", "", final_dst_node)
                    
                    mac_entry = f"esMacs[{es_index}]"
                    vlan_id = 1
                    
                    entry_line = f"  {sw_var}->AddForwardingTableEntry({mac_entry}, {vlan_id}, std::vector<Ptr<NetDevice>>{{{port_var}}});"
                    
                    if entry_line not in added_entries:
                        added_entries.add(entry_line)
                        cpp.append(entry_line)
                        
        cpp.append("")
        
    with open(simulation_path, "a", encoding="utf-8") as file:
        file.write("\n".join(cpp))
        
    print(f"Routes successfully injected from configuration file: {json_path}")