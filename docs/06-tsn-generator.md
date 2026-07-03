# TSN Network Generator: JSON Schema Strategy

## 1. Introduction & Strategy
Configuring Time-Sensitive Networking (TSN) mechanisms directly in `ns-3` (using frameworks like Eden-sim) involves writing hundreds of repetitive lines of C++ code. Manually managing clocks, queues, Stream Identifications, and frame-handling functions (FRER, PSFP, ATS) is highly prone to human error.

To solve this, we implement a **Four-Layer Decoupling Strategy** using a structured JSON configuration file. This abstraction separates the logical network intent from the low-level C++ instantiations, making the syntax easy to write for humans while keeping it strictly typed for an automated Python parsing script.

### The 4 Core Layers:
1. **Infrastructure Layer (`nodes`):** Defines the active devices and their clock domains (e.g., Grandmaster vs. Drift clocks).
2. **Topology & Hardware Layer (`links`):** Connects the network devices and defines hardware-level queues (FIFO vs. CBS).
3. **TSN Control Plane (`tsn_control_plane`):** Implements time schedules (TAS), traffic filtering (PSFP), frame redundancy (FRER), per-hop shaping (ATS), and stream identifiers using **cross-referencing handles** (`stream_handle`, `shaper_id`).
4. **Traffic Layer (`applications`):** Generates application payloads without duplicating network properties by referencing topological endpoints (`dest_device_target`).

## 2. Dynamic C++ Translation Mapping
To build a Python automation script that reads this JSON and outputs a `.cc` file, the parser follows this deterministic C++ code-generation pipeline:

| JSON Element | Logic Triggered in Generator | Translated C++ Output Code |
| :--- | :--- | :--- |
| `"type": "ConstantDriftClock"` | Instantiates a hardware drift model | `CreateObject<ConstantDriftClock>(); c1->SetAttribute(...);` |
| `"cbs_on_fifo_1"` | Binds a Shaper to a specific port queue | `Ptr<Cbs> cbs = CreateObject<Cbs>(); net0->SetQueue(..., cbs);` |
| `"stream_handle": 10` | Allocates a stream key and caches it | `n3->AddStreamIdentificationFunction(10, sif0, ...);` |
| `"dest_device_target"` | Dynamically resolves the destination MAC address | `AddressValue(net2->GetAddress())` |

## 3. The Complete TSN Unified JSON Blueprint

Below is the complete, single-file JSON configuration demonstrating every single TSN mechanism supported by the simulation library (CBS, TAS, gPTP, Stream Identification, PSFP, FRER, and ATS) linked together over our topology.

```json
{
  "simulation": {
    "name": "Full TSN Suite Simulation",
    "stop_time_seconds": 10.0,
    "log_components": ["Chapter 3", "AtsSchedulerGroup"]
  },

  "nodes": [
    { "id": "n0", "name": "ES1", "clock": { "type": "PerfectClock" } },
    { 
      "id": "n1", 
      "name": "ES2", 
      "clock": { 
        "type": "ConstantDriftClock", 
        "initial_offset_sec": 20.0, 
        "drift_rate": -50.0, 
        "granularity_ns": 10.0 
      } 
    },
    { 
      "id": "n2", 
      "name": "ES3", 
      "clock": { 
        "type": "ConstantDriftClock", 
        "initial_offset_sec": 3.0, 
        "drift_rate": 2.0, 
        "granularity_ns": 10.0 
      } 
    },
    { 
      "id": "n3", 
      "name": "SW", 
      "clock": { 
        "type": "ConstantDriftClock", 
        "initial_offset_sec": 0.5, 
        "drift_rate": -25.0, 
        "granularity_ns": 10.0 
      } 
    }
  ],

  "links": [
    {
      "from": { "node": "n0", "device_name": "ES1#01" },
      "to": { "node": "n3", "device_name": "SW#01" },
      "data_rate": "100Mb/s",
      "delay_ns": 50,
      "queues": { 
        "count": 8, 
        "cbs_on_fifo_1": { "idle_slope": "20Kb/s", "transmit_rate": "100Mb/s" } 
      }
    },
    {
      "from": { "node": "n1", "device_name": "ES2#01" },
      "to": { "node": "n3", "device_name": "SW#02" },
      "data_rate": "100Mb/s",
      "delay_ns": 75,
      "queues": { "count": 8 }
    },
    {
      "from": { "node": "n3", "device_name": "SW#03" },
      "to": { "node": "n2", "device_name": "ES3#01" },
      "data_rate": "100Mb/s",
      "delay_ns": 100,
      "queues": { "count": 8 }
    }
  ],

  "tsn_control_plane": {
    "gptp": {
      "domain": 0,
      "sync_interval_sec": 0.125,
      "pdelay_interval_sec": 1.0,
      "priority": 7,
      "bindings": [
        { "node": "n0", "device": "ES1#01", "role": "MASTER" },
        { "node": "n1", "device": "ES2#01", "role": "SLAVE" },
        { "node": "n2", "device": "ES3#01", "role": "SLAVE" },
        { "node": "n3", "device": "SW#01", "role": "SLAVE" },
        { "node": "n3", "device": "SW#02", "role": "MASTER" },
        { "node": "n3", "device": "SW#03", "role": "MASTER" }
      ]
    },

    "tas": [
      {
        "node": "n3",
        "egress_device": "SW#03",
        "schedule": [
          { "duration_sec": 2.0, "gate_mask": 0 },
          { "duration_sec": 3.0, "gate_mask": 2 }
        ]
      }
    ],

    "stream_identification": [
      {
        "stream_handle": 10,
        "node": "n3",
        "ingress_devices": ["SW#01"],
        "filter": { "vlan_id": 1, "dest_device_target": "ES3#01" }
      }
    ],

    "psfp": [
      {
        "node": "n3",
        "stream_handle": 10,
        "priority_wildcard": true,
        "max_sdu_size": 1422,
        "flow_meter": {
          "cir": "20Kb/s",
          "cbs_bytes": 1400,
          "drop_on_yellow": true,
          "mark_all_red_enable": false
        }
      }
    ],

    "frer": [
      {
        "node": "n3",
        "stream_handles": [10],
        "generation": { "direction": "in-facing" },
        "encode": { "direction": "in-facing", "active": true, "port": "SW#01" },
        "replication_targets": {
          "dest_device_target": "ES3#01",
          "vlan_id": 1,
          "egress_ports": ["SW#02", "SW#03"]
        }
      }
    ],

    "ats": [
      {
        "node": "n3",
        "clock_node_ref": "n3",
        "egress_device": "SW#03",
        "ingress_device": "SW#01",
        "max_residence_time_sec": 1.0,
        "shapers": [
          {
            "shaper_id": 1,
            "cir": "15Mbps",
            "cbs_bytes": 32768,
            "bind_streams": [
              { "vlan_id": 1, "dest_device_target": "ES3#01" }
            ]
          }
        ]
      }
    ]
  },

  "applications": [
    {
      "type": "EthernetGenerator",
      "source_node": "n0",
      "source_device": "ES1#01",
      "dest_device_target": "ES3#01",
      "start_time_sec": 0.0,
      "stop_time_sec": 10.0,
      "attributes": {
        "BurstSize": 5,
        "PayloadSize": 1400,
        "Period_sec": 5.0,
        "VlanID": 1,
        "PCP": 1
      }
    }
  ]
}