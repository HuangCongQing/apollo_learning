################################################################################
#
# Copyright (c) 2017 Baidu.com, Inc. All Rights Reserved
#
################################################################################
"""
This module provides routing result parse and plot functions.

Authors: fuxiaoxin(fuxiaoxin@baidu.com)
"""

import sys
import itertools
import matplotlib.pyplot as plt
import debug_topo
import gen.topo_graph_pb2 as topo_graph_pb2
import plot_map

color_iter = itertools.cycle(['navy', 'c', 'cornflowerblue', 'gold',
                              'darkorange'])

def read_route(route_file_name):
    """Read route result text file"""
    fin = open(route_file_name)
    route = []
    for line in fin:
        lane = {}
        items = line.strip().split(',')
        lane['id'] = items[0]
        lane['is virtual'] = int(items[1])
        lane['start s'] = float(items[2])
        lane['end s'] = float(items[3])
        route.append(lane)
    return route


def plot_route(lanes, central_curve_dict):
    """Plot route result"""
    plt.close()
    plt.figure()
    for lane in lanes:
        lane_id = lane['id']
        if lane['is virtual']:
            color = 'red'
        else:
            color = 'green'
        debug_topo.plot_central_curve_with_s_range(central_curve_dict[lane_id],
                                                   lane['start s'],
                                                   lane['end s'],
                                                   color=color)
    plt.gca().set_aspect(1)
    plt.title('Routing result')
    plt.xlabel('x')
    plt.ylabel('y')
    plt.legend()

    plt.draw()


def print_help():
    """Print help information.

    Print help information of usage.

    Args:

    """
    print 'usage:'
    print '     python debug_route.py topo_file_path debug_route_file_path, then',
    print_help_command()


def print_help_command():
    """Print command help information.

    Print help information of command.

    Args:

    """
    print 'type in command: [q] [r]'
    print '         q               exit'
    print '         r               plot route result'
    print '         r_map           plot route result with map'


if __name__ == '__main__':
    if len(sys.argv) != 3:
        print_help()
        sys.exit(0)
    print 'Please wait for loading data...'

    map_path = '/home/caros/adu_data/map/base_map.bin'
    file_name = sys.argv[1]
    fin = open(file_name)
    graph = topo_graph_pb2.Graph()
    graph.ParseFromString(fin.read())
    central_curves = {}
    for nd in graph.node:
        central_curves[nd.lane_id] = nd.central_curve

    plt.ion()
    while 1:
        print_help_command()
        print 'cmd>',
        instruction = raw_input()
        argv = instruction.strip(' ').split(' ')
        if len(argv) == 1:
            if argv[0] == 'q':
                sys.exit(0)
            elif argv[0] == 'r':
                route = read_route(sys.argv[2])
                plot_route(route, central_curves)
            elif argv[0] == 'r_map':
                route = read_route(sys.argv[2])
                plot_route(route, central_curves)
                plot_map.draw_map(plt.gca(), map_path)
            else:
                print '[ERROR] wrong command'
            continue

        else:
            print '[ERROR] wrong arguments'
            continue
