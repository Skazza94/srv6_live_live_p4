from __future__ import division
import sys
import os
import json
try:
    from xml.etree import cElementTree as ElementTree
except ImportError:
    from xml.etree import ElementTree

def parse_time_ns(tm):
    if tm.endswith('ns'):
        return float(tm[:-2])
    raise ValueError(tm)



## FiveTuple
class FiveTuple(object):
    ## class variables
    ## @var sourceAddress
    #  source address
    ## @var destinationAddress
    #  destination address
    ## @var protocol
    #  network protocol
    ## @var sourcePort
    #  source port
    ## @var destinationPort
    #  destination port
    ## @var __slots_
    #  class variable list
    __slots_ = ['sourceAddress', 'destinationAddress', 'protocol', 'sourcePort', 'destinationPort']
    def __init__(self, el):
        '''! The initializer.
        @param self The object pointer.
        @param el The element.
        '''
        self.sourceAddress = el.get('sourceAddress')
        self.destinationAddress = el.get('destinationAddress')
        self.sourcePort = int(el.get('sourcePort'))
        self.destinationPort = int(el.get('destinationPort'))
        self.protocol = int(el.get('protocol'))

## Histogram
class Histogram(object):
    ## class variables
    ## @var bins
    #  histogram bins
    ## @var __slots_
    #  class variable list
    __slots_ = 'bins', 'nbins', 'number_of_flows'
    def __init__(self, el=None):
        '''! The initializer.
        @param self The object pointer.
        @param el The element.
        '''
        self.bins = []
        if el is not None:
            #self.nbins = int(el.get('nBins'))
            for bin in el.findall('bin'):
                self.bins.append( (float(bin.get("start")), float(bin.get("width")), int(bin.get("count"))) )

## Flow
class Flow(object):
    ## class variables
    ## @var flowId
    #  delay ID
    ## @var delayMean
    #  mean delay
    ## @var packetLossRatio
    #  packet loss ratio
    ## @var rxBitrate
    #  receive bit rate
    ## @var txBitrate
    #  transmit bit rate
    ## @var packetSizeMean
    #  packet size mean
    ## @var probe_stats_unsorted
    #  unsirted probe stats
    ## @var hopCount
    #  hop count
    ## @var flowInterruptionsHistogram
    #  flow histogram
    ## @var rx_duration
    #  receive duration
    ## @var __slots_
    #  class variable list
    __slots_ = ['flowId', 'delayMean', 'packetLossRatio', 'rxBitrate', 'txBitrate',
                'fiveTuple', 'packetSizeMean', 'probe_stats_unsorted',
                'hopCount', 'flowInterruptionsHistogram', 'delayHistogram', 'rx_duration', 'fct']
    def __init__(self, flow_el):
        '''! The initializer.
        @param self The object pointer.
        @param flow_el The element.
        '''
        self.flowId = int(flow_el.get('flowId'))
        rxPackets = float(flow_el.get('rxPackets'))
        txPackets = float(flow_el.get('txPackets'))

        tx_duration = (parse_time_ns (flow_el.get('timeLastTxPacket')) - parse_time_ns(flow_el.get('timeFirstTxPacket')))*1e-9
        rx_duration = (parse_time_ns (flow_el.get('timeLastRxPacket')) - parse_time_ns(flow_el.get('timeFirstRxPacket')))*1e-9
        self.rx_duration = rx_duration
        self.fct = float(parse_time_ns (flow_el.get('timeLastRxPacket')) - parse_time_ns(flow_el.get('timeFirstTxPacket')))*1e-9
        self.probe_stats_unsorted = []
        if rxPackets:
            self.hopCount = float(flow_el.get('timesForwarded')) / rxPackets + 1
        else:
            self.hopCount = -1000
        if rxPackets:
            self.delayMean = float(flow_el.get('delaySum')[:-2]) / rxPackets * 1e-9
            self.packetSizeMean = float(flow_el.get('rxBytes')) / rxPackets
        else:
            self.delayMean = None
            self.packetSizeMean = None
        if rx_duration > 0:
            self.rxBitrate = float(flow_el.get('rxBytes'))*8 / rx_duration
        else:
            self.rxBitrate = None
        if tx_duration > 0:
            self.txBitrate = float(flow_el.get('txBytes'))*8 / tx_duration
        else:
            self.txBitrate = None
        lost = float(flow_el.get('lostPackets'))
        #print "rxBytes: %s; txPackets: %s; rxPackets: %s; lostPackets: %s" % (flow_el.get('rxBytes'), txPackets, rxPackets, lost)
        if rxPackets == 0:
            self.packetLossRatio = None
        else:
            self.packetLossRatio = (lost / (rxPackets + lost))

        interrupt_hist_elem = flow_el.find("flowInterruptionsHistogram")
        if interrupt_hist_elem is None:
            self.flowInterruptionsHistogram = None
        else:
            self.flowInterruptionsHistogram = Histogram(interrupt_hist_elem)

        flow_delay_hist = flow_el.find("delayHistogram")
        if flow_delay_hist:
            self.delayHistogram = flow_delay_hist
        else:
            self.delayHistogram = None

