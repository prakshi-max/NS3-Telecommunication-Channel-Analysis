# NS-3 Telecommunication Channel Performance Analysis

## Overview

This project implements a two-node communication network using the NS-3 network simulator to study packet transmission and network performance over a point-to-point telecommunication channel.

The main objective is to analyze:

- Packet Loss
- Latency
- Throughput
- Effect of Channel Delay
- Effect of Limited Bandwidth and High Traffic

## Network Architecture

Node 0 (Sender)
        |
        | UDP Packets
        |
Point-to-Point Communication Channel
        |
        |
Node 1 (Receiver)

## Tools and Technologies

- NS-3 Network Simulator
- C++
- Ubuntu / WSL2
- UDP
- Point-to-Point Network
- FlowMonitor

## Experiments

### 1. Baseline Packet Transmission

A two-node network was created with:

- Bandwidth: 10 Mbps
- Channel Delay: 10 ms
- Packet Size: 1024 bytes
- Packets Sent: 100

Result:

- Packets Received: 100
- Packets Lost: 0
- Packet Loss Rate: 0%

### 2. Packet Loss under Congestion

The channel bandwidth was reduced to 50 Kbps while generating traffic at a much higher rate.

Parameters:

- Bandwidth: 50 Kbps
- Channel Delay: 10 ms
- Packet Size: 1024 bytes
- Packets Sent: 1000
- Packet Interval: 1 ms

Result:

- Packets Received: 106
- Packets Lost: 894
- Packet Loss Rate: 89.4%

The high offered traffic exceeded the channel capacity, causing congestion and packet drops.

### 3. Delay vs Latency

The channel delay was varied while keeping the other parameters constant.

| Channel Delay | Packets Sent | Packets Received | Packet Loss | Average Latency |
|---|---:|---:|---:|---:|
| 10 ms | 100 | 100 | 0% | 10.8432 ms |
| 20 ms | 100 | 100 | 0% | 20.8432 ms |
| 50 ms | 100 | 100 | 0% | 50.8432 ms |
| 100 ms | 100 | 100 | 0% | 100.843 ms |

The results demonstrate that increasing channel delay increases end-to-end latency.

### 4. Throughput Analysis

A throughput experiment was performed using a 10 Mbps point-to-point channel.

Result:

- Packets Sent: 1000
- Packets Received: 1000
- Packets Lost: 0
- Packet Loss Rate: 0%
- Received Bytes: 1,052,000
- Throughput: 0.842442 Mbps
- Average Latency: 10.8432 ms

## Key Findings

1. A sufficient channel capacity can provide reliable packet delivery under moderate traffic.
2. When offered traffic significantly exceeds channel capacity, congestion can result in severe packet loss.
3. Increasing channel delay increases end-to-end latency.
4. Throughput represents the actual successfully delivered data rate and can be lower than the configured channel capacity when the application does not fully utilize the link.

## Project Structure

```text
NS3-Telecommunication-Channel-Analysis/
│
├── README.md
│
├── simulations/
│   ├── first-network.cc
│   ├── packet-loss-experiment.cc
│   ├── latency-experiment.cc
│   ├── delay-comparison.cc
│   └── throughput-experiment.cc
│
└── results/