## ProbeFlowStats
class ProbeFlowStats(object):
    ## class variables
    ## @var packets
    #  network packets
    ## @var bytes
    #  bytes
    ## @var __slots_
    #  class variable list
    __slots_ = ['probeId', 'packets', 'bytes', 'delayFromFirstProbe']

## Simulation
class Simulation(object):
    ## class variables
    ## @var flows
    #  list of flows
    def __init__(self, simulation_el):
        '''! The initializer.
        @param self The object pointer.
        @param simulation_el The element.
        '''
        self.flows = []
        FlowClassifier_el, = simulation_el.findall("Ipv6FlowClassifier")
        flow_map = {}
        for flow_el in simulation_el.findall("FlowStats/Flow"):
            flow = Flow(flow_el)
            flow_map[flow.flowId] = flow
            self.flows.append(flow)
        for flow_cls in FlowClassifier_el.findall("Flow"):
            flowId = int(flow_cls.get('flowId'))
            flow_map[flowId].fiveTuple = FiveTuple(flow_cls)

        for probe_elem in simulation_el.findall("FlowProbes/FlowProbe"):
            probeId = int(probe_elem.get('index'))
            for stats in probe_elem.findall("FlowStats"):
                flowId = int(stats.get('flowId'))
                s = ProbeFlowStats()
                s.packets = int(stats.get('packets'))
                s.bytes = float(stats.get('bytes'))
                s.probeId = probeId
                if s.packets > 0:
                    s.delayFromFirstProbe =  parse_time_ns(stats.get('delayFromFirstProbeSum')) / float(s.packets)
                else:
                    s.delayFromFirstProbe = 0
                flow_map[flowId].probe_stats_unsorted.append(s)


def parse_xml(path):
    with open(path, encoding="utf-8") as file_obj:
        print("Reading XML file ", end=" ")

        sys.stdout.flush()
        level = 0
        sim_list = []
        for event, elem in ElementTree.iterparse(file_obj, events=("start", "end")):
            if event == "start":
                level += 1
            if event == "end":
                level -= 1
                if level == 0 and elem.tag == 'FlowMonitor':
                    print("Creating sim")
                    sim = Simulation(elem)
                    sim_list.append(sim)
                    elem.clear() # won't need this any more
                    sys.stdout.write(".")
                    sys.stdout.flush()
    print(" done.")
    return sim_list


def get_flow_results(sim):
    results = {
        'flows': {}
    } 
    total_fct = 0
    fct_list = []
    for flow in sim.flows:
        t: FiveTuple = flow.fiveTuple
        proto = {6: 'TCP', 17: 'UDP'} [t.protocol]
        print("FlowID: %i (%s %s/%s --> %s/%i)" % \
            (flow.flowId, proto, t.sourceAddress, t.sourcePort, t.destinationAddress, t.destinationPort))
        if flow.txBitrate is None:
            print("\tTX bitrate: None")
        else:
            print("\tTX bitrate: %.2f kbit/s" % (flow.txBitrate*1e-3,))
        if flow.rxBitrate is None:
            print("\tRX bitrate: None")
        else:
            print("\tRX bitrate: %.2f kbit/s" % (flow.rxBitrate*1e-3,))
        if flow.delayMean is None:
            print("\tMean Delay: None")
        else:
            print("\tMean Delay: %.2f ms" % (flow.delayMean*1e3,))
        if flow.packetLossRatio is None:
            print("\tPacket Loss Ratio: None")
        else:
            print("\tPacket Loss Ratio: %.2f %%" % (flow.packetLossRatio*100))
        if flow.fct is None:
            print("\tFCT: None")
        else:
            print(f"\tFCT: {flow.fct}")
        
        results['flows'][flow.flowId] = {
            'src_addr': t.sourceAddress,
            'dsr_addr': t.destinationAddress,
            'src_port': t.sourcePort,
            'dst_port': t.destinationPort,
            'protocol': t.protocol, 
            'tx_bit_rate': flow.txBitrate*1e-3, 
            'rx_bit_rate': flow.rxBitrate*1e-3,
            'mean_delay': flow.delayMean*1e3,
            "packet_loss_ratio": flow.packetLossRatio*100,
            'fct': flow.fct
        }
        total_fct += flow.fct
        fct_list.append(flow.fct)
    
    results['number_of_flows'] = len(sim.flows)
    results['mean_fct'] = total_fct/len(sim.flows)
    results['99-ile_fct'] = fct_list[int((len(fct_list) * 99) / 100)]
    results['99.9-ile_fct'] = fct_list[int((len(fct_list) * 999) / 1000)]
    print(f"Number of Flows: {len(sim.flows)}")
    print(f"AVG FCT: {total_fct/len(sim.flows)}")
    print("Flow 99-ile FCT: %.4f s" % (fct_list[int((len(fct_list) * 99) / 100)]))
    print("Flow 99.9-ile FCT: %.4f s" % (fct_list[int((len(fct_list) * 999) / 1000)]))

    return results

def main(path):
    sim_list = parse_xml(path)
    if len(sim_list) > 1: 
        raise Exception("Two simulations in one single flow monitor file.")
    
    results = get_flow_results(sim_list[0])
    result_path = os.path.join("/".join(path.split("/")[:-1]), "flows_results.json")
    with open(result_path, 'w') as result_file: 
        json.dump(results, result_file)


if __name__ == '__main__':
    main(sys.argv[1])
